#pragma once

#include <QMessageBox>

namespace Docutaz
{
    namespace utils
    {
        int questionDialog(QWidget *parent, const QString &actionText, const QString &itemText, const QString &valueText);

        int questionDialog(QWidget *parent, const QString &actionText, const QString &itemText, const QString &templateText, const QString &valueText);

        // Confirmation for an irreversible action: a warning glyph, a red-filled
        // commit button labelled `confirmText` (e.g. "Drop", "Remove", "Clear")
        // and a neutral Cancel that is the safe default. Returns true only if the
        // user clicked the commit button. `message` may contain rich text.
        bool destructiveConfirm(QWidget *parent, const QString &title,
                                const QString &message, const QString &confirmText);
    }
}

