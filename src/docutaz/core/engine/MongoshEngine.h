#pragma once

#include <QObject>
#include <QProcess>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QByteArray>

#include "docutaz/core/domain/MongoShellResult.h"
#include "docutaz/core/Enums.h"

namespace Docutaz {

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

    // True if a mongosh binary can be located (user-configured path, env var, or
    // a standard install location). Used by the UI to proactively nudge the user
    // to install/configure mongosh before they hit a failed query.
    static bool isMongoshAvailable();

private:
    bool startProcess(const std::string& dbName);
    void stopProcess();
    bool injectPreamble();
    // Run one throwaway query through the full prepare→execute→emit protocol so
    // the user's first real script isn't the first thing the freshly-started
    // mongosh REPL / async-rewriter ever runs. Non-fatal on failure.
    void warmUp();

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

} // Docutaz
