#include "docutaz/gui/widgets/workarea/QueryWidget.h"

#include <QObject>
#include <QRegularExpression>
#include <QPushButton>
#include <QApplication>
#include <QLabel>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QMainWindow>
#include <QDockWidget>
#include <Qsci/qsciscintilla.h>
#include <Qsci/qscilexerjavascript.h>
#include <mongo/client/dbclient_base.h>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/QueryHistory.h"
#include "docutaz/core/EventBus.h"
#include "docutaz/core/domain/App.h"
#include "docutaz/core/domain/MongoCollection.h"
#include "docutaz/core/domain/MongoDatabase.h"
#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/domain/MongoShell.h"
#include "docutaz/core/domain/MongoAggregateInfo.h"
#include "docutaz/core/events/MongoEvents.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/core/utils/Logger.h"

#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/Theme.h"
#include "docutaz/gui/widgets/workarea/OutputWidget.h"
#include "docutaz/gui/widgets/workarea/ScriptWidget.h"
#include "docutaz/gui/widgets/workarea/OutputItemContentWidget.h"
#include "docutaz/gui/widgets/workarea/OutputItemHeaderWidget.h"
#include "docutaz/gui/editors/PlainJavaScriptEditor.h"
#include "docutaz/gui/editors/JSLexer.h"
#include "docutaz/gui/dialogs/ChangeShellTimeoutDialog.h"
#include "docutaz/gui/dialogs/PreferencesDialog.h"

using namespace mongo;

namespace Docutaz
{
    QueryWidget::QueryWidget(MongoShell *shell, QWidget *parent) :
        QWidget(parent),
        _shell(shell),
        _viewer(nullptr),
        _dock(nullptr),
        _isTextChanged(false)
    {
        AppRegistry::instance().bus()->subscribe(this, DocumentListLoadedEvent::Type, shell);
        AppRegistry::instance().bus()->subscribe(this, ScriptExecutedEvent::Type, shell);
        AppRegistry::instance().bus()->subscribe(this, AutocompleteResponse::Type, shell);

        // Make QMessageBox text selectable
        // setStyleSheet("QMessageBox { messagebox-text-interaction-flags: 5; }");

        _scriptWidget = new ScriptWidget(_shell, this);
        VERIFY(connect(_scriptWidget, SIGNAL(textChanged()), this, SLOT(textChange())));

        // Need to use QMainWindow in order to make use of all features of docking.
        // (Note: Qt full support for dock windows implemented only for QMainWindow)
        _viewer = new OutputWidget(this);
        _outputWindow = new QMainWindow;
        _dock = new CustomDockWidget(this);
        _dock->setAllowedAreas(Qt::NoDockWidgetArea);
        _dock->setFeatures(QDockWidget::DockWidgetFloatable);
        _dock->setWidget(_viewer);
        _dock->setTitleBarWidget(new QWidget);
        VERIFY(connect(_dock, SIGNAL(topLevelChanged(bool)), this, SLOT(on_dock_undock())));
        _outputWindow->addDockWidget(Qt::BottomDockWidgetArea, _dock);

        _outputLabel = new QLabel(this);
        _outputLabel->setContentsMargins(0, 5, 0, 0);
        _outputLabel->setVisible(false);

        // A crisp 1px divider between the query editor and the results. In the
        // flat theme there are no bevels, so this structural line is what
        // separates the two regions (a default sunken/raised HLine is invisible
        // on a near-white surface).
        _line = new QFrame(this);
        _line->setFrameShape(QFrame::NoFrame);
        _line->setFixedHeight(1);
        _line->setStyleSheet(QString("background-color: %1;").arg(Theme::current().mid.name()));

        _mainLayout = new QVBoxLayout;
        _mainLayout->setSpacing(0);
        _mainLayout->setContentsMargins(0, 0, 0, 0);
        _mainLayout->addWidget(_scriptWidget); 
        _mainLayout->addWidget(_line);
        _mainLayout->addWidget(_outputLabel, 0, Qt::AlignTop);
        _mainLayout->addWidget(_outputWindow, 1);      
        setLayout(_mainLayout);
    }

    void QueryWidget::setScriptFocus()
    {
        _scriptWidget->setScriptFocus();
    }

    void QueryWidget::showAutocompletion()
    {
        _scriptWidget->showAutocompletion();
    }

    void QueryWidget::hideAutocompletion()
    {
        _scriptWidget->hideAutocompletion();
    }

    void QueryWidget::setCurrentDatabase(const std::string & dbname)
    {
        _scriptWidget->setCurrentDatabase(dbname);
    }

    void QueryWidget::bringDockToFront()
    {
        _dock->raise(); // required for MAC only; possible Qt bug
        _dock->activateWindow();
    }

    bool QueryWidget::outputWindowDocked() const
    {
        if (_dock) {
            return !_dock->isFloating();
        }
        else {  // _dock is not initialized yet, but it will be docked when initialized
            return true;
        }
    }

    void QueryWidget::execute()
    {
        QString query = _scriptWidget->selectedText();

        if (query.isEmpty())
            query = _scriptWidget->text();

        _lastExecutedQuery = query;   // captured for query history; consumed in handle()

        showProgress();
        _shell->open(QtUtils::toStdString(query));
    }

    void QueryWidget::stop()
    {
        _shell->stop();
    }

    void QueryWidget::toggleOrientation()
    {
        _viewer->toggleOrientation();
    }

    void QueryWidget::openNewTab()
    {
        if (_shell) {
            MongoServer *server = _shell->server();
            QString query = _scriptWidget->selectedText();
            AppRegistry::instance().app()->openShell(server, query, _currentResult.currentDatabase(),
                AppRegistry::instance().settingsManager()->autoExec());
        }
    }

    void QueryWidget::saveToFile()
    {
        if (_shell) {
            _shell->setScript(_scriptWidget->text());
            if (_shell->saveToFile()) {
                _isTextChanged = false;
                updateCurrentTab();
            }
        }
    }

    void QueryWidget::savebToFileAs()
    {
        if (_shell) {
            _shell->setScript(_scriptWidget->text());
            if (_shell->saveToFileAs()) {
                _isTextChanged = false;
                updateCurrentTab();
            }
        }        
    }

    void QueryWidget::openFile()
    {
        if (_shell && _shell->loadFromFile()) {
            _scriptWidget->setText(QtUtils::toQString(_shell->query()));
            _isTextChanged = false;
            updateCurrentTab();
        }
    }

    void QueryWidget::textChange()
    {
        _isTextChanged = true;
        updateCurrentTab();
    }

    QueryWidget::~QueryWidget()
    {
        AppRegistry::instance().app()->closeShell(_shell);
    }

    void QueryWidget::reload()
    {
        execute();
    }

    void QueryWidget::duplicate()
    {
        _scriptWidget->selectAll();
        openNewTab();
    }

    void QueryWidget::enterTreeMode()
    {
        _viewer->enterTreeMode();
    }

    void QueryWidget::enterTextMode()
    {
        _viewer->enterTextMode();
    }

    void QueryWidget::enterTableMode()
    {
        _viewer->enterTableMode();
    }

    void QueryWidget::enterCustomMode()
    {
        _viewer->enterCustomMode();
    }

    void QueryWidget::showProgress()
    {
        _executing = true;
        _viewer->showProgress();
    }

    void QueryWidget::dockUndock() 
    {
        // Toggle between dock/undock
        _dock->setFloating(!_dock->isFloating());
    };
    
    void QueryWidget::changeShellTimeout()
    {
        changeShellTimeoutDialog();
    }

    void QueryWidget::openMongoshPreferences()
    {
        PreferencesDialog dlg(this);
        dlg.exec();
    }

    void QueryWidget::hideProgress()
    {
        _executing = false;
        _viewer->hideProgress();
    }

    void QueryWidget::handle(DocumentListLoadedEvent *event)
    {
        hideProgress();

        if (event->isError()) {
            QString message = QString("Failed to load documents.\n\nError:\n%1")
                .arg(QtUtils::toQString(event->error().errorMessage()));
            QMessageBox::information(this, "Error", message);
            return;
        }

        // this should be in viewer, subscribed to ScriptExecutedEvent
        _viewer->updatePart(event->resultIndex(), event->queryInfo(), event->documents()); 
    }

    void QueryWidget::handle(ScriptExecutedEvent *event)
    {
        hideProgress();
        _currentResult = event->result();

        // A fresh user execution sets _lastExecutedQuery in execute(); aggregation
        // *paging* (driven from the paging widget via _shell->execute()) does not.
        // We must know this before recordHistory() clears it below.
        const bool fromUserExecute = !_lastExecutedQuery.isEmpty();

        // Record this run in query history exactly once. An aggregation can emit
        // several ScriptExecutedEvents; clearing the captured text after the first
        // means only that one is recorded (and re-runs re-capture in execute()).
        if (fromUserExecute) {
            recordHistory(event);
            _lastExecutedQuery.clear();
        }

        if (_currentResult.results().size() == 1) {
            MongoShellResult const& result = _currentResult.results().front();
            AggrInfo const& aggrInfo = result.aggrInfo();
            // Update an existing part in place ONLY for an aggregation paging
            // re-run (not a fresh user execution). A fresh run must fall through
            // to present() so it rebuilds the view: this both renders a first-run
            // aggregate on an empty tab (otherwise updatePart's out-of-range check
            // no-ops → blank) and gives a now-successful query a fresh,
            // tree-capable widget instead of reusing a prior error result's
            // text-only widget (which is stuck in text mode forever).
            if (!fromUserExecute && aggrInfo.isValid && aggrInfo.resultIndex > -1 &&
                aggrInfo.resultIndex < _viewer->partsCount()) {
                _viewer->updatePart(aggrInfo.resultIndex, aggrInfo, _currentResult.results().front().documents());
                return;
            }
        }

        updateCurrentTab();

        if (event->isError()) {
            // The error is reported via the dialog below — don't render a
            // "executed successfully, but no results" message for a failed run.
            _outputLabel->setVisible(false);
            _viewer->present(_shell, event->result().results());
        } else {
            displayData(event->result().results(), event->empty());
        }
        // this should be in ScriptWidget, which is subscribed to ScriptExecutedEvent
        _scriptWidget->setup(event->result());
        activateTabContent();

        if (event->isError()) {
            const QString errorMsg = QString::fromStdString(event->error().errorMessage());

            // Compare case-insensitively: the sentinel ("mongosh_not_found") can
            // reach here with its first letter capitalized by the error pipeline.
            if (errorMsg.compare("mongosh_not_found", Qt::CaseInsensitive) == 0) {
                // mongosh isn't bundled, and many people just unzip a release and
                // run a query before installing it. Show a friendly, actionable
                // info dialog (not a scary error) with a direct download action.
                auto *dia = new QMessageBox(QMessageBox::Information,
                    tr("mongosh not found"),
                    tr("Docutaz runs your queries through MongoDB's mongosh shell, which is "
                       "not bundled and wasn't found on your system.\n\n"
                       "Install mongosh, then make sure it's on your PATH — or set its path "
                       "in Options → Preferences."),
                    QMessageBox::Close, this);
                auto *downloadBtn = new QPushButton(tr("Download mongosh"));
                VERIFY(connect(downloadBtn, &QPushButton::clicked, this, [] {
                    QDesktopServices::openUrl(QUrl("https://www.mongodb.com/try/download/shell"));
                }));
                dia->addButton(downloadBtn, QMessageBox::ActionRole);
                auto *prefBtn = new QPushButton(tr("Open Preferences"));
                VERIFY(connect(prefBtn, SIGNAL(clicked()), this, SLOT(openMongoshPreferences())));
                dia->addButton(prefBtn, QMessageBox::ActionRole);
                dia->setDefaultButton(downloadBtn);
                dia->exec();
            } else {
                // For some cases, event error message already contains string "Error:"
                QString const& subStr =
                    errorMsg.startsWith("Error", Qt::CaseInsensitive) ? "" : "Error:\n";
                QMessageBox::critical(this, "Error",
                    "Failed to execute script.\n\n" + subStr + errorMsg);
            }
        }

        if (event->timeoutReached()) {
            auto const shellTimeoutSec = AppRegistry::instance().settingsManager()->shellTimeoutSec();
            QString const subStr = _currentResult.results().size() > 1 ?
                "At least one of the scripts has reached shell timeout" :
                "The script has reached shell timeout";
            QString const secondStr = (shellTimeoutSec > 1) ? " seconds)" : " second)";
            QString messageShort = "Failed to execute all of the script. " + subStr + " (" +
                                    QString::number(shellTimeoutSec) + secondStr + " limit. ";
            QString messageLong = messageShort + 
                                  "\n\nPlease increase the value of shell timeout using button below "
                                  "or from the main window menu \"Options->Change Shell Timeout\".";
            LOG_MSG(messageShort, mongo::logger::LogSeverity::Error());

            auto errorDia = new QMessageBox(QMessageBox::Icon::Critical, "Error", messageLong);
            auto but = new QPushButton("Change Shell Timeout");
            VERIFY(connect(but, SIGNAL(clicked()), this, SLOT(changeShellTimeout())));
            errorDia->addButton(but, QMessageBox::NoRole);
            errorDia->exec();
        }
    }

    void QueryWidget::recordHistory(ScriptExecutedEvent *event)
    {
        // Only successful runs are kept in history — a syntax error or a failed
        // statement is noise, not something worth re-opening later. A run fails
        // if the engine flagged an error, or any produced result is an error
        // (e.g. a runtime/server error captured by the preamble, which doesn't
        // set the engine-level error flag).
        if (event->isError())
            return;
        for (MongoShellResult const &r : _currentResult.results())
            if (r.type() == "error")
                return;

        QueryHistoryEntry e;
        e.query = _lastExecutedQuery;
        if (_shell && _shell->server() && _shell->server()->connectionRecord())
            e.connection = QtUtils::toQString(_shell->server()->connectionRecord()->connectionName());
        e.database = QtUtils::toQString(_currentResult.currentDatabase());

        qint64 durationMs = 0;
        qint64 docs = 0;
        for (MongoShellResult const &r : _currentResult.results()) {
            durationMs += r.elapsedMs();
            docs       += static_cast<qint64>(r.documents().size());
        }
        e.durationMs  = durationMs;
        e.resultCount = docs;   // reached only for successful runs

        QueryHistoryManager::instance().add(e);
    }

    void QueryWidget::activateTabContent()
    {
        AppRegistry::instance().bus()->publish(new QueryWidgetUpdatedEvent(this, _currentResult.results().size()));
        _scriptWidget->setScriptFocus();
    }

    void QueryWidget::handle(AutocompleteResponse *event)
    {
        if (event->isError()) {
            // Do not show error message (error should be already logged)
            return;
        }

        _scriptWidget->showAutocompletion(event->list, QtUtils::toQString(event->prefix) );
    }

    void QueryWidget::on_dock_undock()
    {
        if (!_dock->isFloating()) {    // If output window docked 
            // Settings to revert to docked mode
            _scriptWidget->ui_queryLinesCountChanged();
            _mainLayout->addWidget(_scriptWidget);                     
            _mainLayout->addWidget(_line);
            _mainLayout->addWidget(_outputWindow, 1);
            _dock->setFeatures(QDockWidget::DockWidgetFloatable);
            _dock->setTitleBarWidget(new QWidget);
            _viewer->applyDockUndockSettings(true);
        }
        else {              // output window undocked(floating)
            // Settings for query window in order to use maximum space
            _scriptWidget->disableFixedHeight();
            _mainLayout->addWidget(_scriptWidget, 1); 
            _mainLayout->addWidget(_line);
            _mainLayout->addWidget(_outputWindow);
            _dock->setFeatures(QDockWidget::DockWidgetClosable);
            _dock->setTitleBarWidget(nullptr);
            _viewer->applyDockUndockSettings(false);
        }
    }

    void QueryWidget::updateCurrentTab()
    {
        const QString &shellQuery = QtUtils::toQString(_shell->query());
        QString toolTipQuery = shellQuery.left(700);

        QString tabTitle, toolTipText;
        if (_shell) {
            QFileInfo fileInfo(_shell->filePath());
            if (fileInfo.isFile()) {
                    tabTitle = fileInfo.fileName();
                    toolTipText = fileInfo.filePath();
            }
        }

        if (tabTitle.isEmpty() && shellQuery.isEmpty()) {
            tabTitle = "New Shell";
        }
        else {

            if (tabTitle.isEmpty()) {
                tabTitle = shellQuery.left(41).replace(QRegularExpression("[\n\r\t]"), " ");
                toolTipText = QString("<pre>%1</pre>").arg(toolTipQuery);
            }
            else {
                //tabTitle = QString("%1 %2").arg(tabTitle).arg(shellQuery);
                toolTipText = QString("<b>%1</b><br/><pre>%2</pre>").arg(toolTipText).arg(toolTipQuery);
            }
        }

        if (_isTextChanged) {
            tabTitle = "* " + tabTitle;
        }

        emit titleChanged(tabTitle);
        emit toolTipChanged(toolTipText);
    }

    void QueryWidget::displayData(const std::vector<MongoShellResult> &results, bool empty)
    {
        if (!empty) {
            bool isOutVisible = results.size() == 0 && !_scriptWidget->text().isEmpty();
            if (isOutVisible) {
                _outputLabel->setText("  Script executed successfully, but there are no results to show.");
            }
            _outputLabel->setVisible(isOutVisible);
        }

        _viewer->present(_shell, results);
    }
}
