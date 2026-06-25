#include "docutaz/gui/utils/DialogUtils.h"

#include <QPushButton>

#include "docutaz/gui/GlyphIcons.h"
#include "docutaz/gui/Theme.h"

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
