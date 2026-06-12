#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace Docutaz
{
    /**
     * Checks GitHub for a newer published release of Docutaz.
     *
     * Privacy: this sends NO user data. It issues a single GET to the public
     * GitHub Releases API with only a User-Agent header; nothing about the user,
     * their connections or their data is transmitted. The `/releases/latest`
     * endpoint returns the latest *published, non-prerelease* release, so drafts
     * and release candidates are ignored automatically.
     *
     * Emits updateAvailable() only when the latest release is strictly newer
     * than this build (PROJECT_VERSION). Network/parse failures are ignored
     * silently — a failed update check must never disrupt the app.
     */
    class UpdateChecker : public QObject
    {
        Q_OBJECT
    public:
        explicit UpdateChecker(QObject *parent = nullptr);

        // Fire one check. Safe to call repeatedly; replies are handled async.
        // userInitiated == true (a manual "Check for Updates" menu click) also
        // reports the "already up to date" and "check failed" outcomes via the
        // signals below; a background/launch check stays silent on those and
        // only ever emits updateAvailable().
        void checkForUpdate(bool userInitiated = false);

        // Strict version test: true ONLY when `latest` is newer than `current`
        // (tolerates a leading 'v' and -rc/-beta suffixes; unparseable input is
        // treated as not-newer). The update banner fires only on this, so a
        // rollback — GitHub's latest release being the same as or older than the
        // installed build — never prompts an "update". Static + public so the
        // guarantee can be unit-tested.
        static bool isNewerVersion(const QString &latest, const QString &current);

    Q_SIGNALS:
        void updateAvailable(const QString &latestVersion, const QString &releaseUrl);
        void upToDate();                              // only for a userInitiated check
        void checkFailed(const QString &reason);      // only for a userInitiated check

    private Q_SLOTS:
        void onReply(QNetworkReply *reply);

    private:
        QNetworkAccessManager *_net;
    };
}
