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

    // Mirrors ScriptEngine's public interface:
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
    bool startProcess(const std::string& dbName);
    void stopProcess();
    bool injectPreamble();

    bool send(const QString& line);
    // Writes raw bytes (e.g. a multi-line wrapped script) to mongosh stdin,
    // ensuring a trailing newline so the REPL evaluates the final statement.
    bool sendRaw(const QByteArray& data);
    QByteArray readUntil(const QByteArray& sentinel, int timeoutMs);
    QByteArray readUntilPrompt(int timeoutMs);

    static QString toBase64(const std::string& s);
    static QString makeExecId();

    std::vector<MongoShellResult> parseExecOutput(
        const QByteArray& output,
        const std::string& originalScript,
        qint64 elapsedMs);

    // Strips <<<ROBO_LOG>>>...<<<ROBO_LOG_END>>> lines from raw output,
    // routing each to the application log widget. Returns the cleaned buffer.
    QByteArray drainLogs(const QByteArray& raw);

    std::string buildConnectionUri(const std::string& dbName) const;
    QStringList buildMongoshArgs(const std::string& uri) const;
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
