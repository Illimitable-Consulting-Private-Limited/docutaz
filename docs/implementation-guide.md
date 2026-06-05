# Robomongo → MongoDB 8+ with Shell Tabs: Full Implementation Guide

**This is Strategy B only.** Strategies A and C are dead ends for MongoDB 8+ with working shell tabs:
- Strategy A (rebuild against Mongo 5.0 shell): the legacy `mongo` shell works against 6.0/7.0 but 8.0 removed critical opcodes it depends on.
- Strategy C (mongocxx driver only): gives you the GUI tree view but the shell tabs simply don't work.

The only viable path is replacing both layers simultaneously:
- **Embedded MozJS shell** → `mongosh` subprocess with a structured protocol
- **Embedded DBClientConnection** → official `libmongocxx` driver

The good news: because BSON is a self-describing binary format, you can **keep `mongo::BSONObj` intact throughout the rest of the codebase** and bolt on a conversion bridge at the boundary. No mass-refactor of 200+ files required.

---

## 1. What the New Architecture Looks Like

```
┌────────────────────────────────────────────────────────────┐
│                    Robomongo UI (Qt)                        │
│  ExplorerWidget  QueryWidget  OutputWidget  ConnectionDlg   │
└──────────┬──────────────────────────┬───────────────────────┘
           │                          │
           ▼                          ▼
┌──────────────────┐      ┌─────────────────────┐
│   MongoWorker    │      │   MongoWorker        │
│   (GUI thread)   │      │   (Shell thread)     │
└────────┬─────────┘      └──────────┬────────────┘
         │                           │
         ▼                           ▼
┌──────────────────┐      ┌─────────────────────┐
│   MongoClient    │      │   MongoshEngine      │  ← NEW
│   (mongocxx)     │      │   (QProcess bridge)  │  ← NEW
└────────┬─────────┘      └──────────┬────────────┘
         │                           │
         ▼                           ▼
┌──────────────────┐      ┌─────────────────────┐
│ libmongocxx 3.x  │      │  mongosh binary      │
│ (official driver)│      │  (system install)    │
└────────┬─────────┘      └──────────┬────────────┘
         │                           │
         └──────────────┬────────────┘
                        ▼
              MongoDB Server 5.0–8.x
```

**What gets removed entirely:**
- `cmake/mongodb/linux.objects` (the 500+ line list of robomongo-shell object files)
- All `#include <mongo/client/...>` and `#include <mongo/scripting/...>` in MongoClient/MongoWorker/ScriptEngine
- The robomongo-shell submodule / third-party build entirely

**What gets added:**
- `libmongocxx` + `libbsoncxx` as CMake dependencies
- `src/robomongo/core/engine/MongoshEngine.{h,cpp}` (replaces `ScriptEngine.{h,cpp}`)
- `src/robomongo/resources/mongosh_preamble.js` (the JS protocol layer)
- `src/robomongo/core/utils/BsonBridge.{h,cpp}` (EJSON ↔ mongo::BSONObj)

**What stays unchanged** (or nearly so):
- All UI code
- `MongoShellResult.h` / `MongoQueryInfo.h` / `MongoDocument.h` — these structs stay
- `MongoServer.cpp`, `MongoDatabase.cpp` — event-driven architecture unchanged
- `ConnectionSettings`, auth/SSL/SSH settings
- All of `src/robomongo/gui/`

---

## 2. The BSON Bridge: The Trick That Keeps the Refactor Small

BSON is a self-describing binary format. The wire encoding is **identical** between `mongo::BSONObj` (from the old embedded shell), `bsoncxx::document::value` (from libmongocxx), and the `EJSON.stringify()` output from mongosh (which is just BSON serialized as JSON). This means you can convert between them without any semantic translation.

Create `src/robomongo/core/utils/BsonBridge.h`:

```cpp
#pragma once

#include <string>
#include <vector>

#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/json.hpp>

#include <mongo/bson/bsonobj.h>     // from old embedded shell types
#include <mongo/bson/bsonobjbuilder.h>

namespace Robomongo {
namespace BsonBridge {

// Convert EJSON string (from mongosh) → mongo::BSONObj
// Uses bsoncxx::from_json to parse, then copies raw bytes into BSONObj.
// Safe: BSONObj.copy() takes ownership of the data.
inline mongo::BSONObj ejsonToBson(const std::string& ejson) {
    auto bsoncxxVal = bsoncxx::from_json(ejson);
    auto view = bsoncxxVal.view();
    // BSON binary layout is identical between bsoncxx and mongo::BSONObj
    return mongo::BSONObj(reinterpret_cast<const char*>(view.data())).copy();
}

// Convert mongo::BSONObj → canonical EJSON string
inline std::string bsonToEjson(const mongo::BSONObj& obj) {
    bsoncxx::document::view view(
        reinterpret_cast<const uint8_t*>(obj.objdata()),
        static_cast<std::size_t>(obj.objsize())
    );
    return bsoncxx::to_json(view, bsoncxx::ExtendedJsonMode::k_canonical);
}

// Convert bsoncxx::document::value → mongo::BSONObj
inline mongo::BSONObj fromBsoncxx(const bsoncxx::document::view& view) {
    return mongo::BSONObj(reinterpret_cast<const char*>(view.data())).copy();
}

// Convert vector of EJSON strings → vector of mongo::BSONObj
inline std::vector<mongo::BSONObj> ejsonArrayToBsonVec(
    const std::vector<std::string>& ejsonArray)
{
    std::vector<mongo::BSONObj> result;
    result.reserve(ejsonArray.size());
    for (const auto& ej : ejsonArray) {
        result.push_back(ejsonToBson(ej));
    }
    return result;
}

} // BsonBridge
} // Robomongo
```

This header is the entire bridge. Everything else flows through it.

---

## 3. The mongosh Preamble Script

This is the most critical piece. It defines the protocol that `MongoshEngine` speaks over stdio.

Create `src/robomongo/resources/mongosh_preamble.js`:

```javascript
// ============================================================
// ROBOMONGO MONGOSH PREAMBLE — injected once at shell startup
// Protocol version: 1
// ============================================================
(function () {
    'use strict';

    // Preserve the real print() before we shadow it
    const _print = (...args) => print(...args);

    // --------------------------------------------------------
    // Result serialization helpers
    // --------------------------------------------------------

    function _isCursor(val) {
        if (!val || typeof val !== 'object') return false;
        // mongosh cursor types: DBCursor, AggregationCursor, etc.
        const name = val.constructor ? val.constructor.name : '';
        return (
            name === 'Cursor' ||
            name === 'DBCursor' ||
            name === 'AggregationCursor' ||
            name === 'CommandCursor' ||
            (typeof val.hasNext === 'function' && typeof val.toArray === 'function')
        );
    }

    function _isWriteResult(val) {
        if (!val || typeof val !== 'object') return false;
        const name = val.constructor ? val.constructor.name : '';
        return (
            name === 'WriteResult' ||
            name === 'BulkWriteResult' ||
            name === 'InsertOneResult' ||
            name === 'InsertManyResult' ||
            name === 'UpdateResult' ||
            name === 'DeleteResult'
        );
    }

    function _serialize(val) {
        if (val === undefined || val === null) return null;
        try {
            return EJSON.serialize(val);
        } catch (_) {
            return { __robo_text: String(val) };
        }
    }

    // Classify an eval() return value into a typed result object
    function _classify(val, capturedPrints) {
        if (val === undefined) {
            // Script had side effects only (e.g. print(), insert, etc.)
            if (capturedPrints.length > 0) {
                return { type: 'text', text: capturedPrints.join('\n') };
            }
            return null;
        }

        if (_isCursor(val)) {
            // Extract query metadata if we intercepted it, then fetch docs
            const meta = val.__robo_meta || {};
            let docs = [];
            try {
                // Fetch up to __robo_batchSize docs (set during init)
                docs = val.limit(globalThis.__robo_batchSize || 50).toArray()
                          .map(d => _serialize(d));
            } catch (e) {
                return { type: 'error', message: e.message, code: e.code || 0 };
            }
            return {
                type: 'query',
                ns: meta.ns || '',
                query: meta.query ? EJSON.stringify(meta.query) : '{}',
                projection: meta.projection ? EJSON.stringify(meta.projection) : '{}',
                sort: meta.sort ? EJSON.stringify(meta.sort) : '{}',
                skip: meta.skip || 0,
                // -1 means "no limit was set" — caller will apply their own
                limit: meta.limit !== undefined ? meta.limit : -1,
                docs: docs
            };
        }

        if (_isWriteResult(val)) {
            return { type: 'value', value: _serialize(val) };
        }

        if (Array.isArray(val)) {
            return {
                type: 'value',
                value: val.map(d => _serialize(d))
            };
        }

        if (typeof val === 'object') {
            return { type: 'value', value: _serialize(val) };
        }

        return { type: 'text', text: String(val) };
    }

    // --------------------------------------------------------
    // Intercept DBCollection methods to capture query metadata
    // (preserves the original Robomongo behavior: we record what
    //  was queried so the UI can re-paginate via the C++ driver)
    // --------------------------------------------------------
    try {
        const _origFind = DBCollection.prototype.find;
        DBCollection.prototype.find = function (query, projection) {
            const cursor = _origFind.apply(this, arguments);
            cursor.__robo_meta = {
                ns: this.getFullName(),
                query: query || {},
                projection: projection || {},
                sort: {},
                skip: 0,
                limit: -1
            };
            return cursor;
        };

        const _origSort = DBQuery.prototype.sort;
        DBQuery.prototype.sort = function (sort) {
            const cursor = _origSort.apply(this, arguments);
            if (cursor.__robo_meta) cursor.__robo_meta.sort = sort;
            return cursor;
        };

        const _origSkip = DBQuery.prototype.skip;
        DBQuery.prototype.skip = function (n) {
            const cursor = _origSkip.apply(this, arguments);
            if (cursor.__robo_meta) cursor.__robo_meta.skip = n;
            return cursor;
        };

        const _origLimit = DBQuery.prototype.limit;
        DBQuery.prototype.limit = function (n) {
            const cursor = _origLimit.apply(this, arguments);
            if (cursor.__robo_meta) cursor.__robo_meta.limit = n;
            return cursor;
        };
    } catch (_) {
        // Shell may not expose DBCollection.prototype on all versions; not fatal
    }

    // --------------------------------------------------------
    // __robo_exec: the main entry point called by MongoshEngine
    //
    // Arguments:
    //   scriptB64  — the user's JavaScript, base64-encoded
    //   execId     — unique string to use as sentinel marker
    // --------------------------------------------------------
    globalThis.__robo_exec = function (scriptB64, execId) {
        const script = Buffer.from(scriptB64, 'base64').toString('utf8');
        const START = '<<<ROBO_START_' + execId + '>>>';
        const END   = '<<<ROBO_END_'   + execId + '>>>';

        const output = [];
        const captured = [];

        // Shadow print/printjson during execution
        const origPrint     = globalThis.print;
        const origPrintjson = globalThis.printjson;

        globalThis.print = function () {
            captured.push(Array.from(arguments).join(' '));
        };
        globalThis.printjson = function (v) {
            captured.push(tojson(v));
        };

        let result = undefined;
        let error  = null;

        try {
            result = eval(script);   // eslint-disable-line no-eval
        } catch (e) {
            error = { type: 'error', message: e.message, code: e.code || 0,
                      codeName: e.codeName || '' };
        } finally {
            globalThis.print     = origPrint;
            globalThis.printjson = origPrintjson;
        }

        if (error) {
            output.push(error);
        } else {
            const classified = _classify(result, captured);
            if (classified) output.push(classified);
            if (captured.length > 0 && classified && classified.type !== 'text') {
                output.push({ type: 'text', text: captured.join('\n') });
            }
        }

        // Emit framed output that C++ side reads line-by-line
        _print(START);
        _print(JSON.stringify(output));
        _print(END);
    };

    // --------------------------------------------------------
    // __robo_use: switch the active database
    // --------------------------------------------------------
    globalThis.__robo_use = function (dbNameB64) {
        const name = Buffer.from(dbNameB64, 'base64').toString('utf8');
        try {
            db = db.getSiblingDB(name);
            _print('<<<ROBO_USE_OK>>>');
        } catch (e) {
            _print('<<<ROBO_USE_ERR>>>' + e.message);
        }
    };

    // --------------------------------------------------------
    // __robo_ping: keepalive
    // --------------------------------------------------------
    globalThis.__robo_ping = function () {
        try {
            db.runCommand({ ping: 1 });
            _print('<<<ROBO_PING_OK>>>');
        } catch (e) {
            _print('<<<ROBO_PING_ERR>>>' + e.message);
        }
    };

    // --------------------------------------------------------
    // __robo_complete: basic autocompletion
    // --------------------------------------------------------
    globalThis.__robo_complete = function (prefixB64) {
        const prefix = Buffer.from(prefixB64, 'base64').toString('utf8');
        const results = [];
        try {
            // List collections if prefix starts with "db."
            if (prefix.startsWith('db.')) {
                const partial = prefix.slice(3);
                const colls = db.getCollectionNames();
                for (const c of colls) {
                    if (c.startsWith(partial)) results.push('db.' + c);
                }
            }
        } catch (_) { /* ignore */ }
        _print('<<<ROBO_COMPLETE>>>' + JSON.stringify(results) + '<<<ROBO_COMPLETE_END>>>');
    };

    // --------------------------------------------------------
    // __robo_set_batch: set cursor batch size
    // --------------------------------------------------------
    globalThis.__robo_set_batch = function (n) {
        globalThis.__robo_batchSize = n;
        _print('<<<ROBO_BATCH_OK>>>');
    };

    // Signal to C++ that we're ready
    _print('<<<ROBO_PREAMBLE_READY>>>');

})();
```

Add this file to the Qt resource system in `src/robomongo/resources/robo.qrc`:
```xml
<file>mongosh_preamble.js</file>
```


---

## 4. `MongoshEngine` — The ScriptEngine Replacement

This class has the **exact same public API** as the old `ScriptEngine`, so `MongoWorker.cpp` needs only minimal changes.

### 4.1 `src/robomongo/core/engine/MongoshEngine.h`

```cpp
#pragma once

#include <QObject>
#include <QProcess>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QByteArray>

#include "robomongo/core/domain/MongoShellResult.h"
#include "robomongo/core/Enums.h"

namespace Robomongo {

class ConnectionSettings;

// Drop-in replacement for ScriptEngine.
// Drives a persistent mongosh subprocess using a sentinel-framed JSON protocol.
class MongoshEngine : public QObject
{
    Q_OBJECT

public:
    explicit MongoshEngine(ConnectionSettings* connection, int timeoutSec,
                           QObject* parent = nullptr);
    ~MongoshEngine() override;

    // Mirrors ScriptEngine's public interface exactly:
    void init(bool isLoadMongoJs,
              const std::string& serverAddr = "",
              const std::string& dbName = "");

    MongoShellExecResult exec(const std::string& script,
                               const std::string& dbName = {},
                               AggrInfo aggrInfo = AggrInfo());

    void interrupt();
    void use(const std::string& dbName);
    void setBatchSize(int batchSize);
    void ping();
    QStringList complete(const std::string& prefix, AutocompletionMode mode);
    void invalidateDbCollectionsCache();
    bool failedScope() const { return _failed; }
    void changeTimeout(int newTimeout) { _timeoutSec = newTimeout; }

private:
    // Process lifecycle
    bool startProcess(const std::string& dbName);
    void stopProcess();
    bool injectPreamble();

    // I/O helpers
    bool send(const QString& line);
    QByteArray readUntil(const QByteArray& sentinel, int timeoutMs);
    QByteArray readUntilPrompt(int timeoutMs);  // reads until ">" prompt

    // Protocol helpers
    static QString toBase64(const std::string& s);
    static QString makeExecId();

    // Result parsing
    std::vector<MongoShellResult> parseExecOutput(
        const QByteArray& output,
        const std::string& originalScript,
        qint64 elapsedMs);

    // URI builder
    std::string buildConnectionUri(const std::string& dbName) const;
    QStringList buildMongoshArgs(const std::string& uri) const;

    // Mongosh binary discovery
    static QString findMongosh();

    ConnectionSettings* _settings;
    int _timeoutSec;
    int _batchSize = 50;
    bool _failed = false;
    bool _initialized = false;

    QProcess* _proc = nullptr;
    QMutex   _mutex;
    QString  _mongoshPath;
    std::string _currentDb;
};

} // Robomongo
```

### 4.2 `src/robomongo/core/engine/MongoshEngine.cpp`

```cpp
#include "robomongo/core/engine/MongoshEngine.h"

#include <chrono>
#include <atomic>

#include <QThread>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>
#include <QStandardPaths>
#include <QProcessEnvironment>

#include "robomongo/core/settings/ConnectionSettings.h"
#include "robomongo/core/settings/CredentialSettings.h"
#include "robomongo/core/settings/SslSettings.h"
#include "robomongo/core/domain/MongoQueryInfo.h"
#include "robomongo/core/domain/MongoDocument.h"
#include "robomongo/core/utils/BsonBridge.h"
#include "robomongo/utils/Logger.h"

namespace Robomongo {

// ----------------------------------------------------------------
MongoshEngine::MongoshEngine(ConnectionSettings* connection, int timeoutSec,
                             QObject* parent)
    : QObject(parent)
    , _settings(connection)
    , _timeoutSec(timeoutSec)
    , _mongoshPath(findMongosh())
{}

MongoshEngine::~MongoshEngine() {
    stopProcess();
}

// ----------------------------------------------------------------
// Public API
// ----------------------------------------------------------------

void MongoshEngine::init(bool /*isLoadMongoJs*/,
                         const std::string& /*serverAddr*/,
                         const std::string& dbName)
{
    QMutexLocker lock(&_mutex);
    _currentDb = dbName.empty() ? _settings->defaultDatabase() : dbName;
    _failed = !startProcess(_currentDb);
    _initialized = !_failed;
}

MongoShellExecResult MongoshEngine::exec(const std::string& script,
                                          const std::string& dbName,
                                          AggrInfo /*aggrInfo*/)
{
    QMutexLocker lock(&_mutex);

    if (_failed || !_proc || _proc->state() != QProcess::Running) {
        // Attempt re-init once
        _failed = !startProcess(dbName.empty() ? _currentDb : dbName);
        if (_failed) {
            return MongoShellExecResult(true, "mongosh process not running.");
        }
    }

    // Switch database if requested
    if (!dbName.empty() && dbName != _currentDb) {
        use(dbName);
    }

    const QString execId = makeExecId();
    const QString encoded = toBase64(script);
    const QByteArray END_SENTINEL =
        ("<<<ROBO_END_" + execId + ">>>").toUtf8();

    auto t0 = std::chrono::steady_clock::now();

    if (!send("__robo_exec(\"" + encoded + "\", \"" + execId + "\");")) {
        return MongoShellExecResult(true, "Failed to write to mongosh.");
    }

    const QByteArray output = readUntil(END_SENTINEL,
                                        _timeoutSec * 1000);

    auto t1 = std::chrono::steady_clock::now();
    const qint64 elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    const bool timedOut = output.isEmpty() && _timeoutSec > 0;
    if (timedOut) {
        interrupt();
        return MongoShellExecResult(false, {}, _currentDb, true /*timeoutReached*/);
    }

    auto results = parseExecOutput(output, script, elapsedMs);

    // Extract the JSON line between START and END sentinels
    const QByteArray START_SENTINEL =
        ("<<<ROBO_START_" + execId + ">>>").toUtf8();

    return MongoShellExecResult(
        results,
        _settings->serverHost(),
        true,
        _currentDb,
        true
    );
}

void MongoshEngine::interrupt() {
    if (_proc && _proc->state() == QProcess::Running) {
        // SIGINT to mongosh so it stops the current operation
#ifdef Q_OS_UNIX
        ::kill(static_cast<pid_t>(_proc->processId()), SIGINT);
#else
        _proc->kill();
#endif
    }
}

void MongoshEngine::use(const std::string& dbName) {
    _currentDb = dbName;
    if (!_proc || _proc->state() != QProcess::Running) return;
    const QString encoded = toBase64(dbName);
    send("__robo_use(\"" + encoded + "\");");
    readUntil("<<<ROBO_USE_OK>>>", 5000);
}

void MongoshEngine::setBatchSize(int batchSize) {
    _batchSize = batchSize;
    if (!_proc || _proc->state() != QProcess::Running) return;
    send("__robo_set_batch(" + QString::number(batchSize) + ");");
    readUntil("<<<ROBO_BATCH_OK>>>", 3000);
}

void MongoshEngine::ping() {
    if (!_proc || _proc->state() != QProcess::Running) return;
    send("__robo_ping();");
    readUntil("<<<ROBO_PING_OK>>>", 5000);
}

QStringList MongoshEngine::complete(const std::string& prefix,
                                     AutocompletionMode /*mode*/)
{
    if (!_proc || _proc->state() != QProcess::Running) return {};

    const QString encoded = toBase64(prefix);
    send("__robo_complete(\"" + encoded + "\");");

    const QByteArray raw = readUntil("<<<ROBO_COMPLETE_END>>>", 3000);

    // Parse: <<<ROBO_COMPLETE>>>[...]<<<ROBO_COMPLETE_END>>>
    const int start = raw.indexOf("<<<ROBO_COMPLETE>>>");
    if (start < 0) return {};
    const int dataStart = start + QByteArray("<<<ROBO_COMPLETE>>>").size();
    const int end = raw.indexOf("<<<ROBO_COMPLETE_END>>>", dataStart);
    if (end < 0) return {};

    const QByteArray jsonBytes = raw.mid(dataStart, end - dataStart);
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isArray()) return {};

    QStringList completions;
    for (const auto& v : doc.array()) {
        completions << v.toString();
    }
    return completions;
}

void MongoshEngine::invalidateDbCollectionsCache() {
    // No-op: mongosh doesn't cache; each complete() call is live
}

// ----------------------------------------------------------------
// Private: process management
// ----------------------------------------------------------------

bool MongoshEngine::startProcess(const std::string& dbName) {
    stopProcess();

    if (_mongoshPath.isEmpty()) {
        LOG_MSG("MongoshEngine: mongosh binary not found. "
                "Install it: https://www.mongodb.com/docs/mongodb-shell/install/",
                mongo::logger::LogSeverity::Warning());
        _failed = true;
        return false;
    }

    const std::string uri = buildConnectionUri(dbName);
    const QStringList args = buildMongoshArgs(uri);

    _proc = new QProcess(this);
    _proc->setProcessChannelMode(QProcess::MergedChannels);
    _proc->start(_mongoshPath, args);

    if (!_proc->waitForStarted(5000)) {
        delete _proc;
        _proc = nullptr;
        return false;
    }

    // Wait for the shell to connect (reads until the ">" prompt)
    const QByteArray startup = readUntilPrompt(15000);
    if (startup.isEmpty()) {
        stopProcess();
        return false;
    }

    return injectPreamble();
}

void MongoshEngine::stopProcess() {
    if (_proc) {
        if (_proc->state() == QProcess::Running) {
            _proc->write("exit(0)\n");
            _proc->waitForFinished(3000);
            if (_proc->state() == QProcess::Running) _proc->kill();
        }
        _proc->deleteLater();
        _proc = nullptr;
    }
    _initialized = false;
}

bool MongoshEngine::injectPreamble() {
    // Load preamble from Qt resources
    QFile f(":/robomongo/mongosh_preamble.js");
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray preamble = f.readAll();
    f.close();

    // Send preamble in chunks to avoid overwhelming the process buffer
    const int CHUNK = 4096;
    for (int i = 0; i < preamble.size(); i += CHUNK) {
        _proc->write(preamble.mid(i, CHUNK));
        _proc->waitForBytesWritten(1000);
    }
    _proc->write("\n");
    _proc->waitForBytesWritten(1000);

    // Wait for the ready signal
    const QByteArray ready = readUntil("<<<ROBO_PREAMBLE_READY>>>", 10000);
    return ready.contains("<<<ROBO_PREAMBLE_READY>>>");
}

// ----------------------------------------------------------------
// Private: I/O
// ----------------------------------------------------------------

bool MongoshEngine::send(const QString& line) {
    if (!_proc || _proc->state() != QProcess::Running) return false;
    const QByteArray data = (line + "\n").toUtf8();
    const qint64 written = _proc->write(data);
    _proc->waitForBytesWritten(2000);
    return written == data.size();
}

QByteArray MongoshEngine::readUntil(const QByteArray& sentinel, int timeoutMs) {
    QByteArray accumulated;
    const auto deadline = QDeadlineTimer(timeoutMs);

    while (!deadline.hasExpired()) {
        if (_proc->waitForReadyRead(50)) {
            accumulated += _proc->readAll();
        }
        if (accumulated.contains(sentinel)) break;
        if (_proc->state() != QProcess::Running) break;
    }
    return accumulated;
}

QByteArray MongoshEngine::readUntilPrompt(int timeoutMs) {
    // mongosh prompt ends with "> " (with possible db name prefix)
    QByteArray accumulated;
    const auto deadline = QDeadlineTimer(timeoutMs);

    while (!deadline.hasExpired()) {
        if (_proc->waitForReadyRead(100)) {
            accumulated += _proc->readAll();
        }
        // Check for the interactive prompt pattern: ends with "> "
        const QByteArray trimmed = accumulated.trimmed();
        if (trimmed.endsWith("> ") || trimmed.endsWith(">")) break;
        if (_proc->state() != QProcess::Running) break;
    }
    return accumulated;
}

// ----------------------------------------------------------------
// Private: result parsing
// ----------------------------------------------------------------

std::vector<MongoShellResult> MongoshEngine::parseExecOutput(
    const QByteArray& rawOutput,
    const std::string& originalScript,
    qint64 elapsedMs)
{
    std::vector<MongoShellResult> results;

    // Find the JSON payload between the ROBO_START and ROBO_END sentinels
    // The output structure from the preamble is:
    //   <<<ROBO_START_ID>>>\n[...array of result objects...]\n<<<ROBO_END_ID>>>
    const int jsonStart = rawOutput.indexOf('\n') + 1;
    const int jsonEnd   = rawOutput.lastIndexOf('\n');
    if (jsonStart < 0 || jsonEnd <= jsonStart) {
        // If we can't parse, treat everything as text
        const std::string text = QString::fromUtf8(rawOutput)
            .remove(QRegularExpression("<<<ROBO_[^>]+>>>"))
            .trimmed()
            .toStdString();
        if (!text.empty()) {
            results.emplace_back("text", text,
                                 std::vector<MongoDocumentPtr>{},
                                 MongoQueryInfo{}, originalScript, elapsedMs);
        }
        return results;
    }

    const QByteArray jsonBytes = rawOutput.mid(jsonStart, jsonEnd - jsonStart);
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isArray()) {
        // Fallback: raw text
        results.emplace_back("text", jsonBytes.toStdString(),
                             std::vector<MongoDocumentPtr>{},
                             MongoQueryInfo{}, originalScript, elapsedMs);
        return results;
    }

    for (const QJsonValue& item : doc.array()) {
        if (!item.isObject()) continue;
        const QJsonObject obj = item.toObject();
        const QString type = obj["type"].toString();

        if (type == "error") {
            const std::string msg = obj["message"].toString().toStdString();
            results.emplace_back("error", msg,
                                 std::vector<MongoDocumentPtr>{},
                                 MongoQueryInfo{}, originalScript, elapsedMs);

        } else if (type == "query") {
            // Build MongoQueryInfo so the UI can paginate via mongocxx
            MongoQueryInfo qi;
            qi.setBatchSize(_batchSize);

            const std::string ns = obj["ns"].toString().toStdString();
            const int dot = static_cast<int>(ns.find('.'));
            qi.setDatabaseName(dot != (int)std::string::npos
                               ? ns.substr(0, dot) : _currentDb);
            qi.setCollectionName(dot != (int)std::string::npos
                                 ? ns.substr(dot + 1) : ns);

            // Parse EJSON query/projection/sort from strings
            try {
                qi.setFilter(BsonBridge::ejsonToBson(
                    obj["query"].toString("{}").toStdString()));
                qi.setProjection(BsonBridge::ejsonToBson(
                    obj["projection"].toString("{}").toStdString()));
                qi.setSort(BsonBridge::ejsonToBson(
                    obj["sort"].toString("{}").toStdString()));
            } catch (...) { /* malformed EJSON — use empty */ }

            qi.setSkip(obj["skip"].toInt(0));

            // Convert the sample docs for immediate display
            std::vector<MongoDocumentPtr> docs;
            for (const QJsonValue& d : obj["docs"].toArray()) {
                const std::string ejson =
                    QJsonDocument(d.toObject()).toJson(QJsonDocument::Compact).toStdString();
                try {
                    auto bson = BsonBridge::ejsonToBson(ejson);
                    docs.push_back(std::make_shared<MongoDocument>(bson));
                } catch (...) {}
            }

            results.emplace_back("query", "", docs, qi, originalScript, elapsedMs);

        } else if (type == "value") {
            // Single document or scalar
            const QJsonValue val = obj["value"];
            std::vector<MongoDocumentPtr> docs;
            try {
                const std::string ejson =
                    QJsonDocument(val.isObject() ? val.toObject()
                                                 : QJsonObject{{"v", val}})
                    .toJson(QJsonDocument::Compact).toStdString();
                docs.push_back(
                    std::make_shared<MongoDocument>(BsonBridge::ejsonToBson(ejson)));
            } catch (...) {}
            results.emplace_back("value", "", docs, MongoQueryInfo{},
                                 originalScript, elapsedMs);

        } else { // "text"
            const std::string text = obj["text"].toString().toStdString();
            results.emplace_back("text", text,
                                 std::vector<MongoDocumentPtr>{},
                                 MongoQueryInfo{}, originalScript, elapsedMs);
        }
    }

    return results;
}

// ----------------------------------------------------------------
// Private: URI and args building
// ----------------------------------------------------------------

std::string MongoshEngine::buildConnectionUri(const std::string& dbName) const {
    // Build a mongodb:// URI from ConnectionSettings
    // This mirrors the logic in MongoWorker for SSL/auth/replica sets.

    std::string uri = "mongodb://";

    // Auth credentials
    const CredentialSettings* cred = _settings->primaryCredential();
    if (cred && !cred->userName().empty()) {
        // URL-encode username/password (simplified; add full percent-encoding if needed)
        uri += cred->userName() + ":" + cred->userPassword() + "@";
    }

    // Host
    if (_settings->isReplicaSet()) {
        const auto& members = _settings->replicaSetSettings()->members();
        for (int i = 0; i < members.size(); ++i) {
            if (i) uri += ",";
            uri += members[i].toStdString();
        }
    } else {
        uri += _settings->serverHost() + ":" +
               std::to_string(_settings->serverPort());
    }

    // Default database
    uri += "/" + (dbName.empty() ? _settings->defaultDatabase() : dbName);

    // Options
    std::vector<std::string> opts;

    if (cred && !cred->userName().empty()) {
        const std::string mech = cred->mechanism();
        opts.push_back("authMechanism=" +
                       (mech.empty() ? "SCRAM-SHA-256" : mech));
        if (!cred->databaseName().empty())
            opts.push_back("authSource=" + cred->databaseName());
    }

    if (_settings->isReplicaSet()) {
        opts.push_back("replicaSet=" +
                       _settings->replicaSetSettings()->setName().toStdString());
    }

    if (_settings->sslSettings() && _settings->sslSettings()->enabled()) {
        opts.push_back("tls=true");
        if (!_settings->sslSettings()->caFile().empty())
            opts.push_back("tlsCAFile=" + _settings->sslSettings()->caFile());
        if (!_settings->sslSettings()->pemKeyFile().empty())
            opts.push_back("tlsCertificateKeyFile=" +
                           _settings->sslSettings()->pemKeyFile());
        if (_settings->sslSettings()->allowInvalidCertificates())
            opts.push_back("tlsAllowInvalidCertificates=true");
    }

    if (!opts.empty()) {
        uri += "?";
        for (size_t i = 0; i < opts.size(); ++i) {
            if (i) uri += "&";
            uri += opts[i];
        }
    }

    return uri;
}

QStringList MongoshEngine::buildMongoshArgs(const std::string& uri) const {
    QStringList args;
    args << "--norc"
         << "--quiet"
         << QString::fromStdString(uri);

    // If SSH tunnel is active, the URI already points to localhost:tunnelPort —
    // the tunnel is managed by SshTunnelWorker, so no extra args needed here.

    return args;
}

// ----------------------------------------------------------------
// Private: utilities
// ----------------------------------------------------------------

QString MongoshEngine::toBase64(const std::string& s) {
    return QString::fromUtf8(
        QByteArray(s.c_str(), static_cast<int>(s.size())).toBase64());
}

QString MongoshEngine::makeExecId() {
    // Short hex string unique enough per execution
    static std::atomic<uint64_t> counter{0};
    return QString::number(++counter, 16).toUpper().rightJustified(8, '0');
}

QString MongoshEngine::findMongosh() {
    // Check settings override first
    // (we'll add a settings field for this)
    const QString fromEnv = qgetenv("ROBOMONGO_MONGOSH_PATH");
    if (!fromEnv.isEmpty() && QFile::exists(fromEnv)) return fromEnv;

    // Common install locations on Linux
    const QStringList candidates = {
        "/usr/bin/mongosh",
        "/usr/local/bin/mongosh",
        "/opt/mongosh/bin/mongosh",
        QStandardPaths::findExecutable("mongosh")
    };

    for (const QString& c : candidates) {
        if (!c.isEmpty() && QFile::exists(c)) return c;
    }

    return {};
}

} // Robomongo
```

---

## 5. `MongoWorker.cpp` — Replace ScriptEngine with MongoshEngine

In `MongoWorker.cpp`, the ScriptEngine is instantiated and used. The changes are minimal because the public API is identical.

```cpp
// OLD includes to remove:
// #include "robomongo/core/engine/ScriptEngine.h"
// #include <mongo/client/global_conn_pool.h>
// #include <mongo/client/replica_set_monitor.h>
// #include <mongo/util/net/ssl_manager.h>
// #include <mongo/util/net/ssl_options.h>

// NEW includes to add:
#include "robomongo/core/engine/MongoshEngine.h"

// In MongoWorker.h, change:
// ScriptEngine* _scriptEngine;
// to:
// MongoshEngine* _scriptEngine;

// In MongoWorker::init():
// OLD: _scriptEngine = new ScriptEngine(_connSettings, _shellTimeoutSec);
// NEW: _scriptEngine = new MongoshEngine(_connSettings, _shellTimeoutSec);
// The rest of the init() code is unchanged.
```

---

## 6. `MongoClient.cpp` — Rewrite with libmongocxx

This is the most code-intensive change but also the most straightforward: replace the old `mongo::DBClientConnection` calls with `mongocxx::client` calls.

### 6.1 Key Structural Change

```cpp
// src/robomongo/core/mongodb/MongoClient.h

// OLD:
// #include <mongo/client/dbclient.h>
// class MongoClient {
//     mongo::DBClientBase* _dbclient;
// };

// NEW:
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <bsoncxx/json.hpp>

class MongoClient {
    mongocxx::client _client;
    // ...
};
```

### 6.2 Connection Establishment

```cpp
// MongoWorker.cpp — establish connection
// Replace the old connect() code with:

bool MongoWorker::connectToMongoDB() {
    try {
        const std::string uri = buildConnectionUri();  // same logic as MongoshEngine

        mongocxx::uri mongoUri(uri);
        mongocxx::options::client opts;

        // SSL options
        if (_connSettings->sslSettings()->enabled()) {
            mongocxx::options::tls tlsOpts;
            if (!_connSettings->sslSettings()->caFile().empty()) {
                tlsOpts.ca_file(_connSettings->sslSettings()->caFile());
            }
            if (!_connSettings->sslSettings()->pemKeyFile().empty()) {
                tlsOpts.pem_file(_connSettings->sslSettings()->pemKeyFile());
            }
            opts.tls_opts(tlsOpts);
        }

        // Server selection timeout
        opts.server_selection_timeout(
            std::chrono::milliseconds(
                static_cast<int>(_mongoTimeoutSec * 1000)));

        _client = mongocxx::client(mongoUri, opts);

        // Verify connection
        _client["admin"].run_command(
            bsoncxx::builder::stream::document{}
                << "ping" << 1
            << bsoncxx::builder::stream::finalize
        );

        return true;
    } catch (const mongocxx::exception& e) {
        _lastError = e.what();
        return false;
    }
}
```

### 6.3 Core Operations in MongoClient

```cpp
// Complete replacement for key MongoClient operations:

// listDatabases()
std::vector<std::string> MongoClient::getDatabaseNames() {
    try {
        return _client.list_database_names();
    } catch (const mongocxx::exception& e) {
        throw std::runtime_error(e.what());
    }
}

// listCollections()
std::vector<MongoCollectionInfo> MongoClient::getCollectionInfos(
    const std::string& dbName)
{
    std::vector<MongoCollectionInfo> result;
    auto db = _client[dbName];

    mongocxx::options::list_collections opts;
    auto cursor = db.list_collections();

    for (const auto& doc : cursor) {
        const std::string name =
            doc["name"] ? doc["name"].get_string().value.to_string() : "";
        // Build MongoCollectionInfo from bsoncxx document
        result.emplace_back(MongoNamespace(dbName, name),
                            BsonBridge::fromBsoncxx(doc));
    }
    return result;
}

// Query (find)
std::vector<MongoDocumentPtr> MongoClient::query(
    const MongoQueryInfo& qi, int skip, int batchSize)
{
    auto coll = _client[qi.databaseName()][qi.collectionName()];

    mongocxx::options::find opts;
    opts.skip(skip + qi.skip());
    opts.limit(batchSize);

    if (!qi.sort().isEmpty()) {
        opts.sort(bsoncxx::document::view_or_value{
            reinterpret_cast<const uint8_t*>(qi.sort().objdata()),
            static_cast<std::size_t>(qi.sort().objsize())
        });
    }
    if (!qi.projection().isEmpty()) {
        opts.projection(bsoncxx::document::view_or_value{
            reinterpret_cast<const uint8_t*>(qi.projection().objdata()),
            static_cast<std::size_t>(qi.projection().objsize())
        });
    }

    bsoncxx::document::view_or_value filter{
        reinterpret_cast<const uint8_t*>(qi.filter().objdata()),
        static_cast<std::size_t>(qi.filter().objsize())
    };

    auto cursor = coll.find(filter, opts);

    std::vector<MongoDocumentPtr> docs;
    for (const auto& doc : cursor) {
        docs.push_back(
            std::make_shared<MongoDocument>(BsonBridge::fromBsoncxx(doc)));
    }
    return docs;
}

// count()
long long MongoClient::count(const std::string& dbName,
                              const std::string& collName,
                              const mongo::BSONObj& filter)
{
    auto coll = _client[dbName][collName];
    bsoncxx::document::view_or_value bFilter{
        reinterpret_cast<const uint8_t*>(filter.objdata()),
        static_cast<std::size_t>(filter.objsize())
    };
    return coll.count_documents(bFilter);
}

// insert
void MongoClient::insert(const std::string& dbName,
                          const std::string& collName,
                          const mongo::BSONObj& doc)
{
    auto coll = _client[dbName][collName];
    bsoncxx::document::view_or_value bDoc{
        reinterpret_cast<const uint8_t*>(doc.objdata()),
        static_cast<std::size_t>(doc.objsize())
    };
    coll.insert_one(bDoc);
}

// update
void MongoClient::update(const std::string& dbName,
                          const std::string& collName,
                          const mongo::BSONObj& filter,
                          const mongo::BSONObj& update,
                          bool upsert, bool multi)
{
    auto coll = _client[dbName][collName];

    auto toView = [](const mongo::BSONObj& o) {
        return bsoncxx::document::view_or_value{
            reinterpret_cast<const uint8_t*>(o.objdata()),
            static_cast<std::size_t>(o.objsize())
        };
    };

    mongocxx::options::update opts;
    opts.upsert(upsert);

    if (multi)
        coll.update_many(toView(filter), toView(update), opts);
    else
        coll.update_one(toView(filter), toView(update), opts);
}

// remove
void MongoClient::remove(const std::string& dbName,
                          const std::string& collName,
                          const mongo::BSONObj& filter,
                          bool justOne)
{
    auto coll = _client[dbName][collName];
    bsoncxx::document::view_or_value bFilter{
        reinterpret_cast<const uint8_t*>(filter.objdata()),
        static_cast<std::size_t>(filter.objsize())
    };
    if (justOne) coll.delete_one(bFilter);
    else         coll.delete_many(bFilter);
}

// runCommand (used for many admin operations)
mongo::BSONObj MongoClient::runCommand(const std::string& dbName,
                                        const mongo::BSONObj& cmd)
{
    auto db = _client[dbName];
    bsoncxx::document::view_or_value bCmd{
        reinterpret_cast<const uint8_t*>(cmd.objdata()),
        static_cast<std::size_t>(cmd.objsize())
    };
    auto result = db.run_command(bCmd);
    return BsonBridge::fromBsoncxx(result.view());
}

// isMaster / hello (server info)
mongo::BSONObj MongoClient::serverInfo() {
    // Use "hello" for MongoDB 5.0+; fall back to isMaster
    try {
        return runCommand("admin",
            mongo::BSON("hello" << 1));
    } catch (...) {
        return runCommand("admin",
            mongo::BSON("isMaster" << 1));
    }
}

// Aggregate
std::vector<MongoDocumentPtr> MongoClient::aggregate(
    const std::string& dbName,
    const std::string& collName,
    const mongo::BSONObj& pipeline,
    const mongo::BSONObj& options)
{
    auto coll = _client[dbName][collName];

    // pipeline is a BSON array
    bsoncxx::array::view pipelineView{
        reinterpret_cast<const uint8_t*>(pipeline.objdata() + 4), // skip BSON header
        static_cast<std::size_t>(pipeline.objsize() - 5)
    };

    mongocxx::pipeline mongoPipeline;
    // Build pipeline from stages
    for (const auto& element : bsoncxx::document::view{
             reinterpret_cast<const uint8_t*>(pipeline.objdata()),
             static_cast<std::size_t>(pipeline.objsize())}) {
        mongoPipeline.append_stage(element.get_document().value);
    }

    auto cursor = coll.aggregate(mongoPipeline);
    std::vector<MongoDocumentPtr> docs;
    for (const auto& doc : cursor) {
        docs.push_back(
            std::make_shared<MongoDocument>(BsonBridge::fromBsoncxx(doc)));
    }
    return docs;
}
```


---

## 7. `CMakeLists.txt` Changes

This is where the biggest simplification happens: the entire embedded MongoDB shell build disappears.

### 7.1 Root `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(Robomongo)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(PROJECT_VERSION_MAJOR "2")
set(PROJECT_VERSION_MINOR "0")
set(PROJECT_VERSION_PATCH "0")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(RobomongoPrintUtils)
include(RobomongoCMakeDefaults)
include(RobomongoDefaults)
include(RobomongoCommon)
include(RobomongoTargetArch)
include(RobomongoInstallQt)
include(RobomongoPackage)

# ── Qt ──────────────────────────────────────────────────────────
# Qt 5.15 LTS preferred; Qt 6 also works if you update the component names
find_package(Qt5 5.15 COMPONENTS
    Core Gui Widgets PrintSupport Network Xml
    REQUIRED)

# On Linux, Qt5WebEngineWidgets is still optional (was optional before too)
if(NOT SYSTEM_LINUX)
    find_package(Qt5WebEngineWidgets REQUIRED)
endif()

# ── MongoDB C++ driver ──────────────────────────────────────────
# Install: sudo apt-get install libmongocxx-dev   (Ubuntu 22.04+)
# or build from source: https://github.com/mongodb/mongo-cxx-driver
find_package(libmongocxx REQUIRED)
find_package(libbsoncxx  REQUIRED)

# ── OpenSSL ─────────────────────────────────────────────────────
find_package(OpenSSL 1.1.1 REQUIRED)

# ── Threading ───────────────────────────────────────────────────
find_package(Threading REQUIRED)

# ── macOS ───────────────────────────────────────────────────────
if(SYSTEM_MACOSX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)
    find_package(Qt5MacExtras REQUIRED)
endif()

# ── Third-party (kept) ──────────────────────────────────────────
set(LIBSSH2_VERSION 1.11.0)
set(LIBSSH2_DIR src/third-party/libssh2-${LIBSSH2_VERSION})

set(QJSON_VERSION 0.8.1)
set(QJSON_DIR src/third-party/qjson-${QJSON_VERSION})

set(QSCINTILLA_VERSION 2.8.4)
set(QSCINTILLA_DIR src/third-party/qscintilla-${QSCINTILLA_VERSION})

set(ESPRIMA_VERSION 2.7.3)
set(ESPRIMA_DIR src/third-party/esprima-${ESPRIMA_VERSION})

set(GOOGLE_TEST_VERSION 1.8.1)
set(GOOGLE_TEST_DIR src/third-party/googletest-${GOOGLE_TEST_VERSION})

add_subdirectory(${LIBSSH2_DIR})
add_subdirectory(src/robomongo/ssh)
add_subdirectory(${QJSON_DIR})
add_subdirectory(${QSCINTILLA_DIR})
add_subdirectory(${GOOGLE_TEST_DIR})
add_subdirectory(src/robomongo)
add_subdirectory(src/robomongo-unit-tests)

include(RobomongoConfigurationSummary)
```

### 7.2 `src/robomongo/CMakeLists.txt` — Key Changes

Remove everything related to the embedded MongoDB shell and add mongocxx:

```cmake
# ── REMOVE these lines ──────────────────────────────────────────
# include(${CMAKE_SOURCE_DIR}/cmake/FindMongoDB.cmake)
# target_link_libraries(robomongo
#     ${MONGODB_LIBRARIES}        # ← remove
# )
# include_directories(${MONGODB_INCLUDE_DIR})  # ← remove

# ── ADD these lines ─────────────────────────────────────────────
target_link_libraries(robomongo
    # ... existing libs (Qt5, libssh2, QScintilla, OpenSSL) ...
    mongo::mongocxx_shared        # ← new
    mongo::bsoncxx_shared         # ← new
)

# Include dirs for mongocxx (usually auto-set by find_package, but explicit is safer)
target_include_directories(robomongo PRIVATE
    ${LIBMONGOCXX_INCLUDE_DIRS}
    ${LIBBSONCXX_INCLUDE_DIRS}
)

# New source files
target_sources(robomongo PRIVATE
    # ... existing sources ...
    core/engine/MongoshEngine.cpp    # ← new (replaces ScriptEngine.cpp)
    core/utils/BsonBridge.h          # ← new (header-only, no .cpp needed)
    # REMOVE:
    # core/engine/ScriptEngine.cpp
)
```

### 7.3 Remove the `cmake/FindMongoDB.cmake` and `cmake/mongodb/` directory entirely

These contain the embedded shell object file lists and are no longer needed.

---

## 8. mongocxx Instance Initialization

`libmongocxx` requires exactly one `mongocxx::instance` object for the lifetime of the program. Add this to `src/robomongo/app/main.cpp`:

```cpp
// main.cpp
#include <mongocxx/instance.hpp>

int main(int argc, char* argv[])
{
    // Must be created before any mongocxx objects and destroyed last.
    mongocxx::instance mongocxxInstance{};

    QApplication app(argc, argv);
    // ... rest of startup ...
}
```

---

## 9. mongosh Path Setting in the UI

Users need a way to configure the mongosh binary path. Add it to the Options dialog.

### 9.1 `SettingsManager` — add mongosh path field

```cpp
// In src/robomongo/core/settings/SettingsManager.h, add:
QString mongoshPath() const { return _mongoshPath; }
void setMongoshPath(const QString& path) { _mongoshPath = path; }

// In SettingsManager.cpp, load/save in the JSON config:
// "mongoshPath": "/usr/bin/mongosh"
```

### 9.2 Options Dialog — add mongosh path picker

```cpp
// In src/robomongo/gui/dialogs/PreferencesDialog.cpp:

// Add a QLineEdit + Browse button for mongoshPath
// After the user sets it, call:
// AppRegistry::instance().settingsManager()->setMongoshPath(selectedPath);
// AppRegistry::instance().settingsManager()->save();
```

### 9.3 `MongoshEngine::findMongosh()` — check settings first

Update the finder to check `SettingsManager` before scanning the filesystem:

```cpp
QString MongoshEngine::findMongosh() {
    // 1. Settings override
    const QString fromSettings =
        AppRegistry::instance().settingsManager()->mongoshPath();
    if (!fromSettings.isEmpty() && QFile::exists(fromSettings))
        return fromSettings;

    // 2. Environment variable
    const QString fromEnv = qgetenv("ROBOMONGO_MONGOSH_PATH");
    if (!fromEnv.isEmpty() && QFile::exists(fromEnv)) return fromEnv;

    // 3. Well-known paths
    const QStringList candidates = {
        "/usr/bin/mongosh",
        "/usr/local/bin/mongosh",
        "/opt/mongosh/bin/mongosh",
        QStandardPaths::findExecutable("mongosh"),
    };
    for (const QString& c : candidates)
        if (!c.isEmpty() && QFile::exists(c)) return c;

    return {};
}
```

---

## 10. Handling SSH Tunnels

SSH tunnel support in the existing code creates the tunnel in `App.cpp` before calling `MongoWorker`. The tunnel gives you a `localhost:localPort` address. Since `MongoshEngine::buildConnectionUri()` reads from `ConnectionSettings`, and the existing `SshTunnelWorker` already writes the tunnel's local port back to those settings before `MongoWorker` starts, **no special handling is needed** in `MongoshEngine`. The URI it builds will already be `mongodb://localhost:tunnelPort/...`.

The only thing to verify: after `SshTunnelWorker` establishes the tunnel and updates the settings with the local port, the `MongoshEngine` is initialized. Check the order of events in `App.cpp::handleConnectResponse()` — if `MongoWorker` initializes `MongoshEngine` before the tunnel port is written back, you need to delay init until after the SSH response.

---

## 11. Build Steps for Linux (Ubuntu 22.04 / 24.04)

```bash
# 1. Install system dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential git cmake ninja-build \
    qt5-default qtbase5-dev qtbase5-dev-tools \
    libqt5widgets5 libqt5network5 libqt5xml5 libqt5printsupport5 \
    libssl-dev libcurl4-openssl-dev libsasl2-dev \
    libmongocxx-dev libmongoc-dev \
    mongosh                          # MongoDB Shell

# Note: On Ubuntu 22.04, qt5-default is removed; use:
# sudo apt-get install qtbase5-dev qt5-qmake

# 2. Clone the repo
git clone https://github.com/Studio3T/robomongo.git
cd robomongo

# 3. Apply the changes described in this guide.
#    The key files to modify/add:
#    - CMakeLists.txt (root)
#    - src/robomongo/CMakeLists.txt
#    - src/robomongo/app/main.cpp        (mongocxx::instance)
#    - src/robomongo/core/engine/         (add MongoshEngine.h/.cpp)
#    - src/robomongo/core/utils/          (add BsonBridge.h)
#    - src/robomongo/core/mongodb/MongoClient.cpp  (rewrite)
#    - src/robomongo/core/mongodb/MongoWorker.cpp  (swap ScriptEngine → MongoshEngine)
#    - src/robomongo/resources/           (add mongosh_preamble.js)
#    - src/robomongo/resources/robo.qrc  (add preamble to resources)

# 4. Configure
mkdir build && cd build
cmake .. \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake

# 5. Build
ninja -j$(nproc)

# 6. Run
./robo3t
```

If `libmongocxx-dev` is too old in your distro's packages, build from source:
```bash
git clone https://github.com/mongodb/mongo-cxx-driver.git \
    --branch releases/stable --depth 1
cd mongo-cxx-driver
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DMONGOCXX_OVERRIDE_DEFAULT_INSTALL_PREFIX=OFF
cmake --build build -j$(nproc)
sudo cmake --install build
# Then add to cmake: -DCMAKE_PREFIX_PATH=/usr/local
```

---

## 12. Testing

```bash
# Start test instances
docker run -d -p 27020:27017 --name mongo6  mongo:6.0
docker run -d -p 27021:27017 --name mongo7  mongo:7.0
docker run -d -p 27022:27017 --name mongo8  mongo:8.0

# With authentication
docker run -d -p 27023:27017 --name mongo8auth \
    -e MONGO_INITDB_ROOT_USERNAME=admin \
    -e MONGO_INITDB_ROOT_PASSWORD=secret \
    mongo:8.0

# With TLS (needs cert files)
docker run -d -p 27024:27017 --name mongo8tls \
    -v $(pwd)/certs:/etc/ssl mongo:8.0 \
    --tlsMode requireTLS \
    --tlsCertificateKeyFile /etc/ssl/server.pem
```

**Checklist per version:**
- [ ] Connection established (status bar shows server version)
- [ ] Database list loads in explorer
- [ ] Collection list loads
- [ ] Shell tab opens: type `db.runCommand({ping:1})` → `{ ok: 1 }`
- [ ] Shell tab: `db.test.insertOne({x:1})` → success
- [ ] Shell tab: `db.test.find()` → results shown in tree view
- [ ] GUI CRUD: insert document via form, verify in tree
- [ ] GUI CRUD: edit document, delete document
- [ ] Index listing and creation
- [ ] User management panel
- [ ] Aggregation: `db.test.aggregate([{$group:{_id:null,count:{$sum:1}}}])`
- [ ] Auth connection works with SCRAM-SHA-256
- [ ] `use anotherDb` in shell tab switches database

---

## 13. Known Limitations of This Approach

**What you gain:**
- Full MongoDB 5.0–8.x (and beyond) support
- Shell tabs work with the current shell environment
- No embedded shell build required (no SCons, no MongoDB source)
- Much simpler build system
- Smaller binary

**What changes behavior:**
- Shell tab startup has a ~0.5–1s delay (mongosh process startup). The original was instantaneous because the JS engine was already in-process.
- Shell tab `interrupt()` (Ctrl+C) sends SIGINT to mongosh — works, but is slightly less reliable than the in-process interrupt.
- `mongosh` must be installed on the user's system. Add a clear error dialog if not found.
- The preamble's cursor interception may miss some cursor types introduced in future mongosh versions. This will require preamble updates, which are just JS, not C++.
- Some Robomongo-specific shell helpers (e.g. the UUID functions) may need to be ported to the preamble.

**What to add in follow-up iterations:**
- Better autocomplete (inspect schema, use mongosh's own completion API if exposed)
- Progress indication during slow queries (currently no feedback until complete)
- Multi-statement parsing (the current preamble runs the whole script as one eval; the Esprima-based statementize logic from the original ScriptEngine.cpp should be ported to the preamble for per-statement result display)
- Transaction support (`session.withTransaction(...)`)

---

## 14. File Change Summary

| File | Action | Notes |
|---|---|---|
| `CMakeLists.txt` | **Modify** | Remove FindMongoDB, add libmongocxx |
| `src/robomongo/CMakeLists.txt` | **Modify** | Swap ScriptEngine for MongoshEngine |
| `src/robomongo/app/main.cpp` | **Modify** | Add `mongocxx::instance` |
| `src/robomongo/core/engine/ScriptEngine.{h,cpp}` | **Delete** | Replaced by MongoshEngine |
| `src/robomongo/core/engine/MongoshEngine.{h,cpp}` | **Add** | Full implementation above |
| `src/robomongo/core/utils/BsonBridge.h` | **Add** | EJSON↔BSONObj conversion |
| `src/robomongo/resources/mongosh_preamble.js` | **Add** | JS protocol layer |
| `src/robomongo/resources/robo.qrc` | **Modify** | Add preamble to resources |
| `src/robomongo/core/mongodb/MongoClient.{h,cpp}` | **Rewrite** | mongocxx-based |
| `src/robomongo/core/mongodb/MongoWorker.cpp` | **Modify** | Use MongoshEngine, remove mongo:: includes |
| `src/robomongo/core/settings/SettingsManager.{h,cpp}` | **Modify** | Add mongoshPath field |
| `src/robomongo/gui/dialogs/PreferencesDialog.*` | **Modify** | Add mongosh path setting |
| `cmake/FindMongoDB.cmake` | **Delete** | Replaced by find_package(libmongocxx) |
| `cmake/mongodb/` (whole directory) | **Delete** | Embedded shell object file lists |
| `src/third-party/mongodb/` (if present) | **Delete** | Entire embedded shell source |
