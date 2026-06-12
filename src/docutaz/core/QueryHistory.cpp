#include "docutaz/core/QueryHistory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"

namespace Docutaz
{
    namespace
    {
        QJsonObject toJson(const QueryHistoryEntry &e)
        {
            QJsonObject o;
            o["query"]       = e.query;
            o["timestamp"]   = e.timestamp.toString(Qt::ISODate);
            o["connection"]  = e.connection;
            o["database"]    = e.database;
            o["kind"]        = e.kind;
            o["durationMs"]  = double(e.durationMs);
            o["resultCount"] = double(e.resultCount);
            o["runCount"]    = e.runCount;
            o["success"]     = e.success;
            if (!e.success)  o["error"]  = e.errorMessage;
            if (e.pinned)    o["pinned"] = true;
            return o;
        }

        QueryHistoryEntry fromJson(const QJsonObject &o)
        {
            QueryHistoryEntry e;
            e.query        = o.value("query").toString();
            e.timestamp    = QDateTime::fromString(o.value("timestamp").toString(), Qt::ISODate);
            e.connection   = o.value("connection").toString();
            e.database     = o.value("database").toString();
            e.kind         = o.value("kind").toString();
            e.errorMessage = o.value("error").toString();
            e.durationMs   = qint64(o.value("durationMs").toDouble());
            e.resultCount  = o.contains("resultCount") ? qint64(o.value("resultCount").toDouble()) : -1;
            e.runCount     = o.value("runCount").toInt(1);
            e.success      = o.value("success").toBool(true);
            e.pinned       = o.value("pinned").toBool(false);
            return e;
        }
    }

    QueryHistoryManager::QueryHistoryManager() { load(); }
    QueryHistoryManager::~QueryHistoryManager() = default;

    QString QueryHistoryManager::filePath()
    {
        return QString("%1/.Docutaz/%2/history.json").arg(QDir::homePath()).arg(PROJECT_VERSION);
    }

    QString QueryHistoryManager::normalizeQuery(const QString &query)
    {
        // simplified(): trim + collapse runs of whitespace to a single space.
        return query.simplified();
    }

    QString QueryHistoryManager::dedupKey(const QString &query, const QString &connection,
                                          const QString &database)
    {
        const QChar sep(0x1f);   // unit separator — won't appear in the parts
        return connection + sep + database + sep + normalizeQuery(query);
    }

    QString QueryHistoryManager::deriveKind(const QString &query)
    {
        // Case-sensitive (Mongo method names). First match by priority wins.
        if (query.contains(".aggregate("))                                 return QStringLiteral("aggregate");
        if (query.contains(".find(") || query.contains(".findOne("))       return QStringLiteral("find");
        if (query.contains(".insert"))                                     return QStringLiteral("insert");
        if (query.contains(".update") || query.contains(".replaceOne("))   return QStringLiteral("update");
        if (query.contains(".delete") || query.contains(".remove("))       return QStringLiteral("delete");
        if (query.contains(".count")  || query.contains(".distinct(")
            || query.contains(".estimatedDocumentCount("))                 return QStringLiteral("count");
        return QStringLiteral("other");
    }

    void QueryHistoryManager::add(const QueryHistoryEntry &incoming)
    {
        if (!AppRegistry::instance().settingsManager()->saveQueryHistory())
            return;
        if (incoming.query.trimmed().isEmpty())
            return;

        QueryHistoryEntry e = incoming;
        if (e.kind.isEmpty())       e.kind = deriveKind(e.query);
        if (!e.timestamp.isValid()) e.timestamp = QDateTime::currentDateTime();

        const QString key = dedupKey(e.query, e.connection, e.database);
        for (int i = 0; i < _entries.size(); ++i) {
            const QueryHistoryEntry &cur = _entries.at(i);
            if (dedupKey(cur.query, cur.connection, cur.database) == key) {
                e.runCount = cur.runCount + 1;   // running total across runs
                e.pinned   = cur.pinned;         // preserve pin state
                _entries.removeAt(i);
                break;
            }
        }
        _entries.prepend(e);
        trim();
        save();
        emit changed();
    }

    void QueryHistoryManager::trim()
    {
        // Cap the list, dropping oldest first — but never a pinned entry.
        for (int i = _entries.size() - 1; i >= 0 && _entries.size() > MaxEntries; --i)
            if (!_entries.at(i).pinned)
                _entries.removeAt(i);
    }

    void QueryHistoryManager::clear()
    {
        _entries.clear();
        save();
        emit changed();
    }

    void QueryHistoryManager::remove(int index)
    {
        if (index < 0 || index >= _entries.size()) return;
        _entries.removeAt(index);
        save();
        emit changed();
    }

    void QueryHistoryManager::setPinned(int index, bool pinned)
    {
        if (index < 0 || index >= _entries.size()) return;
        _entries[index].pinned = pinned;
        save();
        emit changed();
    }

    void QueryHistoryManager::load()
    {
        QFile f(filePath());
        if (!f.open(QIODevice::ReadOnly))
            return;
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
        _entries.clear();
        for (const QJsonValue &v : arr)
            if (v.isObject())
                _entries.append(fromJson(v.toObject()));
    }

    void QueryHistoryManager::save() const
    {
        const QString path = filePath();
        QDir().mkpath(QFileInfo(path).absolutePath());

        QJsonArray arr;
        for (const QueryHistoryEntry &e : _entries)
            arr.append(toJson(e));

        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return;
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        f.commit();
    }
}
