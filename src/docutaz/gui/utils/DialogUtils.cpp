#include "docutaz/gui/utils/DialogUtils.h"

#include <QCheckBox>
#include <QPushButton>

#include "docutaz/gui/GlyphIcons.h"
#include "docutaz/gui/Theme.h"
#include "docutaz/gui/ConnectionEnvironment.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{
    namespace utils
    {
        namespace
        {
            const QString titleTemaple = QString("%1 %2");
            const QString textTemaple = QString("%1 <b>%3</b> %2?");
        }

        bool destructiveConfirm(QWidget *parent, const QString &title,
                                const QString &message, const QString &confirmText)
        {
            QMessageBox box(parent);
            box.setWindowTitle(title);
            box.setText(message);
            box.setTextFormat(Qt::RichText);
            // Monochrome warning glyph tinted danger-red, in place of the platform
            // critical/question pixmap, to match the flat icon language.
            box.setIconPixmap(
                GlyphIcons::icon("warn", Theme::current().danger).pixmap(QSize(40, 40)));

            QPushButton *confirm = box.addButton(confirmText, QMessageBox::AcceptRole);
            QPushButton *cancel  = box.addButton(QMessageBox::Cancel);
            // Cancel is the safe default for an irreversible action.
            box.setDefaultButton(cancel);
            Theme::markDanger(confirm);

            box.exec();
            return box.clickedButton() == confirm;
        }

        bool confirmGuardedWrite(QWidget *parent, const ConnectionSettings *conn,
                                 const QString &actionText)
        {
            if (!conn)
                return true;

            SettingsManager *sm = AppRegistry::instance().settingsManager();
            if (!sm->confirmDestructiveOps())
                return true;

            const std::string env = conn->environment();
            if (!sm->isEnvironmentGuarded(env))
                return true;

            const QString envName = ConnectionEnvironment::displayName(env);
            const QString connName = QtUtils::toQString(conn->getReadableName());

            QMessageBox box(parent);
            box.setWindowTitle("Confirm " + envName + " operation");
            box.setTextFormat(Qt::RichText);
            box.setText(
                QString("You are about to %1 on <b>%2</b>, a <b>%3</b> connection.<br><br>"
                        "This change cannot be undone. Continue?")
                    .arg(actionText, connName.toHtmlEscaped(), envName));
            box.setIconPixmap(
                GlyphIcons::icon("warn", Theme::current().danger).pixmap(QSize(40, 40)));

            QPushButton *proceed = box.addButton("Proceed", QMessageBox::AcceptRole);
            QPushButton *cancel  = box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(cancel);   // safe default
            Theme::markDanger(proceed);

            QCheckBox *dontAsk = new QCheckBox("Don't ask me again for any connection", &box);
            dontAsk->setToolTip(
                "Disable the destructive-operation confirmation for all guarded "
                "connections. Re-enable it any time in Preferences.");
            box.setCheckBox(dontAsk);

            box.exec();
            const bool proceeded = box.clickedButton() == proceed;

            // Only honour "don't ask again" when the user actually proceeded —
            // cancelling shouldn't silently disable the safety net.
            if (proceeded && dontAsk->isChecked()) {
                sm->setConfirmDestructiveOps(false);
                sm->save();
            }
            return proceeded;
        }

        int questionDialog(QWidget *parent, const QString &actionText, const QString &itemText, const QString& valueText)
        {
            return questionDialog(parent, actionText, itemText, textTemaple, valueText);
        }

        int questionDialog(QWidget *parent, const QString &actionText, const QString &itemText, const QString &templateText, const QString &valueText)
        {
            // Every caller is an irreversible drop, so route through the
            // red-filled destructive confirmation (commit button == actionText).
            const QString title   = titleTemaple.arg(actionText).arg(itemText);
            const QString message = templateText.arg(actionText).arg(itemText.toLower()).arg(valueText);
            return destructiveConfirm(parent, title, message, actionText)
                       ? QMessageBox::Yes : QMessageBox::No;
        }
    }
}
