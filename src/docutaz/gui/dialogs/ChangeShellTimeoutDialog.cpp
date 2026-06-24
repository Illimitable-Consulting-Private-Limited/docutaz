
#include "docutaz/gui/dialogs/ChangeShellTimeoutDialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <QObject>
#include <QLabel>
#include <QGridLayout>
#include <QIntValidator>

#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/core/utils/Logger.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/domain/App.h"
#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/gui/Theme.h"

namespace Docutaz
{
    void changeShellTimeoutDialog()
    {
        auto changeShellTimeoutDialog = new QDialog;
        auto settingsManager = AppRegistry::instance().settingsManager();
        auto currentShellTimeout = new QLabel(QString::number(settingsManager->shellTimeoutSec()));
        auto newShellTimeout = new QLineEdit;
        newShellTimeout->setValidator(new QIntValidator(0, 100000));
        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        Theme::markPrimary(buttonBox->button(QDialogButtonBox::Save));
        QObject::connect(buttonBox, SIGNAL(accepted()), changeShellTimeoutDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), changeShellTimeoutDialog, SLOT(reject()));
        auto lay = new QGridLayout;
        auto firstLabel = new QLabel("Enter new value for " PROJECT_NAME_TITLE " shell timeout in seconds:\n");
        lay->addWidget(firstLabel,                      0, 0, 1, 2, Qt::AlignLeft);
        lay->addWidget(new QLabel("Current Value: "),   1, 0);
        lay->addWidget(currentShellTimeout,             1, 1);
        lay->addWidget(new QLabel("New Value: "),       2, 0);
        lay->addWidget(newShellTimeout,                 2, 1);
        lay->addWidget(buttonBox,                       3, 0, 1, 2, Qt::AlignRight);
        changeShellTimeoutDialog->setLayout(lay);
        changeShellTimeoutDialog->setWindowTitle(PROJECT_NAME_TITLE);

        if (changeShellTimeoutDialog->exec()) {
            settingsManager->setShellTimeoutSec(newShellTimeout->text().toInt());
            settingsManager->save();
            auto subStr = settingsManager->shellTimeoutSec() > 1 ? " seconds." : " second.";
            LOG_MSG("Shell timeout value changed from " + currentShellTimeout->text() + " to " +
                QString::number(settingsManager->shellTimeoutSec()) + subStr, mongo::logger::LogSeverity::Info());

            for (auto const& server : AppRegistry::instance().app()->getServers())
                server->changeWorkerShellTimeout(std::abs(newShellTimeout->text().toInt()));
        }
    }
}
