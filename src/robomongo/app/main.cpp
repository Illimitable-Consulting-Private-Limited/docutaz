#include <QApplication>
#include <QDesktopWidget>

#include <locale.h>

#include <mongocxx/instance.hpp>

#include "robomongo/core/AppRegistry.h"
#include "robomongo/core/settings/SettingsManager.h"
#include "robomongo/core/utils/Logger.h"
#include "robomongo/gui/MainWindow.h"
#include "robomongo/gui/AppStyle.h"
#include "robomongo/gui/dialogs/EulaDialog.h"
#include "robomongo/ssh/ssh.h"
#include "robomongo/utils/RoboCrypt.h"

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
    // never appears under Wayland compositors.
    QApplication::setApplicationName("Docutaz");
    QApplication::setApplicationDisplayName("Docutaz");
    QApplication::setOrganizationName("Docutaz");
    QApplication::setDesktopFileName("docutaz");

    // On Unix/Linux Qt uses the system locale by default, which can break
    // POSIX float/string conversions. Reset to "C" locale.
    setlocale(LC_NUMERIC, "C");

#ifdef Q_OS_MAC
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    // EULA License Agreement
    auto const& settings { Robomongo::AppRegistry::instance().settingsManager() };
    if (!settings->acceptedEulaVersions().contains(PROJECT_VERSION)) {
        bool const showFormPage { settings->programExitedNormally() && !settings->disableHttpsFeatures() };
        Robomongo::EulaDialog eulaDialog(showFormPage);
        settings->setProgramExitedNormally(false);
        settings->save();
        int const result = eulaDialog.exec();
        settings->setProgramExitedNormally(true);
        settings->save();
        if (QDialog::Rejected == result) {
            rbm_ssh_cleanup();
            return 1;
        }
        settings->addAcceptedEulaVersion(PROJECT_VERSION);
        settings->save();
    }

    // Init GUI style
    Robomongo::AppStyleUtils::initStyle();

    settings->setProgramExitedNormally(false);
    settings->save();

    Robomongo::MainWindow mainWindow;
    mainWindow.show();

    for (auto const& msgAndSeverity : Robomongo::RoboCrypt::roboCryptLogs())
        Robomongo::LOG_MSG(msgAndSeverity.first, msgAndSeverity.second);

    int rc = app.exec();
    rbm_ssh_cleanup();
    return rc;
}
