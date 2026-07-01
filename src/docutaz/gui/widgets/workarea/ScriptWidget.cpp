#include "docutaz/gui/widgets/workarea/ScriptWidget.h"

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QPalette>
#include <QCompleter>
#include <QStringListModel>
#include <QTimer>
#include <QRegularExpression>
#include <QLabel>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qsciscintilla.h>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/domain/MongoShell.h"
#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/domain/MongoDatabase.h"
#include "docutaz/core/domain/MongoCollection.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/utils/QtUtils.h"

#include "docutaz/gui/widgets/workarea/IndicatorLabel.h"
#include "docutaz/gui/widgets/workarea/QueryWidget.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/Theme.h"
#include "docutaz/gui/ConnectionEnvironment.h"
#include "docutaz/gui/editors/JSLexer.h"
#include "docutaz/gui/editors/FindFrame.h"
#include "docutaz/gui/editors/PlainJavaScriptEditor.h"
#include "docutaz/gui/editors/MongoCompletionData.h"

namespace
{
    // Debounce window before a server-backed completion request is sent.
    const int kServerCompleteDebounceMs = 150;

    // Minimum visible height of the query editor, in text lines. A short (e.g.
    // one-line) query still reserves this much so the editor reads as its own
    // panel rather than blending into the surrounding chrome.
    const int kMinEditorLines = 3;

    // Split a token like "db.users.fi" into qualifier ("db.users") and the
    // partial word after the last dot ("fi"). No dot → empty qualifier.
    void splitToken(const QString &token, QString &qualifier, QString &partial)
    {
        const int dot = token.lastIndexOf('.');
        if (dot < 0) { qualifier.clear(); partial = token; return; }
        qualifier = token.left(dot);
        partial = token.mid(dot + 1);
    }

    // True when @p qualifier is exactly "db.<identifier>" — i.e. a single
    // collection segment, so collection methods are the right completion.
    bool isCollectionQualifier(const QString &qualifier)
    {
        if (!qualifier.startsWith(QLatin1String("db.")))
            return false;
        const QString coll = qualifier.mid(3);
        if (coll.isEmpty() || coll.contains('.'))
            return false;
        for (const QChar ch : coll)
            if (!(ch.isLetterOrNumber() || ch == '_' || ch == '$'))
                return false;
        return true;
    }

    // True when @p textBeforeDot ends with a getCollection("name") /
    // getCollection('name') call. The tokenizer stops at the call's ')', so a
    // member access like db.getCollection("user").<partial> loses its receiver;
    // this recovers it so collection methods still complete there.
    bool endsWithGetCollectionCall(const QString &textBeforeDot)
    {
        static const QRegularExpression re(
            QStringLiteral("getCollection\\s*\\(\\s*(?:\"[^\"]*\"|'[^']*')\\s*\\)\\s*$"));
        return re.match(textBeforeDot).hasMatch();
    }
}

namespace
{
    bool isStopChar(const QChar &ch, bool direction)
    {
        if (ch == '='  ||  ch == ';'  ||
            ch == '('  ||  ch == ')'  ||
            ch == '{'  ||  ch == '}'  ||
            ch == '-'  ||  ch == '/'  ||
            ch == '+'  ||  ch == '*'  ||
            ch == '\r' ||  ch == '\n' ||
            ch == ' ' ) {
                return true;
        }

        if (direction) { // right direction
            if (ch == '.')
                return true;
        }

        return false;
    }

    bool isForbiddenChar(const QChar &ch)
    {
        return ch == '\"' ||  ch == '\'';
    }
}

namespace Docutaz
{
    ScriptWidget::ScriptWidget(MongoShell *shell, QueryWidget *parent) :
        _shell(shell),
        _parent(parent),
        _textChanged(false),
        _disableTextAndCursorNotifications(false)
    {
        _queryText = new FindFrame(this);
        _topStatusBar = new TopStatusBar(_shell->server()->connectionRecord()->connectionName(),
                                         _shell->server()->connectionRecord()->getFullAddress(), "loading...",
                                         _shell->server()->connectionRecord()->environment());

        QVBoxLayout *layout = new QVBoxLayout;
        layout->setSpacing(0);
        layout->setContentsMargins(5, 1, 5, 5);
        layout->addWidget(_topStatusBar, 0, Qt::AlignTop);
        layout->addWidget(_queryText);
        setLayout(layout);

        // Query text widget
        configureQueryText();
        _queryText->sciScintilla()->setFocus();

        _queryText->sciScintilla()->installEventFilter(this);

        _completer = new QCompleter(this);
        _completer->setWidget(_queryText->sciScintilla());
        _completer->setCompletionMode(QCompleter::PopupCompletion);
        _completer->setCaseSensitivity(Qt::CaseInsensitive);
        _completer->setMaxVisibleItems(20);
        _completer->setWrapAround(false);
        _completer->popup()->setFont(GuiRegistry::instance().editorFont());
        VERIFY(connect(_completer, SIGNAL(activated(const QString &)), this, SLOT(onCompletionActivated(const QString&))));

        QStringListModel *model = new QStringListModel(_completer);
        _completer->setModel(model);

        // Server-backed completion is debounced: local suggestions show instantly
        // on every keystroke, but the request to the (shared) mongosh subprocess
        // is only sent once typing pauses.
        _serverCompleteTimer = new QTimer(this);
        _serverCompleteTimer->setSingleShot(true);
        _serverCompleteTimer->setInterval(kServerCompleteDebounceMs);
        VERIFY(connect(_serverCompleteTimer, SIGNAL(timeout()), this, SLOT(requestServerCompletion())));

        setText(QtUtils::toQString(shell->query()));
        setTextCursor(shell->cursor());
    }

    bool ScriptWidget::eventFilter(QObject *obj, QEvent *event)
    {
        if (obj == _queryText->sciScintilla()) {
            if (event->type() == QEvent::KeyPress) {
                QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter
                        || keyEvent->key() == Qt::Key_Tab) {
                    hideAutocompletion();
                    return false;
                }
            }
        }
        return QFrame::eventFilter(obj, event);
    }

    void ScriptWidget::setup(const MongoShellExecResult &execResult)
    {
        setCurrentDatabase(execResult.currentDatabase(), execResult.isCurrentDatabaseValid());
        setCurrentServer(execResult.currentServer(), execResult.isCurrentServerValid());
    }

    void ScriptWidget::setText(const QString &text)
    {
        _queryText->sciScintilla()->setText(text);
    }

    void ScriptWidget::setTextCursor(const CursorPosition &cursor)
    {
        if (cursor.isNull()) {
            _queryText->sciScintilla()->setCursorPosition(15, 1000);
            return;
        }

        int column = cursor.column();
        if (column < 0) {
            column = _queryText->sciScintilla()->text(cursor.line()).length() + column;
        }

        _queryText->sciScintilla()->setCursorPosition(cursor.line(), column);
    }

    QString ScriptWidget::text() const
    {
        return _queryText->sciScintilla()->text();
    }

    QString ScriptWidget::selectedText() const
    {
        return _queryText->sciScintilla()->selectedText();
    }

    void ScriptWidget::selectAll()
    {
        _queryText->sciScintilla()->selectAll();
    }

    void ScriptWidget::setScriptFocus()
    {
        _queryText->sciScintilla()->setFocus();
    }

    void ScriptWidget::setCurrentDatabase(const std::string &database, bool isValid)
    {
        _topStatusBar->setCurrentDatabase(database, isValid);
    }

    void ScriptWidget::setCurrentServer(const std::string &address, bool isValid)
    {
        _topStatusBar->setCurrentServer(address, isValid);
    }

    void ScriptWidget::popupCompletions(const QStringList &list, const QString &prefix)
    {
        if (list.isEmpty()) {
            hideAutocompletion();
            return;
        }

        // do not show single autocompletion which is identical to existing prefix
        // or if it identical to prefix + '('.
        if (list.count() == 1) {
            if (list.at(0) == prefix ||
                list.at(0) == (prefix + "(")) {
                hideAutocompletion();
                return;
            }
        }

        // update list of completions
        QStringListModel * model = static_cast<QStringListModel *>(_completer->model());
        model->setStringList(list);

        int currentLine = 0;
        int currentIndex = 0;
        _queryText->sciScintilla()->getCursorPosition(&currentLine, &currentIndex);
        int physicalLine = currentLine - _queryText->sciScintilla()->firstVisibleLine(); // "physical" line number in text editor (not logical)
        int lineIndexLeft = _currentAutoCompletionInfo.lineIndexLeft();

        QRect rect = _queryText->sciScintilla()->rect();
        rect.setWidth(550);
        rect.setHeight(editorHeight(physicalLine + 1));
        rect.moveLeft(charWidth() * lineIndexLeft
            + autocompletionBoxLeftPosition()
            + _queryText->sciScintilla()->lineNumberMarginWidth());

        _completer->complete(rect);
        _completer->popup()->setCurrentIndex(_completer->completionModel()->index(0, 0));
        DocutazScintilla* scin = static_cast<DocutazScintilla*>(_queryText->sciScintilla());
        scin->setIgnoreEnterKey(true);
        scin->setIgnoreTabKey(true);
    }

    // Server-backed completion reply. Async: by the time it arrives the user may
    // have typed on, so we re-sanitize and drop it if the live prefix no longer
    // matches (stale-guard). Otherwise we merge it with the instant local list.
    void ScriptWidget::showAutocompletion(const QStringList &list, const QString &prefix)
    {
        const AutoCompletionInfo live = sanitizeForAutocompletion();
        if (live.isEmpty() || live.text() != prefix)
            return; // stale — the cursor has moved past this request
        _currentAutoCompletionInfo = live;

        QStringList merged = _lastLocalCandidates;
        for (const QString &item : list)
            if (!merged.contains(item))
                merged += item;

        popupCompletions(merged, prefix);
    }

    void ScriptWidget::showAutocompletion()
    {
        const AutocompletionMode mode =
            AppRegistry::instance().settingsManager()->autocompletionMode();
        if (mode == AutocompleteNone) {
            hideAutocompletion();
            return;
        }

        _currentAutoCompletionInfo = sanitizeForAutocompletion();
        if (_currentAutoCompletionInfo.isEmpty()) {
            hideAutocompletion();
            return;
        }

        // Cancel any pending/in-flight server request for an earlier keystroke.
        _serverCompleteTimer->stop();
        _pendingServerPrefix.clear();

        const QString token = _currentAutoCompletionInfo.text();

        // 1. Instant, local suggestions (Tier 1 static + Tier 2 model).
        _lastLocalCandidates = localCandidates(token);
        popupCompletions(_lastLocalCandidates, token);

        // 2. Server-backed live collection names — only in full mode, only for the
        //    "db." context, and debounced so a typing burst sends at most one
        //    request. requestServerCompletion() also skips while a query runs.
        if (mode == AutocompleteAll) {
            QString qualifier, partial;
            splitToken(token, qualifier, partial);
            if (qualifier == QLatin1String("db") && !token.contains('(')) {
                _pendingServerPrefix = token;
                _serverCompleteTimer->start();
            }
        }
    }

    QStringList ScriptWidget::localCandidates(const QString &token) const
    {
        using namespace MongoCompletion;

        const AutocompletionMode mode =
            AppRegistry::instance().settingsManager()->autocompletionMode();
        // AutocompleteNoCollectionNames suppresses collection-name suggestions
        // (local and server alike); methods/operators/globals still show.
        const bool wantCollections = (mode != AutocompleteNoCollectionNames);

        QString qualifier, partial;
        splitToken(token, qualifier, partial);

        QStringList out;
        if (token.startsWith(QLatin1Char('.'))) {
            // Member access whose receiver the tokenizer truncated at a stop
            // char (e.g. the ')' in db.getCollection("x").<partial>). Recover the
            // common case so collection methods complete; never offer globals
            // here — a leading '.' is never a place for a bare keyword.
            const QString lineText =
                _queryText->sciScintilla()->text(_currentAutoCompletionInfo.line());
            const QString before = lineText.left(_currentAutoCompletionInfo.lineIndexLeft());
            if (endsWithGetCollectionCall(before)) {
                for (const QString &m : staticCandidates(Context::CollectionMember, partial))
                    out += QLatin1Char('.') + m;
            }
        } else if (qualifier.isEmpty()) {
            out += staticCandidates(Context::Global, partial);
        } else if (qualifier == QLatin1String("db")) {
            for (const QString &m : staticCandidates(Context::DbMember, partial))
                out += QLatin1String("db.") + m;
            if (wantCollections) {
                for (const QString &c : currentDbCollectionNames())
                    if (c.startsWith(partial, Qt::CaseInsensitive))
                        out += QLatin1String("db.") + c;
            }
        } else if (isCollectionQualifier(qualifier)) {
            for (const QString &m : staticCandidates(Context::CollectionMember, partial))
                out += qualifier + QLatin1Char('.') + m;
        }

        out.removeDuplicates();
        return out;
    }

    QStringList ScriptWidget::currentDbCollectionNames() const
    {
        QStringList names;
        MongoServer *server = _shell ? _shell->server() : nullptr;
        if (!server)
            return names;
        MongoDatabase *database = server->findDatabaseByName(_shell->dbname());
        if (!database)
            return names;
        for (MongoCollection *coll : database->collections())
            names += QtUtils::toQString(coll->name());
        return names;
    }

    void ScriptWidget::requestServerCompletion()
    {
        // Skip while a query is executing — completion must not queue behind a
        // long query on the shared mongosh subprocess.
        if (_parent && _parent->isExecuting())
            return;
        if (_pendingServerPrefix.isEmpty())
            return;
        _shell->autocomplete(QtUtils::toStdString(_pendingServerPrefix));
    }

    void ScriptWidget::hideAutocompletion()
    {
        _completer->popup()->hide();
        DocutazScintilla *scin = static_cast<DocutazScintilla*>(_queryText->sciScintilla());
        scin->setIgnoreEnterKey(false);
        scin->setIgnoreTabKey(false);
    }

    void ScriptWidget::disableFixedHeight() const
    {
        _queryText->setMinimumSize(0, 0);
        _queryText->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        _queryText->sciScintilla()->setMinimumSize(0, 0);
        _queryText->sciScintilla()->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        _queryText->sciScintilla()->setFocus();
    }

    void ScriptWidget::ui_queryLinesCountChanged()
    {
        // Set fixed size only if output widget is docked
        if (_parent->outputWindowDocked())
        {
            int lines = qMax(_queryText->sciScintilla()->lines(), kMinEditorLines);
            int editorTotalHeight = editorHeight(lines);

            int maxHeight = editorHeight(18);
            if (editorTotalHeight > maxHeight) {
                editorTotalHeight = maxHeight;
            }
            // Hide & Show solves problem of UI blinking
            _queryText->hide();
            _queryText->setFixedHeight(editorTotalHeight);
            _queryText->sciScintilla()->setFixedHeight(editorTotalHeight);
            _queryText->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
            _queryText->setMaximumHeight(editorTotalHeight + FindFrame::HeightFindPanel);
            _queryText->sciScintilla()->setFocus();
            _queryText->show();
        }
    }

    void ScriptWidget::onTextChanged()
    {
        emit textChanged();
        if (!_disableTextAndCursorNotifications)
            _textChanged = true;
    }

    void ScriptWidget::onCursorPositionChanged(int line, int index)
    {
        if (!_disableTextAndCursorNotifications && _textChanged) {
            showAutocompletion();
            _textChanged = false;
        }
    }

    void ScriptWidget::onCompletionActivated(const QString &text)
    {
        int row = _currentAutoCompletionInfo.line();
        int colLeft = _currentAutoCompletionInfo.lineIndexLeft();
        int colRight = _currentAutoCompletionInfo.lineIndexRight();
        QString line = _queryText->sciScintilla()->text(row);

        int selectionIndexRight = colRight + 1;

        // overwrite open parenthesis, if it already exists in text
        if (text.endsWith('(')) {
            if (line.length() > colRight + 1) {
                if (line.at(colRight + 1) == '(') {
                    ++selectionIndexRight;
                }
            }
        }

        _disableTextAndCursorNotifications = true;

        _queryText->sciScintilla()->setSelection(row, colLeft, row, selectionIndexRight);
        _queryText->sciScintilla()->replaceSelectedText(text);

        _disableTextAndCursorNotifications = false;
    }

    /*
    ** Configure QsciScintilla query widget
    */
    void ScriptWidget::configureQueryText()
    {
        QsciLexerJavaScript *javaScriptLexer = new JSLexer(this);
        javaScriptLexer->setFont(GuiRegistry::instance().editorFont());
        int height = editorHeight(kMinEditorLines);
        _queryText->sciScintilla()->setMinimumHeight(height);
        _queryText->sciScintilla()->setFixedHeight(height);
        _queryText->sciScintilla()->setAppropriateBraceMatching();
        _queryText->sciScintilla()->setFont(GuiRegistry::instance().editorFont());
        _queryText->sciScintilla()->setPaper(Theme::current().editorCanvas);
        _queryText->sciScintilla()->setLexer(javaScriptLexer);
        // setLexer ran STYLECLEARALL, which resets the margin/gutter background;
        // re-assert the editor theme colours (margins set last) so the gutter
        // doesn't render as a light strip in dark mode.
        _queryText->sciScintilla()->applyThemeColors();

        // Frame backgrounds/borders follow the theme; refresh them on a live
        // colour-scheme change (the editor canvas/syntax refresh themselves).
        applyTheme();
        VERIFY(connect(Theme::Notifier::instance(), &Theme::Notifier::changed,
                       this, &ScriptWidget::applyTheme));
        VERIFY(connect(_queryText->sciScintilla(), SIGNAL(linesChanged()), SLOT(ui_queryLinesCountChanged())));
        VERIFY(connect(_queryText->sciScintilla(), SIGNAL(textChanged()), SLOT(onTextChanged())));
        VERIFY(connect(_queryText->sciScintilla(), SIGNAL(cursorPositionChanged(int, int)), SLOT(onCursorPositionChanged(int, int))));
    }

    void ScriptWidget::reapplyEditorFont()
    {
        const QFont &f = GuiRegistry::instance().editorFont();
        auto *sci = _queryText->sciScintilla();
        sci->setFont(f);
        if (QsciLexer *lex = sci->lexer())
            lex->setFont(f);   // applies to every lexer style
        if (_completer && _completer->popup())
            _completer->popup()->setFont(f);
    }

    void ScriptWidget::applyTheme()
    {
        // Top strip blends with the window chrome; the editor frame carries the
        // canvas fill and a thin themed border. Read the colour from Theme (not
        // palette()): on a live switch this runs synchronously from the theme
        // notifier, before the async ApplicationPaletteChange reaches the widget,
        // so palette() would still be the previous scheme — leaving the breadcrumb
        // strip in the old colour.
        const QString bg = Theme::current().window.name();
        setStyleSheet(QString("QFrame {background-color: %1; border: 0px solid %2;"
                      "border-radius: 0px; margin: 0px; padding: 0px;}")
                      .arg(bg, Theme::current().mid.name()));

        _queryText->sciScintilla()->setStyleSheet(
            QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 4px;"
                    " margin: 0px; padding: 0px;}")
                .arg(Theme::current().editorCanvas.name(), Theme::current().mid.name()));
    }

    /**
     * @brief Calculates line height of text editor
     */
    int ScriptWidget::lineHeight() const
    {  
        return _queryText->sciScintilla()->textHeight(-1);
    }

    /**
     * @brief Calculates char width of text editor
     */
    int ScriptWidget::charWidth()
    {
        QFontMetrics m(_queryText->sciScintilla()->font());
        return m.averageCharWidth();
    }

    int ScriptWidget::autocompletionBoxLeftPosition()
    {
    #if defined(Q_OS_MAC)
        return -1;
    #endif
        // for Linux and Windows it is the same for now
        return 1;
    }

    /**
     * @brief Calculates preferable editor height for specified number of lines
     */
    int ScriptWidget::editorHeight(int lines) const
    {
        return lines * lineHeight() + 8;
    }

    AutoCompletionInfo ScriptWidget::sanitizeForAutocompletion()
    {
        int row = 0;
        int col = 0;
        _queryText->sciScintilla()->getCursorPosition(&row, &col);
        QString line = _queryText->sciScintilla()->text(row);

        int leftStop = -1;
        for (int i = col - 1; i >= 0; --i) {
            const QChar ch = line.at(i);

            if (isForbiddenChar(ch))
                return AutoCompletionInfo();

            if (isStopChar(ch, false)) {
                leftStop = i;
                break;
            }
        }

        int rightStop = line.length() + 1;
        for (int i = col; i < line.length(); ++i) {
            const QChar ch = line.at(i);

            if (isForbiddenChar(ch))
                return AutoCompletionInfo();

            if (isStopChar(ch, true)) {
                rightStop = i;
                break;
            }
        }

        leftStop = leftStop + 1;
        rightStop = rightStop - 1;
        //int len = ondemand ? col - leftStop : rightStop - leftStop + 1;
        int len = col - leftStop;

        QString final = line.mid(leftStop, len);
        return AutoCompletionInfo(final, row, leftStop, rightStop);
    }

    TopStatusBar::TopStatusBar(const std::string &connectionName, const std::string &serverName,
                               const std::string &dbName, const std::string &environment)
    {
        setContentsMargins(0, 0, 0, 0);
        _textColor = palette().text().color().lighter(200);

        _currentConnectionLabel = new Indicator(GuiRegistry::instance().connectIcon(), 
            QString("<font color='%1'>%2</font>").arg(_textColor.name()).arg(connectionName.c_str()));
        _currentConnectionLabel->setDisabled(true);
        
        _currentServerLabel = new Indicator(GuiRegistry::instance().serverIcon(), 
            QString("<font color='%1'>%2</font>").arg(_textColor.name()).arg(serverName.c_str()));
        _currentServerLabel->setDisabled(true);

        _currentDatabaseLabel = new Indicator(GuiRegistry::instance().databaseIcon(), 
            QString("<font color='%1'>%2</font>").arg(_textColor.name()).arg(dbName.c_str()));
        _currentDatabaseLabel->setDisabled(true);
        
        QHBoxLayout *topLayout = new QHBoxLayout;
        topLayout->setSpacing(0);
    #if defined(Q_OS_MAC)
        topLayout->setContentsMargins(2, 3, 2, 3);
    #else
        topLayout->setContentsMargins(2, 7, 2, 3);
    #endif

        // Environment banner: for a tagged connection, lead the breadcrumb with a
        // solid colour accent strip + an uppercased env-name pill (PRODUCTION,
        // STAGING, …) in the environment colour, so it is unmistakable which
        // environment this editor is pointed at — right above where destructive
        // queries get typed. Untagged connections add nothing (neutral strip).
        // Both use #objectName selectors so the parent ScriptWidget's `QFrame`
        // stylesheet (set in applyTheme) can't override them, and so a live
        // theme switch leaves the banner untouched.
        const QColor envColor = ConnectionEnvironment::color(environment);
        if (envColor.isValid()) {
            QFrame *accent = new QFrame;
            accent->setObjectName("envAccentStrip");
            accent->setFixedWidth(3);
            accent->setStyleSheet(
                QString("QFrame#envAccentStrip { background-color: %1; border: none; }")
                    .arg(envColor.name()));
            topLayout->addWidget(accent);
            topLayout->addSpacing(6);

            QLabel *pill = new QLabel(ConnectionEnvironment::displayName(environment).toUpper());
            pill->setObjectName("envPill");
            pill->setStyleSheet(QString(
                "QLabel#envPill { background-color: %1; color: white; font-weight: bold;"
                " border-radius: 3px; padding: 1px 6px; }").arg(envColor.name()));
            topLayout->addWidget(pill, 0, Qt::AlignVCenter);
            topLayout->addSpacing(6);
        }

        topLayout->addWidget(_currentConnectionLabel, 0, Qt::AlignLeft);
        topLayout->addWidget(_currentServerLabel, 0, Qt::AlignLeft);
        topLayout->addWidget(_currentDatabaseLabel, 0, Qt::AlignLeft);
        topLayout->addStretch(1);

        setLayout(topLayout);
    }

    void TopStatusBar::setCurrentDatabase(const std::string &database, bool isValid)
    {
        QString color = isValid ? _textColor.name() : "red";

        QString text = QString("<font color='%1'>%2</font>")
                .arg(color)
                .arg(database.c_str());

        _currentDatabaseLabel->setText(text);
    }

    void TopStatusBar::setCurrentServer(const std::string &address, bool isValid)
    {
        QString color = isValid ? _textColor.name() : "red";

        QString text = QString("<font color='%1'>%2</font>")
                .arg(color)
                .arg(detail::prepareServerAddress(address).c_str());

        _currentServerLabel->setText(text);
    }
}
