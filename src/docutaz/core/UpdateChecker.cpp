#include "docutaz/core/UpdateChecker.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QVersionNumber>

namespace Docutaz
{
    namespace
    {
        // Parse a version string ("v2.1.0", "2.1.0-rc1", ...) into a
        // QVersionNumber, dropping any leading 'v' and pre-release suffix.
        QVersionNumber parseVersion(QString s)
        {
            s = s.trimmed();
            if (s.startsWith('v', Qt::CaseInsensitive))
                s.remove(0, 1);
            int const dash = s.indexOf('-');     // strip -rc1 / -beta / +meta
            if (dash >= 0)
                s = s.left(dash);
            return QVersionNumber::fromString(s);
        }

        bool isNewer(const QString &latest, const QString &current)
        {
            return QVersionNumber::compare(parseVersion(latest), parseVersion(current)) > 0;
        }
    }

    UpdateChecker::UpdateChecker(QObject *parent)
        : QObject(parent), _net(new QNetworkAccessManager(this))
    {
        connect(_net, &QNetworkAccessManager::finished, this, &UpdateChecker::onReply);
    }

    void UpdateChecker::checkForUpdate()
    {
        QNetworkRequest req(QUrl(
            "https://api.github.com/repos/" PROJECT_GITHUB_REPO "/releases/latest"));
        // GitHub requires a User-Agent; we send only the app name + version.
        req.setRawHeader("User-Agent", "Docutaz/" PROJECT_VERSION);
        req.setRawHeader("Accept", "application/vnd.github+json");
        _net->get(req);
    }

    void UpdateChecker::onReply(QNetworkReply *reply)
    {
        reply->deleteLater();

        // Offline, rate-limited, or any error: ignore silently.
        if (reply->error() != QNetworkReply::NoError)
            return;

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject())
            return;

        const QJsonObject obj = doc.object();
        // /releases/latest already excludes drafts/prereleases; double-check.
        if (obj.value("draft").toBool() || obj.value("prerelease").toBool())
            return;

        const QString tag = obj.value("tag_name").toString();
        const QString url = obj.value("html_url").toString();
        if (tag.isEmpty())
            return;

        if (isNewer(tag, QStringLiteral(PROJECT_VERSION)))
            Q_EMIT updateAvailable(tag, url);
    }
}
