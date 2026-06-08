#include "docutaz/core/mongodb/MongoWorker.h"

#include <algorithm>
#include <exception>
#include <memory>

#include <QThread>

#include <mongocxx/exception/exception.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/domain/App.h"
#include "docutaz/core/domain/MongoShellResult.h"
#include "docutaz/core/domain/MongoCollectionInfo.h"
#include "docutaz/core/events/MongoEvents.h"
#include "docutaz/core/engine/MongoshEngine.h"
#include "docutaz/core/EventBus.h"
#include "docutaz/core/mongodb/MongoClient.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/settings/ReplicaSetSettings.h"
#include "docutaz/core/settings/CredentialSettings.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/settings/SslSettings.h"
#include "docutaz/core/utils/Logger.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/utils/StringOperations.h"

namespace Docutaz
{
    std::string const APP_VERSION = PROJECT_VERSION;

    MongoWorker::MongoWorker(ConnectionSettings *connection, bool isLoadMongoRcJs, int batchSize,
                             double mongoTimeoutSec, int shellTimeoutSec, QObject *parent)
        : QObject(parent),
        _scriptEngine(nullptr),
        _isLoadMongoRcJs(isLoadMongoRcJs),
        _batchSize(batchSize),
        _timerId(-1),
        _dbAutocompleteCacheTimerId(-1),
        _mongoTimeoutSec(mongoTimeoutSec),
        _shellTimeoutSec(shellTimeoutSec),
        _isQuiting(0),
        _connSettings(connection)
    {
        _connSettings->setServerHost(
            QString::fromStdString(_connSettings->serverHost()).trimmed().toStdString());
        _thread = new QThread();
        moveToThread(_thread);
        VERIFY(connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater())));
        VERIFY(connect(_thread, SIGNAL(finished()), this, SLOT(deleteLater())));
        _thread->start();
    }

    void MongoWorker::timerEvent(QTimerEvent *event)
    {
        if (_timerId == event->timerId()) {
            keepAlive();
            return;
        }
        if (_dbAutocompleteCacheTimerId == event->timerId() && _scriptEngine) {
            _scriptEngine->invalidateDbCollectionsCache();
            return;
        }
    }

    void MongoWorker::keepAlive()
    {
        try {
            using bsoncxx::builder::basic::kvp;
            using bsoncxx::builder::basic::make_document;
            _client["admin"].run_command(make_document(kvp("ping", 1)).view());

            if (_scriptEngine)
                _scriptEngine->ping();
        } catch (const std::exception &ex) {
            sendLog(this, LogEvent::RBM_WARN,
                    "Failed to ping the server. " + std::string(ex.what()));
        }
    }

    void MongoWorker::init()
    {
        try {
            _scriptEngine.reset(new MongoshEngine(_connSettings, _shellTimeoutSec));
            _scriptEngine->init(_isLoadMongoRcJs);
            _scriptEngine->use(_connSettings->defaultDatabase());
            _scriptEngine->setBatchSize(_batchSize);
            constexpr int PING_INTERVAL_MSEC{60 * 1000};
            _timerId = startTimer(PING_INTERVAL_MSEC);
            _dbAutocompleteCacheTimerId = startTimer(30000);
        } catch (const std::exception &ex) {
            auto const msg{"Failed to initialize MongoWorker. Reason: "};
            sendLog(this, LogEvent::RBM_ERROR, msg + std::string(ex.what()));
            throw std::runtime_error(msg + std::string(ex.what()));
        }
    }

    void MongoWorker::interrupt()
    {
        try {
            if (_isQuiting || !_scriptEngine)
                return;
            _scriptEngine->interrupt();
        } catch (const std::exception &ex) {
            sendLog(this, LogEvent::RBM_ERROR, std::string(ex.what()));
        }
    }

    MongoWorker::~MongoWorker()
    {
        if (_timerId != -1)
            killTimer(_timerId);
        if (_dbAutocompleteCacheTimerId != -1)
            killTimer(_dbAutocompleteCacheTimerId);
        delete _connSettings;
    }

    void MongoWorker::stopAndDelete()
    {
        _isQuiting = 1;
        _thread->quit();
    }

    void MongoWorker::changeTimeout(int newTimeout)
    {
        if (_scriptEngine)
            _scriptEngine->changeTimeout(newTimeout);
    }

    std::string MongoWorker::buildConnectionUri() const
    {
        std::string uri = "mongodb://";
        const CredentialSettings *cred = _connSettings->primaryCredential();
        if (cred && !cred->userName().empty())
            uri += cred->userName() + ":" + cred->userPassword() + "@";

        if (_connSettings->isReplicaSet()) {
            const auto &members = _connSettings->replicaSetSettings()->members();
            for (int i = 0; i < static_cast<int>(members.size()); ++i) {
                if (i) uri += ",";
                uri += members[i];
            }
        } else {
            uri += _connSettings->serverHost() + ":" +
                   std::to_string(_connSettings->serverPort());
        }
        uri += "/" + _connSettings->defaultDatabase();

        std::vector<std::string> opts;
        if (cred && !cred->userName().empty()) {
            const std::string mech = cred->mechanism();
            opts.push_back("authMechanism=" + (mech.empty() ? "SCRAM-SHA-256" : mech));
            if (!cred->databaseName().empty())
                opts.push_back("authSource=" + cred->databaseName());
        }
        if (_connSettings->isReplicaSet()) {
            // Prefer user-entered name; fall back to cached discovered name.
            // Omit entirely when both are empty so the driver auto-discovers.
            const std::string& entered = _connSettings->replicaSetSettings()->setNameUserEntered();
            const std::string& cached  = _connSettings->replicaSetSettings()->cachedSetName();
            const std::string& name = !entered.empty() ? entered : cached;
            if (!name.empty())
                opts.push_back("replicaSet=" + name);
        }
        if (_connSettings->sslSettings() && _connSettings->sslSettings()->sslEnabled()) {
            opts.push_back("tls=true");
            if (!_connSettings->sslSettings()->caFile().empty())
                opts.push_back("tlsCAFile=" + _connSettings->sslSettings()->caFile());
            if (!_connSettings->sslSettings()->pemKeyFile().empty())
                opts.push_back("tlsCertificateKeyFile=" +
                               _connSettings->sslSettings()->pemKeyFile());
            if (_connSettings->sslSettings()->allowInvalidCertificates())
                opts.push_back("tlsAllowInvalidCertificates=true");
        }
        // Fail fast on unreachable servers. Without an explicit timeout the
        // driver waits the default 30s (serverSelectionTimeoutMS), freezing the
        // worker on a bad connection.
        const int timeoutMs = (_mongoTimeoutSec > 0 ? static_cast<int>(_mongoTimeoutSec) : 10) * 1000;
        opts.push_back("serverSelectionTimeoutMS=" + std::to_string(timeoutMs));
        opts.push_back("connectTimeoutMS=" + std::to_string(timeoutMs));
        // For a single (non-replica-set) host, connect directly. This disables
        // SDAM topology monitoring, whose background monitor threads otherwise
        // keep retrying an unreachable host and linger across failed attempts.
        if (!_connSettings->isReplicaSet())
            opts.push_back("directConnection=true");

        if (!opts.empty()) {
            uri += "?";
            for (size_t i = 0; i < opts.size(); ++i) {
                if (i) uri += "&";
                uri += opts[i];
            }
        }
        return uri;
    }

    ReplicaSet MongoWorker::getReplicaSetInfo() const
    {
        try {
            using bsoncxx::builder::basic::kvp;
            using bsoncxx::builder::basic::make_document;
            auto res  = _client["admin"].run_command(make_document(kvp("hello", 1)).view());
            auto view = res.view();

            std::string setName;
            if (auto e = view["setName"])
                setName = std::string(e.get_string().value);

            mongo::HostAndPort primary;
            if (auto e = view["primary"])
                primary = mongo::HostAndPort(std::string(e.get_string().value));

            std::vector<std::pair<std::string, bool>> membersAndHealths;
            if (auto e = view["hosts"])
                for (auto &h : e.get_array().value)
                    membersAndHealths.push_back({std::string(h.get_string().value), true});
            if (auto e = view["passives"])
                for (auto &h : e.get_array().value)
                    membersAndHealths.push_back({std::string(h.get_string().value), false});

            return ReplicaSet(setName, primary, membersAndHealths);
        } catch (const std::exception &ex) {
            return ReplicaSet("", mongo::HostAndPort(), {}, ex.what());
        }
    }

    std::string MongoWorker::getAuthBase() const
    {
        if (_connSettings->hasEnabledPrimaryCredential())
            return _connSettings->primaryCredential()->databaseName();
        return {};
    }

    std::vector<std::string> MongoWorker::getDatabaseNamesSafe(EstablishConnectionRequest *event)
    {
        std::set<std::string> dbNames;
        auto const primaryCredential{_connSettings->primaryCredential()};

        try {
            std::unique_ptr<MongoClient> client(getClient());
            std::vector<std::string> fetched = client->getDatabaseNames();
            dbNames = std::set<std::string>{fetched.begin(), fetched.end()};
        } catch (const std::exception &ex) {
            // Only surface the "Manually specify visible databases" hint to the
            // user when it is actionable: a primary connection that actually has
            // credentials and is not already relying on a populated manual list.
            // Without the parentheses this expression parsed as
            // (A && B && C && !D) || E, where E (manuallyVisibleDbs().empty()) is
            // true for nearly every connection — so EVERY failed connection (wrong
            // host/port, no auth at all) popped a modal Warning on top of the error
            // dialog and froze the UI.
            bool const informUser{
                event != nullptr &&
                event->connectionType == ConnectionType::ConnectionPrimary &&
                _connSettings->credentialCount() > 0 &&
                (!primaryCredential->useManuallyVisibleDbs() ||
                 primaryCredential->manuallyVisibleDbs().empty())
            };
            std::string const hint{
                "\n\nHint: If this user has access to a specific database, "
                "please use \"Manually specify visible databases\" option in "
                "Connection Settings window -> Authentication tab."
            };
            sendLog(this, LogEvent::RBM_WARN, ex.what() + hint, informUser);
        }

        if (_connSettings->credentialCount() > 0 &&
            primaryCredential->useManuallyVisibleDbs() &&
            !primaryCredential->manuallyVisibleDbs().empty()) {
            QString const manuallyVisibleDbs{
                QString::fromStdString(primaryCredential->manuallyVisibleDbs())};
            const auto splitList = manuallyVisibleDbs.split(',');
            for (auto const &db : std::vector<QString>(splitList.begin(), splitList.end()))
                dbNames.insert(db.toStdString());
        }

        std::string const authBase = getAuthBase();
        if (!authBase.empty())
            dbNames.insert(authBase);

        return std::vector<std::string>{dbNames.begin(), dbNames.end()};
    }

    bool MongoWorker::handle(EstablishConnectionRequest *event)
    {
        QMutexLocker lock(&_firstConnectionMutex);
        std::unique_ptr<ReplicaSet> repSetInfo(new ReplicaSet);
        auto errorCode = EventError::ErrorCode::Unknown;

        try {
            _client = mongocxx::client(mongocxx::uri(buildConnectionUri()));

            std::vector<std::string> const dbNames = getDatabaseNamesSafe(event);
            if (dbNames.empty())
                throw std::runtime_error("Failed to execute \"listdatabases\" command.");

            if (_connSettings->isReplicaSet()) {
                ReplicaSet const &setInfo = getReplicaSetInfo();
                repSetInfo.reset(new ReplicaSet(setInfo));
                if (setInfo.primary.empty()) {
                    sendLog(this, LogEvent::RBM_ERROR, setInfo.errorStr);
                    throw std::runtime_error(setInfo.errorStr);
                }
            }

            init();

            std::unique_ptr<MongoClient> client(getClient());
            auto connInfo = ConnectionInfo(
                _connSettings->getFullAddress(), dbNames,
                client->getVersion(), client->dbVersionStr(),
                client->getStorageEngineType(), event->uuid);

            reply(event->sender(), new EstablishConnectionResponse(
                      this, connInfo, event->connectionType, *repSetInfo.release()));
            return true;
        } catch (const std::exception &ex) {
            auto errorReason = _connSettings->sslSettings()->sslEnabled()
                                   ? EstablishConnectionResponse::ErrorReason::MongoSslConnection
                                   : EstablishConnectionResponse::ErrorReason::MongoAuth;
            reply(event->sender(),
                  new EstablishConnectionResponse(this, EventError(ex.what(), errorCode),
                                                  event->connectionType, ConnectionInfo(event->uuid),
                                                  *repSetInfo.release(), errorReason));
            sendLog(this, LogEvent::RBM_ERROR, ex.what());
        }
        return false;
    }

    void MongoWorker::handle(RefreshReplicaSetFolderRequest *event)
    {
        try {
            ReplicaSet const replicaSetInfo = getReplicaSetInfo();
            if (replicaSetInfo.primary.empty()) {
                reply(event->sender(),
                      new RefreshReplicaSetFolderResponse(
                          this, replicaSetInfo, event->expanded, EventError(replicaSetInfo.errorStr)));
                sendLog(this, LogEvent::RBM_ERROR, replicaSetInfo.errorStr);
            } else {
                reply(event->sender(),
                      new RefreshReplicaSetFolderResponse(this, replicaSetInfo, event->expanded));
            }
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new RefreshReplicaSetFolderResponse(
                      this, ReplicaSet(), event->expanded, EventError(ex.what())));
            sendLog(this, LogEvent::RBM_ERROR, ex.what());
        }
    }

    void MongoWorker::handle(LoadDatabaseNamesRequest *event)
    {
        try {
            std::vector<std::string> dbNames = getDatabaseNamesSafe();

            for (auto it = dbNames.begin(); it != dbNames.end(); ++it)
                _createdDbs.erase(*it);

            for (auto const &db : _createdDbs)
                dbNames.push_back(db);

            if (!dbNames.empty())
                reply(event->sender(), new LoadDatabaseNamesResponse(this, dbNames));
            else
                reply(event->sender(),
                      new LoadDatabaseNamesResponse(
                          this, EventError("Failed to execute \"listdatabases\" command.")));
        } catch (const std::exception &ex) {
            reply(event->sender(), new LoadDatabaseNamesResponse(this, EventError(ex.what())));
            sendLog(this, LogEvent::RBM_ERROR, ex.what());
        }
    }

    void MongoWorker::handle(LoadCollectionNamesRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            auto const &namespaces = client->getCollectionNamesWithDbname(event->databaseName());
            std::vector<MongoCollectionInfo> const &collInfos = client->runCollStatsCommand(namespaces);
            client->done();
            reply(event->sender(),
                  new LoadCollectionNamesResponse(this, event->databaseName(), collInfos));
        } catch (const std::exception &ex) {
            reply(event->sender(), new LoadCollectionNamesResponse(this, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(LoadUsersRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            const std::vector<MongoUser> &users = client->getUsers(event->databaseName());
            client->done();
            reply(event->sender(),
                  new LoadUsersResponse(this, event->databaseName(), users));
        } catch (const std::exception &ex) {
            reply(event->sender(), new LoadUsersResponse(this, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(LoadCollectionIndexesRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            const std::vector<IndexInfo> &ind = client->getIndexes(event->collection());
            client->done();
            reply(event->sender(), new LoadCollectionIndexesResponse(this, ind));
        } catch (const std::exception &ex) {
            reply(event->sender(), new LoadCollectionIndexesResponse(this, EventError(ex.what())));
            sendLog(this, LogEvent::RBM_ERROR, ex.what());
        }
    }

    void MongoWorker::handle(AddEditIndexRequest *event)
    {
        const IndexInfo &newIndex = event->newInfo();
        const IndexInfo &oldIndex = event->oldInfo();
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->addEditIndex(oldIndex, newIndex);
            client->done();
            reply(event->sender(), new AddEditIndexResponse(this, oldIndex, newIndex));

            std::vector<IndexInfo> const &indexes = client->getIndexes(newIndex._collection);
            reply(event->sender(), new LoadCollectionIndexesResponse(this, indexes));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new AddEditIndexResponse(this, EventError(ex.what()), oldIndex, newIndex));
        }
    }

    void MongoWorker::handle(DropCollectionIndexRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->dropIndexFromCollection(event->collection(), event->index());
            client->done();
            reply(event->sender(),
                  new DropCollectionIndexResponse(this, event->collection(), event->index()));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new DropCollectionIndexResponse(this, EventError(ex.what()), event->index()));
        }
    }

    void MongoWorker::handle(LoadFunctionsRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            const std::vector<MongoFunction> &funcs = client->getFunctions(event->databaseName());
            client->done();

            if (funcs.empty()) {
                MongoShellExecResult const &result =
                    _scriptEngine->exec("db.system.js.find()", event->databaseName());
                std::vector<MongoFunction> functions;
                if (!result.results().empty()) {
                    auto const &resultDocs = result.results().front().documents();
                    for (auto const res : resultDocs)
                        functions.push_back(MongoFunction(res->bsonObj()));
                }
                reply(event->sender(),
                      new LoadFunctionsResponse(this, event->databaseName(), functions));
                return;
            }
            reply(event->sender(), new LoadFunctionsResponse(this, event->databaseName(), funcs));
        } catch (const std::exception &ex) {
            reply(event->sender(), new LoadFunctionsResponse(this, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(InsertDocumentRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            if (event->overwrite())
                client->saveDocument(event->obj(), event->ns());
            else
                client->insertDocument(event->obj(), event->ns());
            client->done();
            reply(event->sender(), new InsertDocumentResponse(this));
        } catch (const std::exception &ex) {
            reply(event->sender(), new InsertDocumentResponse(this, EventError(ex.what())));
            sendLog(this, LogEvent::RBM_ERROR, ex.what());
        }
    }

    void MongoWorker::handle(RemoveDocumentRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->removeDocuments(event->ns(), event->query(),
                                    event->removeCount() == RemoveDocumentCount::ONE);
            client->done();
            reply(event->sender(),
                  new RemoveDocumentResponse(this, event->removeCount(), event->index()));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new RemoveDocumentResponse(this, EventError(ex.what()),
                                             event->removeCount(), event->index()));
        }
    }

    void MongoWorker::handle(ExecuteQueryRequest *event)
    {
        auto const executeQuery = [&]() {
            std::unique_ptr<MongoClient> client{getClient()};
            std::vector<MongoDocumentPtr> docs = client->query(event->queryInfo());
            client->done();
            reply(event->sender(),
                  new ExecuteQueryResponse(this, event->resultIndex(), event->queryInfo(), docs));
        };

        try {
            executeQuery();
        } catch (const std::exception &ex) {
            reply(event->sender(), new ExecuteQueryResponse(this, EventError(ex.what())));
            sendLog(this, LogEvent::RBM_ERROR, std::string(ex.what()));
        }
    }

    void MongoWorker::handle(ExecuteScriptRequest *event)
    {
        try {
            if (!_scriptEngine) {
                reply(event->sender(),
                      new ExecuteScriptResponse(
                          this, EventError("MongoDB Shell was not initialized or connection failure")));
                return;
            }

            if (_scriptEngine->failedScope()) {
                try {
                    _scriptEngine->init(_isLoadMongoRcJs);
                } catch (std::exception const &ex) {
                    sendLog(this, LogEvent::RBM_ERROR,
                            captilizeFirstChar(ex.what()) + ", cannot init mongo scope");
                }
            }

            MongoShellExecResult result{
                _scriptEngine->exec(event->script, _connSettings->defaultDatabase(), event->aggrInfo)};

            if (!result.error()) {
                reply(event->sender(),
                      new ExecuteScriptResponse(this, result, event->script.empty(),
                                                result.timeoutReached()));
                return;
            }

            retry(event);
        } catch (const std::exception &ex) {
            auto const error{EventError(ex.what(), EventError::Unknown)};
            reply(event->sender(), new ExecuteScriptResponse(this, error));
            sendLog(this, LogEvent::RBM_ERROR, ex.what());
        }
    }

    void MongoWorker::retry(ExecuteScriptRequest *event)
    {
        MongoShellExecResult const result{
            _scriptEngine->exec(event->script, _connSettings->defaultDatabase())};
        if (result.error()) {
            auto const error{EventError(result.errorMessage())};
            reply(event->sender(), new ExecuteScriptResponse(this, error));
        } else {
            reply(event->sender(),
                  new ExecuteScriptResponse(this, result, event->script.empty(),
                                            result.timeoutReached()));
        }
    }

    void MongoWorker::handle(StopScriptRequest *)
    {
        try {
            if (!_scriptEngine)
                return;
            _scriptEngine->interrupt();
        } catch (const std::exception &ex) {
            sendLog(this, LogEvent::RBM_ERROR, std::string(ex.what()));
        }
    }

    void MongoWorker::handle(AutocompleteRequest *event)
    {
        try {
            if (!_scriptEngine) {
                reply(event->sender(),
                      new AutocompleteResponse(
                          this, EventError("MongoDB Shell was not initialized")));
                return;
            }
            QStringList list = _scriptEngine->complete(event->prefix, event->mode);
            reply(event->sender(), new AutocompleteResponse(this, list, event->prefix));
        } catch (const std::exception &ex) {
            reply(event->sender(), new AutocompleteResponse(this, EventError(ex.what())));
            sendLog(this, LogEvent::RBM_ERROR, std::string(ex.what()));
        }
    }

    void MongoWorker::handle(CreateDatabaseRequest *event)
    {
        std::string dbname = event->database();
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->createDatabase(dbname);
            _createdDbs.insert(dbname);
            reply(event->sender(), new CreateDatabaseResponse(this, dbname));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new CreateDatabaseResponse(this, dbname, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(DropDatabaseRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->dropDatabase(event->database);
            _createdDbs.erase(event->database);
            reply(event->sender(), new DropDatabaseResponse(this, event->database));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new DropDatabaseResponse(this, event->database, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(CreateCollectionRequest *event)
    {
        std::string const &collection = event->ns().collectionName();
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->createCollection(event->ns().toString(), event->getSize(), event->getCapped(),
                                     event->getMaxDocNum(), event->getExtraOptions());
            client->done();
            reply(event->sender(), new CreateCollectionResponse(this, collection));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new CreateCollectionResponse(this, collection, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(DropCollectionRequest *event)
    {
        std::string const &collection = event->ns().collectionName();
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->dropCollection(event->ns());
            client->done();
            reply(event->sender(), new DropCollectionResponse(this, collection));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new DropCollectionResponse(this, collection, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(RenameCollectionRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->renameCollection(event->ns(), event->newCollection());
            client->done();
            reply(event->sender(),
                  new RenameCollectionResponse(this, event->ns().collectionName(),
                                               event->newCollection()));
        } catch (const std::exception &ex) {
            reply(event->sender(), new RenameCollectionResponse(this, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(DuplicateCollectionRequest *event)
    {
        std::string const &sourceCollection = event->ns().collectionName();
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->duplicateCollection(event->ns(), event->newCollection());
            client->done();
            reply(event->sender(),
                  new DuplicateCollectionResponse(this, sourceCollection, event->newCollection()));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new DuplicateCollectionResponse(this, sourceCollection, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(CopyCollectionToDiffServerRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            MongoWorker *cl = event->worker();
            client->copyCollectionToDiffServer(cl->mongoClient(), event->from(), event->to());
            client->done();
            reply(event->sender(), new CopyCollectionToDiffServerResponse(this));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new CopyCollectionToDiffServerResponse(this, EventError(ex.what())));
            sendLog(this, LogEvent::RBM_ERROR, std::string(ex.what()));
        }
    }

    void MongoWorker::handle(CreateUserRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->createUser(event->database(), event->user());
            client->done();
            reply(event->sender(), new CreateUserResponse(this, event->user().name()));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new CreateUserResponse(this, event->user().name(), EventError(ex.what())));
        }
    }

    void MongoWorker::handle(DropUserRequest *event)
    {
        try {
            std::unique_ptr<MongoClient> client(getClient());
            client->dropUser(event->database(), event->username());
            client->done();
            reply(event->sender(), new DropUserResponse(this, event->username()));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new DropUserResponse(this, event->username(), EventError(ex.what())));
        }
    }

    void MongoWorker::handle(CreateFunctionRequest *event)
    {
        std::string const &functionName = event->function().name();
        try {
            if (event->dbVersion() >= 3.4) {
                auto const cmd =
                    "db.system.js.save(" + event->function().toBson().toString() + ')';
                MongoShellExecResult const &result = _scriptEngine->exec(cmd, event->database());
                if (result.error())
                    throw std::runtime_error(result.errorMessage());
            } else {
                std::unique_ptr<MongoClient> client(getClient());
                client->createFunction(event->database(), event->function(),
                                       event->existingFunctionName());
                client->done();
            }
            reply(event->sender(), new CreateFunctionResponse(this, functionName));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new CreateFunctionResponse(this, functionName, EventError(ex.what())));
        }
    }

    void MongoWorker::handle(DropFunctionRequest *event)
    {
        try {
            if (event->dbVersion() >= 3.4) {
                auto const cmd =
                    "db.system.js.remove( { _id : \"" + event->functionName() + "\" } )";
                MongoShellExecResult const &result = _scriptEngine->exec(cmd, event->database());
                if (result.error())
                    throw std::runtime_error(result.errorMessage());
            } else {
                std::unique_ptr<MongoClient> client(getClient());
                client->dropFunction(event->database(), event->functionName());
                client->done();
            }
            reply(event->sender(), new DropFunctionResponse(this, event->functionName()));
        } catch (const std::exception &ex) {
            reply(event->sender(),
                  new DropFunctionResponse(this, event->functionName(), EventError(ex.what())));
        }
    }

    MongoClient *MongoWorker::getClient()
    {
        return new MongoClient(_client);
    }

    void MongoWorker::send(Event *event)
    {
        if (_isQuiting)
            return;
        AppRegistry::instance().bus()->send(this, event);
    }

    void MongoWorker::reply(QObject *receiver, Event *event)
    {
        if (_isQuiting)
            return;
        AppRegistry::instance().bus()->send(receiver, event);
    }

} // Robomongo
