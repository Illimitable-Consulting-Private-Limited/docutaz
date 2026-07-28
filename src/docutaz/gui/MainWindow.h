#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QRect>

QT_BEGIN_NAMESPACE
class QLabel;
class QToolBar;
class QDockWidget;
class QToolButton;
class QPushButton;
class QTreeWidgetItem;
class QScreen;
QT_END_NAMESPACE

namespace Docutaz
{
    class ConnectionFailedEvent;
    class ScriptExecutingEvent;
    class ScriptExecutedEvent;
    class OperationFailedEvent;

    class QueryWidgetUpdatedEvent;
    class MongoshSettingsChangedEvent;
    class WorkAreaTabWidget;
    class ConnectionMenu;
    class App;
    class ExplorerWidget;
    class WelcomeTab;
    class UpdateChecker;

    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        typedef QMainWindow BaseClass;
        MainWindow();

        WelcomeTab* getWelcomeTab();
        void showQueryWidgetProgressBar() const;
        void hideQueryWidgetProgressBar() const;

    public Q_SLOTS:
        void manageConnections();
        void toggleOrientation();
        void enterTextMode();
        void enterTreeMode();
        void enterTableMode();
        void enterCustomMode();
        void toggleAutoExpand();
        void toggleAutoExec();
        void toggleLineNumbers();
        void executeScript();
        void stopScript();
        void toggleFullScreen2();
        void selectNextTab();
        void selectPrevTab();
        void duplicateTab();
        void refreshConnections();
        void aboutDocutaz();
        void open();
        void save();
        void saveAs();
        void exit();

        void setDefaultUuidEncoding();
        void setJavaUuidEncoding();
        void setCSharpUuidEncoding();
        void setPythonUuidEncoding();
        void setShellAutocompletionAll();
        void setShellAutocompletionNoCollectionNames();
        void setShellAutocompletionNone();
        void setLoadMongoRcJs();
        void setDisableConnectionShortcuts();

        void toggleLogs(bool show);
        void connectToServer(QAction *action);
        void handle(ConnectionFailedEvent *event);
        void handle(ScriptExecutingEvent *event);
        void handle(ScriptExecutedEvent *event);
        void handle(QueryWidgetUpdatedEvent *event);
        void handle(OperationFailedEvent *event);
        void handle(MongoshSettingsChangedEvent *event);

    protected:
        void keyPressEvent(QKeyEvent *event) override;
        void closeEvent(QCloseEvent *event) override;
        void hideEvent(QHideEvent *event) override;
        void showEvent(QShowEvent *event) override;
        void resizeEvent(QResizeEvent* event) override;
        void moveEvent(QMoveEvent *event) override;
        void changeEvent(QEvent *event) override;
        // Catches Expose events on the native QWindow: the un/re-map transitions
        // are how we detect a Wayland minimize/restore (see showEvent/eventFilter).
        bool eventFilter(QObject *watched, QEvent *event) override;
        
    private Q_SLOTS:
        void updateMenus();
        void setUtcTimeZone();
        void setLocalTimeZone();
        void openPreferences();
        void openWelcomeTab();

        void onConnectToolbarVisibilityChanged(bool isVisisble);
        void onOpenSaveToolbarVisibilityChanged(bool isVisisble);
        void onExecToolbarVisibilityChanged(bool isVisisble);
        void onExplorerVisibilityChanged(bool isVisisble);
        void on_tabChange();

        void toggleMinimize();
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void toggleMinimizeToTray();

        // On application focus changes
        void on_focusChanged();

        void openShellTimeoutDialog();

        // Update notifier (GitHub Releases; sends no user data)
        void toggleCheckUpdates();
        void toggleSaveQueryHistory();
        void checkForUpdatesNow();                    // manual: Help -> Check for Updates
        void onUpdateAvailable(const QString &latestVersion, const QString &releaseUrl);
        void onUpToDate();                            // manual-check feedback
        void onUpdateCheckFailed(const QString &reason);

        // Open the Query History work-area tab.
        void openQueryHistoryTab();

    private:
        // (Re-)install the application stylesheet: the shared global flat chrome
        // plus the MainWindow-specific explorer/separator/queryWidget rules.
        // Re-run on a live colour-scheme change so the appended rules survive
        // Theme::apply() resetting qApp's stylesheet to the bare global sheet.
        void applyChromeStyleSheet();
        void updateConnectionsMenu();
        void createDatabaseExplorer();
        void createTabs();
        void createStatusBar();
        // Show/hide the persistent "mongosh not detected" status-bar item based
        // on current detection.
        void updateMongoshIndicator();
        void restoreWindowSettings();
        void saveWindowSettings() const;
        void rememberNormalGeometry();
        // "Almost maximized", centered on the given screen (nullptr = primary).
        QRect almostMaximizedOn(const QScreen *screen) const;
        // First-run / seed window geometry: almost maximized on the primary screen.
        QRect defaultWindowGeometry() const;

        QDockWidget *_logDock;

        WorkAreaTabWidget *_workArea;

        ExplorerWidget* _explorer;

        App *_app;

        ConnectionMenu *_connectionsMenu;
        QToolButton *_connectButton;
        QMenu *_viewMenu;
        QMenu *_toolbarsMenu;
        QAction *_connectAction;
        // Open/Save tool bar
        QAction *_openAction;
        QAction *_saveAction;
        QAction *_saveAsAction;
        // Execution tool bar
        QAction *_executeAction;
        QAction *_stopAction;
        QAction *_orientationAction;
        QToolBar *_execToolBar;

        // Update notifier
        UpdateChecker *_updateChecker = nullptr;
        QToolBar *_updateBar = nullptr;
        QLabel *_updateLabel = nullptr;

        // Persistent status-bar nudge shown when mongosh can't be found.
        QLabel *_mongoshStatusLabel = nullptr;

#if defined(Q_OS_WIN)
        QSystemTrayIcon *_trayIcon;
#endif

        bool _allowExit;
        bool _updateMenusAtStart = true;
        // Guards the one-time QWindow signal wiring in showEvent().
        bool _windowSignalsHooked = false;
        // Wayland minimize/restore maximized-state preservation. On GNOME Wayland a
        // minimize un-maximizes the window; visibilityChanged(Minimized) fires while
        // it is still maximized (recorded as _tentativeReMax). Because that signal
        // can also flicker without a real minimize, it is only promoted to _armedReMax
        // when the surface actually unmaps (Expose 1->0), and consumed to reassert
        // showMaximized() when it re-maps (Expose 0->1). _lastExposed tracks the
        // previous expose state so those transitions can be detected.
        bool _tentativeReMax = false;
        bool _armedReMax = false;
        bool _lastExposed = true;

        // Last geometry the window had while shown normally (not minimized,
        // maximized or full screen). Captured on every move/resize and re-applied
        // when the window is restored from minimized, so it comes back at exactly
        // the size and position it had — some window managers otherwise restore a
        // minimized window to a tiny default.
        QRect _normalGeometry;
    };

}
