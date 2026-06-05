#include "robomongo/core/engine/MongoshEngine.h"

#include <chrono>
#include <atomic>

#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDeadlineTimer>

#include <boost/make_shared.hpp>
#include "robomongo/core/settings/ConnectionSettings.h"
#include "robomongo/core/settings/CredentialSettings.h"
#include "robomongo/core/settings/SslSettings.h"
#include "robomongo/core/settings/ReplicaSetSettings.h"
#include "robomongo/core/domain/MongoQueryInfo.h"
#include "robomongo/core/domain/MongoDocument.h"
#include "robomongo/core/utils/BsonBridge.h"
#include "robomongo/core/utils/Logger.h"
#include "robomongo/core/AppRegistry.h"
#include "robomongo/core/settings/SettingsManager.h"

#ifdef Q_OS_UNIX
#include <signal.h>
#endif

namespace Robomongo {

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

// ── Public API ──────────────────────────────────────────────────────────────

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

    if (_mongoshPath.isEmpty())
        return MongoShellExecResult(true, "mongosh_not_found");

    if (_failed || !_proc || _proc->state() != QProcess::Running) {
        _failed = !startProcess(dbName.empty() ? _currentDb : dbName);
        if (_failed)
            return MongoShellExecResult(true, "mongosh process not running.");
    }

    if (!dbName.empty() && dbName != _currentDb) {
        _currentDb = dbName;
        const QString encoded = toBase64(dbName);
        send("__robo_use(\"" + encoded + "\");");
        readUntil("<<<ROBO_USE_OK>>>", 5000);
    }

    const QString execId = makeExecId();
    const QString encoded = toBase64(script);
    const QByteArray END_SENTINEL =
        ("<<<ROBO_END_" + execId + ">>>").toUtf8();

    auto t0 = std::chrono::steady_clock::now();

    if (!send("__robo_exec(\"" + encoded + "\", \"" + execId + "\");"))
        return MongoShellExecResult(true, "Failed to write to mongosh.");

    const QByteArray output = readUntil(END_SENTINEL, _timeoutSec * 1000);

    auto t1 = std::chrono::steady_clock::now();
    const qint64 elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (output.isEmpty() && _timeoutSec > 0) {
        interrupt();
        return MongoShellExecResult(false, "timeout", true);
    }

    auto results = parseExecOutput(output, script, elapsedMs);
    return MongoShellExecResult(
        results, _settings->serverHost(), true, _currentDb, true);
}

void MongoshEngine::interrupt() {
    if (_proc && _proc->state() == QProcess::Running) {
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

QStringList MongoshEngine::complete(const std::string& prefix, AutocompletionMode /*mode*/) {
    if (!_proc || _proc->state() != QProcess::Running) return {};
    const QString encoded = toBase64(prefix);
    send("__robo_complete(\"" + encoded + "\");");
    const QByteArray raw = readUntil("<<<ROBO_COMPLETE_END>>>", 3000);
    const int start = raw.indexOf("<<<ROBO_COMPLETE>>>");
    if (start < 0) return {};
    const int dataStart = start + QByteArray("<<<ROBO_COMPLETE>>>").size();
    const int end = raw.indexOf("<<<ROBO_COMPLETE_END>>>", dataStart);
    if (end < 0) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(raw.mid(dataStart, end - dataStart));
    if (!doc.isArray()) return {};
    QStringList completions;
    for (const auto& v : doc.array()) completions << v.toString();
    return completions;
}

void MongoshEngine::invalidateDbCollectionsCache() {}

// ── Process lifecycle ────────────────────────────────────────────────────────

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
        delete _proc; _proc = nullptr;
        return false;
    }
    const QByteArray startup = readUntilPrompt(15000);
    if (startup.isEmpty()) { stopProcess(); return false; }
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
    QFile f(":/robomongo/mongosh_preamble.js");
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray preamble = f.readAll();
    f.close();
    const int CHUNK = 4096;
    for (int i = 0; i < preamble.size(); i += CHUNK) {
        _proc->write(preamble.mid(i, CHUNK));
        _proc->waitForBytesWritten(1000);
    }
    _proc->write("\n");
    _proc->waitForBytesWritten(1000);
    const QByteArray ready = readUntil("<<<ROBO_PREAMBLE_READY>>>", 10000);
    return ready.contains("<<<ROBO_PREAMBLE_READY>>>");
}

// ── I/O ──────────────────────────────────────────────────────────────────────

bool MongoshEngine::send(const QString& line) {
    if (!_proc || _proc->state() != QProcess::Running) return false;
    const QByteArray data = (line + "\n").toUtf8();
    const qint64 written = _proc->write(data);
    _proc->waitForBytesWritten(2000);
    return written == data.size();
}

QByteArray MongoshEngine::readUntil(const QByteArray& sentinel, int timeoutMs) {
    QByteArray accumulated;
    QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        if (_proc->waitForReadyRead(50)) accumulated += _proc->readAll();
        if (accumulated.contains(sentinel)) break;
        if (_proc->state() != QProcess::Running) break;
    }
    return accumulated;
}

QByteArray MongoshEngine::readUntilPrompt(int timeoutMs) {
    QByteArray accumulated;
    QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        if (_proc->waitForReadyRead(100)) accumulated += _proc->readAll();
        const QByteArray trimmed = accumulated.trimmed();
        if (trimmed.endsWith("> ") || trimmed.endsWith(">")) break;
        if (_proc->state() != QProcess::Running) break;
    }
    return accumulated;
}

// ── Result parsing ───────────────────────────────────────────────────────────

std::vector<MongoShellResult> MongoshEngine::parseExecOutput(
    const QByteArray& rawOutput,
    const std::string& originalScript,
    qint64 elapsedMs)
{
    std::vector<MongoShellResult> results;

    const int jsonStart = rawOutput.indexOf('\n') + 1;
    const int jsonEnd   = rawOutput.lastIndexOf('\n');
    if (jsonStart <= 0 || jsonEnd <= jsonStart) {
        const std::string text = QString::fromUtf8(rawOutput)
            .remove(QRegularExpression("<<<ROBO_[^>]+>>>"))
            .trimmed().toStdString();
        if (!text.empty())
            results.emplace_back("text", text,
                                 std::vector<MongoDocumentPtr>{},
                                 MongoQueryInfo{}, originalScript, elapsedMs);
        return results;
    }

    const QByteArray jsonBytes = rawOutput.mid(jsonStart, jsonEnd - jsonStart);
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isArray()) {
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
            results.emplace_back("error", obj["message"].toString().toStdString(),
                                 std::vector<MongoDocumentPtr>{},
                                 MongoQueryInfo{}, originalScript, elapsedMs);

        } else if (type == "query") {
            MongoQueryInfo qi;
            qi.setBatchSize(_batchSize);
            const std::string ns = obj["ns"].toString().toStdString();
            const size_t dot = ns.find('.');
            qi.setDatabaseName(dot != std::string::npos ? ns.substr(0, dot) : _currentDb);
            qi.setCollectionName(dot != std::string::npos ? ns.substr(dot + 1) : ns);
            try {
                qi.setFilter(BsonBridge::ejsonToBson(obj["query"].toString("{}").toStdString()));
                qi.setProjection(BsonBridge::ejsonToBson(obj["projection"].toString("{}").toStdString()));
                qi.setSort(BsonBridge::ejsonToBson(obj["sort"].toString("{}").toStdString()));
            } catch (...) {}
            qi.setSkip(obj["skip"].toInt(0));

            std::vector<MongoDocumentPtr> docs;
            for (const QJsonValue& d : obj["docs"].toArray()) {
                const std::string ejson =
                    QJsonDocument(d.toObject()).toJson(QJsonDocument::Compact).toStdString();
                try {
                    docs.push_back(boost::make_shared<MongoDocument>(
                        BsonBridge::ejsonToBson(ejson)));
                } catch (...) {}
            }
            results.emplace_back("query", "", docs, qi, originalScript, elapsedMs);

        } else if (type == "value") {
            const QJsonValue val = obj["value"];
            std::vector<MongoDocumentPtr> docs;
            try {
                const std::string ejson =
                    QJsonDocument(val.isObject() ? val.toObject()
                                                 : QJsonObject{{"v", val}})
                    .toJson(QJsonDocument::Compact).toStdString();
                docs.push_back(boost::make_shared<MongoDocument>(
                    BsonBridge::ejsonToBson(ejson)));
            } catch (...) {}
            results.emplace_back("value", "", docs, MongoQueryInfo{},
                                 originalScript, elapsedMs);

        } else {
            results.emplace_back("text", obj["text"].toString().toStdString(),
                                 std::vector<MongoDocumentPtr>{},
                                 MongoQueryInfo{}, originalScript, elapsedMs);
        }
    }
    return results;
}

// ── URI building ─────────────────────────────────────────────────────────────

std::string MongoshEngine::buildConnectionUri(const std::string& dbName) const {
    std::string uri = "mongodb://";
    const CredentialSettings* cred = _settings->primaryCredential();
    if (cred && !cred->userName().empty())
        uri += cred->userName() + ":" + cred->userPassword() + "@";

    if (_settings->isReplicaSet()) {
        const auto& members = _settings->replicaSetSettings()->members();
        for (int i = 0; i < members.size(); ++i) {
            if (i) uri += ",";
            uri += members[i];
        }
    } else {
        uri += _settings->serverHost() + ":" +
               std::to_string(_settings->serverPort());
    }
    uri += "/" + (dbName.empty() ? _settings->defaultDatabase() : dbName);

    std::vector<std::string> opts;
    if (cred && !cred->userName().empty()) {
        const std::string mech = cred->mechanism();
        opts.push_back("authMechanism=" + (mech.empty() ? "SCRAM-SHA-256" : mech));
        if (!cred->databaseName().empty())
            opts.push_back("authSource=" + cred->databaseName());
    }
    if (_settings->isReplicaSet()) {
        const std::string& entered = _settings->replicaSetSettings()->setNameUserEntered();
        const std::string& cached  = _settings->replicaSetSettings()->cachedSetName();
        const std::string& name = !entered.empty() ? entered : cached;
        if (!name.empty())
            opts.push_back("replicaSet=" + name);
    }
    if (_settings->sslSettings() && _settings->sslSettings()->sslEnabled()) {
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
    return { "--norc", "--quiet", QString::fromStdString(uri) };
}

// ── Utilities ────────────────────────────────────────────────────────────────

QString MongoshEngine::toBase64(const std::string& s) {
    return QString::fromUtf8(
        QByteArray(s.c_str(), static_cast<int>(s.size())).toBase64());
}

QString MongoshEngine::makeExecId() {
    static std::atomic<uint64_t> counter{0};
    return QString::number(++counter, 16).toUpper().rightJustified(8, '0');
}

QString MongoshEngine::findMongosh() {
    // User-configured path takes highest priority
    const QString fromSettings = AppRegistry::instance().settingsManager()->mongoshPath();
    if (!fromSettings.isEmpty() && QFile::exists(fromSettings)) return fromSettings;

    const QString fromEnv = qgetenv("ROBOMONGO_MONGOSH_PATH");
    if (!fromEnv.isEmpty() && QFile::exists(fromEnv)) return fromEnv;
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

} // Robomongo
