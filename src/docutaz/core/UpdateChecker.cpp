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

    void UpdateChecker::checkForUpdate(bool userInitiated)
    {
        QNetworkRequest req(QUrl(
            "https://api.github.com/repos/" PROJECT_GITHUB_REPO "/releases/latest"));
        // GitHub requires a User-Agent; we send only the app name + version.
        req.setRawHeader("User-Agent", "Docutaz/" PROJECT_VERSION);
        req.setRawHeader("Accept", "application/vnd.github+json");
        QNetworkReply *reply = _net->get(req);
        // Remember whether this was a manual check so onReply knows whether to
        // report the no-update / failure outcomes.
        reply->setProperty("userInitiated", userInitiated);
    }

    void UpdateChecker::onReply(QNetworkReply *reply)
    {
        reply->deleteLater();
        const bool userInitiated = reply->property("userInitiated").toBool();

        // Offline, rate-limited (HTTP 4xx/5xx), TLS error, etc.
        if (reply->error() != QNetworkReply::NoError) {
            if (userInitiated)
                Q_EMIT checkFailed(reply->errorString());
            return;
        }

        // fromJson(...).object() is an empty object for non-object/garbage input.
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = obj.value("tag_name").toString();
        const QString url = obj.value("html_url").toString();
        // /releases/latest already excludes drafts/prereleases; double-check.
        if (tag.isEmpty() || obj.value("draft").toBool() || obj.value("prerelease").toBool()) {
            if (userInitiated)
                Q_EMIT checkFailed(tr("Unexpected response from the update server."));
            return;
        }

        if (isNewer(tag, QStringLiteral(PROJECT_VERSION)))
            Q_EMIT updateAvailable(tag, url);
        else if (userInitiated)
            Q_EMIT upToDate();
    }
}
