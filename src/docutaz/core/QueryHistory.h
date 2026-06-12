#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>

#include "docutaz/core/utils/SingletonPattern.hpp"

namespace Docutaz
{
    // One executed-query record. Stores the query + context/metadata only —
    // never the result documents (size + privacy; re-run for fresh data).
    struct QueryHistoryEntry
    {
        QString   query;             // exact text submitted (latest run)
        QDateTime timestamp;         // last run
        QString   connection;        // connection name
        QString   database;          // current database at run time
        QString   kind;              // find / aggregate / update / ... (derived)
        QString   errorMessage;      // set when !success
        qint64    durationMs  = 0;
        qint64    resultCount = -1;  // -1 = unknown / not applicable
        int       runCount    = 1;
        bool      success     = true;
        bool      pinned      = false;
    };

    // Persistent, de-duplicated history of executed queries. Singleton (like
    // Logger). Persists to ~/.Docutaz/<version>/history.json.
    class QueryHistoryManager : public QObject,
                                public Patterns::LazySingleton<QueryHistoryManager>
    {
        Q_OBJECT
        friend class Patterns::LazySingleton<QueryHistoryManager>;

    public:
        // Record an execution. De-dupes on (connection, database, normalized
        // text): an identical re-run updates the existing entry (latest
        // metadata, ++runCount) and moves it to the top; otherwise a new entry
        // is prepended. No-op when the "save query history" setting is off or
        // the query is blank.
        void add(const QueryHistoryEntry &entry);

        QList<QueryHistoryEntry> const &entries() const { return _entries; }
        void clear();
        void remove(int index);
        void setPinned(int index, bool pinned);

        // Pure helpers — exposed for testing and reuse.
        // Whitespace-normalize for the dedup key: trim + collapse runs of
        // whitespace to a single space (does NOT strip single spaces, which
        // could corrupt string values). Case-sensitive.
        static QString normalizeQuery(const QString &query);
        static QString dedupKey(const QString &query, const QString &connection,
                                const QString &database);
        static QString deriveKind(const QString &query);   // find/aggregate/update/...

    Q_SIGNALS:
        void changed();

    private:
        QueryHistoryManager();
        ~QueryHistoryManager();

        void load();
        void save() const;
        void trim();
        static QString filePath();

        static constexpr int MaxEntries = 500;
        QList<QueryHistoryEntry> _entries;   // newest first
    };
}
