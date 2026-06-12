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
        void checkForUpdate();

    Q_SIGNALS:
        void updateAvailable(const QString &latestVersion, const QString &releaseUrl);

    private Q_SLOTS:
        void onReply(QNetworkReply *reply);

    private:
        QNetworkAccessManager *_net;
    };
}
