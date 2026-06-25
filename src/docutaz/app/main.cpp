#include <QApplication>

#include <cstdio>
#include <locale.h>

#ifdef Q_OS_WIN
#include <shobjidl.h>
#endif

#include <mongocxx/instance.hpp>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/utils/Logger.h"
#include "docutaz/gui/MainWindow.h"
#include "docutaz/gui/AppStyle.h"
#include "docutaz/gui/Theme.h"
#include "docutaz/gui/dialogs/EulaDialog.h"
#include "docutaz/ssh/ssh.h"
#include "docutaz/utils/DocutazCrypt.h"

int main(int argc, char *argv[])
{
    if (rbm_ssh_init())
        return 1;

    // Must be created before any mongocxx objects and destroyed last.
    mongocxx::instance mongocxxInstance{};

    // Cross Platform High DPI support - Qt 5.7
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // Initialize Qt application
    QApplication app(argc, argv);

    // Identify the application so the desktop environment can match the
    // window to its .desktop entry and show the launcher/taskbar icon.
    // On Wayland the icon comes from the .desktop file matched by app_id
    // (= desktopFileName), NOT from setWindowIcon(); without this the icon
    // never appears under Wayland compositors. The desktop-file name is the
    // reverse-DNS app id (in.illimitable.Docutaz) so it matches the installed
    // in.illimitable.Docutaz.desktop and the Flatpak app id. On X11 Qt builds
    // WM_CLASS as <applicationName>\0<desktopFileName>, so StartupWMClass in the
    // .desktop must equal this same id.
    QApplication::setApplicationName("Docutaz");
    // Deliberately NOT setting applicationDisplayName: Qt appends it to every
    // top-level window title, which turned the main window's "Docutaz - 2.3"
    // into "Docutaz - 2.3 - Docutaz". The app titles its own windows, so the
    // fallback it would provide isn't needed. (WM_CLASS uses applicationName,
    // and the Windows taskbar identity uses the explicit AUMID below.)
    QApplication::setOrganizationName("Docutaz");
    QApplication::setDesktopFileName("in.illimitable.Docutaz");

#ifdef Q_OS_WIN
    // Give the process a stable, explicit AppUserModelID. Without one Windows
    // derives the taskbar identity from the executable path, which drifts
    // across version upgrades/new install locations and is one of the things
    // that makes the taskbar lose the app icon (falling back to the generic
    // blank-page icon). A fixed CompanyName.AppName id keeps taskbar grouping,
    // pinning and the icon stable across updates. Must be set before the first
    // window is shown.
    SetCurrentProcessExplicitAppUserModelID(L"Illimitable.Docutaz");
#endif

    // On Unix/Linux Qt uses the system locale by default, which can break
    // POSIX float/string conversions. Reset to "C" locale.
    setlocale(LC_NUMERIC, "C");

    // --version: print the version and exit cleanly. Placed after the
    // QApplication ctor (so it exercises platform-plugin loading and every
    // linked runtime library) but before the EULA dialog (which would block on
    // a machine with no accepted EULA). CI uses this as a smoke test that the
    // packaged binary actually launches.
    if (app.arguments().contains(QStringLiteral("--version")) ||
        app.arguments().contains(QStringLiteral("-v"))) {
        std::printf("Docutaz %s\n", PROJECT_VERSION);
        rbm_ssh_cleanup();
        return 0;
    }

#ifdef Q_OS_MAC
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    // License agreement. Keyed on the license-text revision (CurrentEulaVersion),
    // NOT the app version, so a routine update never re-prompts; it only shows
    // again if the license text itself changes (and the bump moves the token).
    auto const& settings { Docutaz::AppRegistry::instance().settingsManager() };
    if (!settings->acceptedEulaVersions().contains(QString::fromLatin1(Docutaz::CurrentEulaVersion))) {
        Docutaz::EulaDialog eulaDialog;
        if (QDialog::Rejected == eulaDialog.exec()) {
            rbm_ssh_cleanup();
            return 1;
        }
        settings->addAcceptedEulaVersion(QString::fromLatin1(Docutaz::CurrentEulaVersion));
        settings->save();
    }

    // Init GUI style settings, then apply the flat Docutaz look: Fusion base
    // style + a curated light/dark palette (built from the design tokens, with
    // the brand accent as Highlight/Link), selected from the OS colour scheme.
    Docutaz::AppStyleUtils::initStyle();
    Docutaz::Theme::setSchemePreference(
        static_cast<Docutaz::Theme::Scheme>(settings->colorSchemePreference()));
    Docutaz::Theme::setUiFontOverride(settings->uiFontFamily());
    Docutaz::Theme::apply();
    // Follow live OS light/dark switches (Qt 6.5+): re-apply palette/QSS and
    // notify widgets that paint theme colours imperatively.
    Docutaz::Theme::installColorSchemeWatch();

    Docutaz::MainWindow mainWindow;
    mainWindow.show();

    for (auto const& msgAndSeverity : Docutaz::DocutazCrypt::roboCryptLogs())
        Docutaz::LOG_MSG(msgAndSeverity.first, msgAndSeverity.second);

    int rc = app.exec();
    rbm_ssh_cleanup();
    return rc;
}
