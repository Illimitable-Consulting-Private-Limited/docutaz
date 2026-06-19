#include "docutaz/gui/editors/PlainJavaScriptEditor.h"

#include <QPainter>
#include <QApplication>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QMenu>
#include <QAction>
#include <algorithm>
#include <cctype>
#include <vector>
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/editors/JsBeautifier.h"
#include "docutaz/core/utils/QtUtils.h"

namespace
{
    // Bracket-pair colorization: container-indicator numbers (the app uses no
    // other indicators) cycling through kBpcLevels colours, and a size cap above
    // which colouring is skipped to keep typing responsive on huge buffers.
    constexpr int kBpcFirstIndicator = 20;
    constexpr int kBpcLevels = 3;
    // Indicator (just past the bracket cycle) used to highlight mongo
    // $-operators and field paths: $match, $group, "$amount", ...
    constexpr int kDollarIndicator = kBpcFirstIndicator + kBpcLevels; // 23
    // Red squiggle drawn under structural syntax errors (unbalanced brackets,
    // a missing comma between literals). One past the $-operator indicator.
    constexpr int kErrorIndicator = kDollarIndicator + 1;             // 24
    constexpr int kBpcMaxBytes = 200000;

    // Whether a '/' here starts a regex literal rather than a division, inferred
    // from the previous significant character (coarse but adequate: it only
    // affects whether brackets inside a regex are counted).
    bool bpcRegexAllowed(char prevSig)
    {
        if (prevSig == 0) return true;
        if (std::isalnum((unsigned char)prevSig) || prevSig == '_' || prevSig == '$') return false;
        if (prevSig == ')' || prevSig == ']' || prevSig == '}') return false;
        if (prevSig == '"' || prevSig == '\'' || prevSig == '`' || prevSig == '.') return false;
        return true;
    }

    /**
    * @brief Returns the number of digits in an 32-bit integer
    * http://stackoverflow.com/questions/1489830/efficient-way-to-determine-number-of-digits-in-an-integer
    */
    int getNumberOfDigits(int x)
    {
        if (x < 0) return getNumberOfDigits(-x) + 1;

        if (x >= 10000) {
            if (x >= 10000000) {
                if (x >= 100000000) {
                    if (x >= 1000000000)
                        return 10;
                    return 9;
                }
                return 8;
            }
            if (x >= 100000) {
                if (x >= 1000000)
                    return 7;
                return 6;
            }
            return 5;
        }
        if (x >= 100) {
            if (x >= 1000)
                return 4;
            return 3;
        }
        if (x >= 10)
            return 2;
        return 1;
    }
}

namespace Docutaz
{
    const QColor DocutazScintilla::marginsBackgroundColor = QColor(73, 76, 78);
    const QColor DocutazScintilla::caretForegroundColor = QColor("#FFFFFF");
    const QColor DocutazScintilla::matchedBraceForegroundColor = QColor("#FF8861");

    DocutazScintilla::DocutazScintilla(QWidget *parent) : QsciScintilla(parent),
        _ignoreEnterKey(false),
        _ignoreTabKey(false),
        _lineNumberDigitWidth(0),
        _lineNumberMarginWidth(0)
    {
        setAutoIndent(true);
        setIndentationsUseTabs(false);
        setIndentationWidth(indentationWidth);
        setUtf8(true);
        setMarginWidth(1, 0);
        setCaretForegroundColor(caretForegroundColor);
        setMatchedBraceForegroundColor(matchedBraceForegroundColor); //1AB0A6
        setMatchedBraceBackgroundColor(marginsBackgroundColor);
        setContentsMargins(0, 0, 0, 0);
        setViewportMargins(3, 3, 3, 3);
        QFont ourFont = GuiRegistry::instance().font();
        setMarginsFont(ourFont);
        setMarginLineNumbers(0, true);
        setMarginsBackgroundColor(QColor(53, 56, 58));
        setMarginsForegroundColor(QColor(173, 176, 178));

        SendScintilla(QsciScintilla::SCI_STYLESETFONT, 1, ourFont.family().data());
        SendScintilla(QsciScintilla::SCI_SETHSCROLLBAR, 0);

        setWrapMode((QsciScintilla::WrapMode)QsciScintilla::SC_WRAP_NONE);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        // ---- Editor experience: folding, indentation guides, current line ----
        // Code folding in the dedicated fold margin (index 2). The JavaScript
        // lexer supplies the fold points and QScintilla makes the margin
        // clickable to collapse/expand. Enabling it here (before the lexer is
        // attached) is safe: setFolding sets the master "fold" document property,
        // which the lexer's refreshProperties() leaves untouched.
        setFolding(QsciScintilla::BoxedTreeFoldStyle, 2);
        setFoldMarginColors(QColor(53, 56, 58), QColor(53, 56, 58));
        // Render the fold tree/markers as light glyphs on the dark margin: pass
        // the symbol fill (back) the margin colour so only the light outline and
        // +/- glyph (fore) show. Scintilla wants colours packed as 0xBBGGRR.
        auto sciColor = [](const QColor &c) -> long {
            return (long)((c.blue() << 16) | (c.green() << 8) | c.red());
        };
        for (int marker = SC_MARKNUM_FOLDEREND; marker <= SC_MARKNUM_FOLDEROPEN; ++marker) {
            SendScintilla(SCI_MARKERSETFORE, (unsigned long)marker, sciColor(QColor(173, 176, 178)));
            SendScintilla(SCI_MARKERSETBACK, (unsigned long)marker, sciColor(QColor(53, 56, 58)));
        }

        // Faint vertical indentation guides.
        setIndentationGuides(true);
        setIndentationGuidesForegroundColor(QColor(99, 102, 104));
        setIndentationGuidesBackgroundColor(QColor(73, 76, 78));

        // Subtly highlight the line the caret is on (white at low alpha so it
        // works over the dark editor background without washing out the text).
        setCaretLineVisible(true);
        setCaretLineBackgroundColor(QColor(255, 255, 255, 18));

        // Bracket-pair colorization: each bracket glyph is tinted by its nesting
        // depth via an INDIC_TEXTFORE indicator (which overrides the lexer's
        // operator colour just on that character). The colours cycle by depth.
        const QColor bpcColors[kBpcLevels] = {
            QColor("#179FFF"),  // depth 0 - blue
            QColor("#FFD700"),  // depth 1 - gold
            QColor("#DA70D6"),  // depth 2 - orchid
        };
        for (int i = 0; i < kBpcLevels; ++i) {
            SendScintilla(SCI_INDICSETSTYLE, (unsigned long)(kBpcFirstIndicator + i), (long)INDIC_TEXTFORE);
            SendScintilla(SCI_INDICSETFORE, (unsigned long)(kBpcFirstIndicator + i), sciColor(bpcColors[i]));
        }
        // mongo $-operators / field paths get their own colour (magenta), set
        // apart from the orange mongo keywords and the green strings they live in.
        SendScintilla(SCI_INDICSETSTYLE, (unsigned long)kDollarIndicator, (long)INDIC_TEXTFORE);
        SendScintilla(SCI_INDICSETFORE, (unsigned long)kDollarIndicator, sciColor(QColor("#FF79C6")));

        // Syntax errors: a red squiggle under the offending character. The
        // message is shown on hover (see event()). Drawn under the text so it
        // never hides the glyph.
        SendScintilla(SCI_INDICSETSTYLE, (unsigned long)kErrorIndicator, (long)INDIC_SQUIGGLE);
        SendScintilla(SCI_INDICSETFORE, (unsigned long)kErrorIndicator, sciColor(QColor("#FF3B30")));
        SendScintilla(SCI_INDICSETUNDER, (unsigned long)kErrorIndicator, (long)true);

        VERIFY(connect(this, SIGNAL(textChanged()), this, SLOT(updateSyntaxIndicators())));

        // Cache width of one digit
#ifdef Q_OS_WIN
        _lineNumberDigitWidth = rowNumberWidth;
#else
        _lineNumberDigitWidth = textWidth(STYLE_LINENUMBER, "0");
#endif
        updateLineNumbersMarginWidth();

        setLineNumbers(AppRegistry::instance().settingsManager()->lineNumbers());
        setUtf8(true);
        VERIFY(connect(this, SIGNAL(linesChanged()), this, SLOT(updateLineNumbersMarginWidth())));
    }

    int DocutazScintilla::lineNumberMarginWidth() const
    {
        return marginWidth(0);
    }

    int DocutazScintilla::textWidth(int style, const QString &text)
    {
        const char *byteArray = (text.toUtf8()).constData();
        return SendScintilla(SCI_TEXTWIDTH, style, byteArray);
    }

    void DocutazScintilla::wheelEvent(QWheelEvent *e)
    {
        if (this->isActiveWindow()) {
            QsciScintilla::wheelEvent(e);
        }
        else {
            qApp->sendEvent(parentWidget(), e);
            e->accept();
        }
    }

    void DocutazScintilla::setLineNumbers(bool displayNumbers)
    {
        if (displayNumbers) {
            setMarginWidth(0, _lineNumberMarginWidth);
        }
        else {
            setMarginWidth(0, 0);
        }
    }

    void DocutazScintilla::toggleLineNumbers()
    {
        setLineNumbers(!lineNumberMarginWidth());
    }

    void DocutazScintilla::keyPressEvent(QKeyEvent *keyEvent)
    {
        if (_ignoreEnterKey) {
            if (keyEvent->key() == Qt::Key_Return) {
                keyEvent->ignore();
                _ignoreEnterKey = false;
                return;
            }
        }

        if (_ignoreTabKey) {
            if (keyEvent->key() == Qt::Key_Tab) {
                keyEvent->ignore();
                _ignoreTabKey = false;
                return;
            }
        }

        if (keyEvent->key() == Qt::Key_F11) {
            keyEvent->ignore();
            toggleLineNumbers();
            return;
        }

        if ((keyEvent->modifiers() & Qt::ControlModifier) &&
            (keyEvent->modifiers() & Qt::ShiftModifier) &&
            keyEvent->key() == Qt::Key_F) {
            keyEvent->accept();
            formatCode();
            return;
        }

        if (handleAutoClose(keyEvent))
            return;

        if (((keyEvent->modifiers() & Qt::ControlModifier) &&
            (keyEvent->key() == Qt::Key_F4 || keyEvent->key() == Qt::Key_W ||
             keyEvent->key() == Qt::Key_T || keyEvent->key() == Qt::Key_Space ||
             keyEvent->key() == Qt::Key_F || keyEvent->key() == Qt::Key_Slash))
            || keyEvent->key() == Qt::Key_Escape /*|| keyEvent->key() == Qt::Key_Return*/
            || ((keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->modifiers() & Qt::AltModifier) && keyEvent->key() == Qt::Key_Left)
            || ((keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->modifiers() & Qt::AltModifier) && keyEvent->key() == Qt::Key_Right)
            || ((keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->modifiers() & Qt::ShiftModifier) && keyEvent->key() == Qt::Key_C)
           )
        {
            keyEvent->ignore();
        }
        else
        {
            BaseClass::keyPressEvent(keyEvent);
        }
    }

    void DocutazScintilla::updateLineNumbersMarginWidth()
    {
        int numberOfDigits = getNumberOfDigits(lines());
        _lineNumberMarginWidth = numberOfDigits * _lineNumberDigitWidth + rowNumberWidth;

        // If line numbers margin already displayed, update its width
        if (lineNumberMarginWidth()) {
            setMarginWidth(0, _lineNumberMarginWidth);
        }
    }

    void DocutazScintilla::contextMenuEvent(QContextMenuEvent *e)
    {
        // QsciScintilla's built-in context menu cannot be extended, so build a
        // standard edit menu and append the Format action.
        QMenu menu(this);

        QAction *undoAct = menu.addAction(tr("&Undo"));
        undoAct->setShortcut(QKeySequence::Undo);
        undoAct->setEnabled(isUndoAvailable());
        VERIFY(connect(undoAct, SIGNAL(triggered()), this, SLOT(undo())));

        QAction *redoAct = menu.addAction(tr("&Redo"));
        redoAct->setShortcut(QKeySequence::Redo);
        redoAct->setEnabled(isRedoAvailable());
        VERIFY(connect(redoAct, SIGNAL(triggered()), this, SLOT(redo())));

        menu.addSeparator();

        QAction *cutAct = menu.addAction(tr("Cu&t"));
        cutAct->setShortcut(QKeySequence::Cut);
        cutAct->setEnabled(hasSelectedText());
        VERIFY(connect(cutAct, SIGNAL(triggered()), this, SLOT(cut())));

        QAction *copyAct = menu.addAction(tr("&Copy"));
        copyAct->setShortcut(QKeySequence::Copy);
        copyAct->setEnabled(hasSelectedText());
        VERIFY(connect(copyAct, SIGNAL(triggered()), this, SLOT(copy())));

        QAction *pasteAct = menu.addAction(tr("&Paste"));
        pasteAct->setShortcut(QKeySequence::Paste);
        VERIFY(connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste())));

        menu.addSeparator();

        QAction *formatAct = menu.addAction(tr("&Format Code"));
        formatAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
        formatAct->setEnabled(!text().isEmpty());
        VERIFY(connect(formatAct, SIGNAL(triggered()), this, SLOT(formatCode())));

        menu.addSeparator();

        QAction *selectAllAct = menu.addAction(tr("Select &All"));
        selectAllAct->setShortcut(QKeySequence::SelectAll);
        VERIFY(connect(selectAllAct, SIGNAL(triggered()), this, SLOT(selectAll())));

        menu.exec(e->globalPos());
    }

    void DocutazScintilla::formatCode()
    {
        const bool hasSel = hasSelectedText();
        const std::string src =
            QtUtils::toStdString(hasSel ? selectedText() : text());
        if (src.empty())
            return;

        const std::string out = JsBeautifier::format(src, indentationWidth);
        if (out == src)   // nothing to do, or the beautifier safely bailed out
            return;

        const QString formatted = QtUtils::toQString(out);
        if (hasSel) {
            replaceSelectedText(formatted);
        } else {
            int line = 0, index = 0;
            getCursorPosition(&line, &index);
            setText(formatted);
            // Best-effort: keep the caret on roughly the same line.
            setCursorPosition(std::min(line, lines() - 1), 0);
        }
    }

    QChar DocutazScintilla::autoCloseChar(QChar open)
    {
        switch (open.unicode()) {
        case '(': return QChar(')');
        case '[': return QChar(']');
        case '{': return QChar('}');
        case '"': return QChar('"');
        case '\'': return QChar('\'');
        case '`': return QChar('`');
        default: return QChar();
        }
    }

    bool DocutazScintilla::handleAutoClose(QKeyEvent *e)
    {
        // Backspace inside a freshly inserted, still-empty pair removes both
        // characters at once (e.g. the caret between "()" deletes "()").
        if (e->key() == Qt::Key_Backspace && !hasSelectedText()) {
            const long pos = SendScintilla(SCI_GETCURRENTPOS);
            const long len = SendScintilla(SCI_GETLENGTH);
            if (pos > 0 && pos < len) {
                const QChar before((ushort)(uchar)SendScintilla(SCI_GETCHARAT, (unsigned long)(pos - 1)));
                const QChar after((ushort)(uchar)SendScintilla(SCI_GETCHARAT, (unsigned long)pos));
                if (!before.isNull() && autoCloseChar(before) == after) {
                    SendScintilla(SCI_DELETERANGE, (unsigned long)(pos - 1), (long)2);
                    e->accept();
                    return true;
                }
            }
            return false;
        }

        const QString t = e->text();
        if (t.size() != 1)
            return false;
        const QChar ch = t.at(0);
        const bool opener = (ch == '(' || ch == '[' || ch == '{');
        const bool closer = (ch == ')' || ch == ']' || ch == '}');
        const bool quote  = (ch == '"' || ch == '\'' || ch == '`');
        if (!opener && !closer && !quote)
            return false;

        // Wrap the current selection with the typed bracket/quote pair, keeping
        // the original (now wrapped) text selected.
        if ((opener || quote) && hasSelectedText()) {
            const QChar close = autoCloseChar(ch);
            const long selStart = SendScintilla(SCI_GETSELECTIONSTART);
            const QString sel = selectedText();
            const int innerBytes = sel.toUtf8().size();
            replaceSelectedText(QString(ch) + sel + QString(close));
            SendScintilla(SCI_SETSELECTIONSTART, (unsigned long)(selStart + 1));
            SendScintilla(SCI_SETSELECTIONEND, (unsigned long)(selStart + 1 + innerBytes));
            e->accept();
            return true;
        }

        int line = 0, idx = 0;
        getCursorPosition(&line, &idx);
        const QString lineText = text(line);
        const QChar nextCh = (idx < lineText.length()) ? lineText.at(idx) : QChar();
        const QChar prevCh = (idx > 0) ? lineText.at(idx - 1) : QChar();

        // Type over an existing closer/quote instead of inserting a duplicate.
        if ((closer || quote) && nextCh == ch) {
            setCursorPosition(line, idx + 1);
            e->accept();
            return true;
        }
        if (closer)
            return false; // a plain closing bracket: insert normally

        // Only auto-close when the caret sits at a sensible boundary, so we
        // never split an existing token (e.g. typing '(' just before "foo").
        const bool atBoundary = nextCh.isNull() || nextCh.isSpace() ||
            nextCh == ')' || nextCh == ']' || nextCh == '}' ||
            nextCh == ',' || nextCh == ';' || nextCh == ':';
        if (!atBoundary)
            return false;
        // Don't auto-close a quote that is really an apostrophe / sits against a
        // word (e.g. inside an identifier or right after one).
        if (quote && (prevCh.isLetterOrNumber() || prevCh == ch))
            return false;

        const QChar close = autoCloseChar(ch);
        insertAt(QString(ch) + QString(close), line, idx);
        setCursorPosition(line, idx + 1);
        e->accept();
        return true;
    }

    void DocutazScintilla::updateSyntaxIndicators()
    {
        // Scintilla positions are byte offsets into the UTF-8 buffer; brackets
        // and $-tokens are ASCII, so iterating the UTF-8 bytes keeps the
        // positions exact.
        const QByteArray bytes = text().toUtf8();
        const int n = bytes.size();

        // Clear the previous bracket + $-operator + error indicators over the
        // whole document first (indices kBpcFirstIndicator .. kErrorIndicator).
        for (int ind = kBpcFirstIndicator; ind <= kErrorIndicator; ++ind) {
            SendScintilla(SCI_SETINDICATORCURRENT, (unsigned long)ind);
            SendScintilla(SCI_INDICATORCLEARRANGE, (unsigned long)0, (long)n);
        }
        _syntaxErrors.clear();
        if (n == 0 || n > kBpcMaxBytes)
            return;

        const char *s = bytes.constData();
        auto isDollarChar = [](char ch) { return std::isalnum((unsigned char)ch) || ch == '_'; };
        auto paintBracket = [&](int pos, int depth) {
            const int ind = kBpcFirstIndicator + (depth % kBpcLevels);
            SendScintilla(SCI_SETINDICATORCURRENT, (unsigned long)ind);
            SendScintilla(SCI_INDICATORFILLRANGE, (unsigned long)pos, (long)1);
        };
        auto paintDollar = [&](int pos, int len) {
            SendScintilla(SCI_SETINDICATORCURRENT, (unsigned long)kDollarIndicator);
            SendScintilla(SCI_INDICATORFILLRANGE, (unsigned long)pos, (long)len);
        };
        auto addError = [&](int pos, int len, const QString &msg) {
            _syntaxErrors.push_back({pos, len, msg});
        };

        // Bracket stack used for the structural checks. Each open bracket records
        // its byte position (for the "unclosed" report) and its kind, which tells
        // the missing-comma check whether the container is a comma-separated list
        // ('[' array literal, '{' object literal, '(' call/group) as opposed to
        // member indexing or a statement block (where adjacency is legal).
        enum Kind { Paren, ArrayLit, Index, ObjLit, Block };
        struct Frame { char open; int pos; Kind kind; };
        std::vector<Frame> stack;
        auto closerFor = [](char open) -> char {
            return open == '(' ? ')' : open == '[' ? ']' : '}';
        };

        char prevSig = 0;
        int i = 0;
        while (i < n) {
            const char c = s[i];
            // line / block comments
            if (c == '/' && i + 1 < n && s[i + 1] == '/') { i += 2; while (i < n && s[i] != '\n') i++; continue; }
            if (c == '/' && i + 1 < n && s[i + 1] == '*') { i += 2; while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) i++; i = std::min(n, i + 2); continue; }
            // regex literal
            if (c == '/' && bpcRegexAllowed(prevSig)) {
                i++; bool inClass = false;
                while (i < n) {
                    const char d = s[i];
                    if (d == '\\') { i += 2; continue; }
                    if (d == '[') inClass = true;
                    else if (d == ']') inClass = false;
                    else if (d == '/' && !inClass) { i++; break; }
                    else if (d == '\n') break;
                    i++;
                }
                while (i < n && std::isalpha((unsigned char)s[i])) i++;
                prevSig = '/'; continue;
            }
            // strings (single / double / template). A string that opens with '$'
            // is a quoted operator key or an aggregation field path ("$amount"):
            // highlight the $-token, then skip the rest of the string.
            if (c == '"' || c == '\'' || c == '`') {
                const char q = c; i++;
                if (i < n && s[i] == '$') {
                    const int ds = i; i++;
                    while (i < n && isDollarChar(s[i])) i++;
                    paintDollar(ds, i - ds);
                }
                while (i < n) { if (s[i] == '\\') { i += 2; continue; } if (s[i] == q) { i++; break; } i++; }
                prevSig = q; continue;
            }
            // bare $-operator / field key, e.g. { $match: ... }, $sum
            if (c == '$') {
                const int ds = i; i++;
                while (i < n && isDollarChar(s[i])) i++;
                paintDollar(ds, i - ds);
                prevSig = s[i - 1];
                continue;
            }
            // opening brackets
            if (c == '(' || c == '[' || c == '{') {
                // Missing-comma check: an object/array literal opening right after
                // a closed literal ('}{' or ']{') inside a comma-separated list
                // is a missing separator — the exact "forgot a comma between
                // pipeline stages" mistake. Restricted to '{' after '}'/']' so we
                // never flag legal member access ('{...}[i]', '[...][i]') or a
                // block following a paren ('function(){ }').
                if (c == '{' && (prevSig == '}' || prevSig == ']') && !stack.empty()) {
                    const Kind k = stack.back().kind;
                    if (k == ArrayLit || k == ObjLit || k == Paren)
                        addError(i, 1, tr("Missing ',' before '{'"));
                }
                Kind kind;
                if (c == '(')      kind = Paren;
                else if (c == '[') kind = bpcRegexAllowed(prevSig) ? ArrayLit : Index;
                else               kind = bpcRegexAllowed(prevSig) ? ObjLit : Block;
                paintBracket(i, (int)stack.size());
                stack.push_back({c, i, kind});
                prevSig = c; i++; continue;
            }
            // closing brackets
            if (c == ')' || c == ']' || c == '}') {
                if (stack.empty()) {
                    addError(i, 1, tr("Unexpected '%1'").arg(QChar(c)));
                    paintBracket(i, 0);
                } else {
                    const char open = stack.back().open;
                    if (closerFor(open) != c)
                        addError(i, 1, tr("Mismatched '%1' — expected '%2'")
                                           .arg(QChar(c)).arg(QChar(closerFor(open))));
                    stack.pop_back();
                    paintBracket(i, (int)stack.size());
                }
                prevSig = c; i++; continue;
            }
            if (!std::isspace((unsigned char)c)) prevSig = c;
            i++;
        }

        // Anything still open at end-of-buffer was never closed.
        for (const Frame &f : stack)
            addError(f.pos, 1, tr("Unclosed '%1'").arg(QChar(f.open)));

        // Paint the red squiggles.
        SendScintilla(SCI_SETINDICATORCURRENT, (unsigned long)kErrorIndicator);
        for (const SyntaxError &e : _syntaxErrors)
            SendScintilla(SCI_INDICATORFILLRANGE, (unsigned long)e.pos, (long)e.len);
    }

    bool DocutazScintilla::event(QEvent *e)
    {
        if (e->type() == QEvent::ToolTip) {
            auto *help = static_cast<QHelpEvent *>(e);
            const long pos = SendScintilla(SCI_POSITIONFROMPOINTCLOSE,
                                           (unsigned long)help->pos().x(),
                                           (long)help->pos().y());
            if (pos >= 0) {
                for (const SyntaxError &err : _syntaxErrors) {
                    if (pos >= err.pos && pos < err.pos + err.len) {
                        QToolTip::showText(help->globalPos(), err.message, this);
                        return true;
                    }
                }
            }
            // Not over an error — let the base class show whatever it would.
        }
        return QsciScintilla::event(e);
    }

    void DocutazScintilla::setAppropriateBraceMatching() {
#ifdef Q_OS_MAC
        // On Mac OS when brace matching is enabled, text
        // will blink when you move cursor to some brace or
        // when inside braces. This behaviour is not fully fixed
        // in QScintilla 2.9.1 and 2.8.4
        setBraceMatching(QsciScintilla::NoBraceMatch);
#else
        setBraceMatching(QsciScintilla::StrictBraceMatch);
#endif
    }


}
