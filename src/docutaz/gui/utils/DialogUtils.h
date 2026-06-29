#pragma once

#include <QMessageBox>

namespace Docutaz
{
    class ConnectionSettings;

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

        // Production-safety gate for an update/delete. Returns true (proceed)
        // immediately when no confirmation is owed — `conn` is null, the master
        // "confirm destructive ops" preference is off, or the connection's
        // environment is not in the guarded set. Otherwise shows a prominent
        // confirmation naming the connection and its environment, with a "Don't
        // ask me again" checkbox that clears the preference, and returns whether
        // the user chose to proceed. `actionText` completes "You are about to
        // <actionText>" (e.g. "delete the selected document(s)").
        bool confirmGuardedWrite(QWidget *parent, const ConnectionSettings *conn,
                                 const QString &actionText);
    }
}

