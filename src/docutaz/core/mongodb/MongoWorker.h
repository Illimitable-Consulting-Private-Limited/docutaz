#pragma once

#include <QObject>
#include <QMutex>
#include <unordered_set>

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>

#include "docutaz/core/events/MongoEvents.h"
#include "docutaz/core/mongodb/ReplicaSet.h"

QT_BEGIN_NAMESPACE
class QThread;
class QTimer;
QT_END_NAMESPACE

namespace Docutaz
{
    class MongoClient;
    class MongoshEngine;
    class ConnectionSettings;

    class MongoWorker : public QObject
    {
        Q_OBJECT

    public:
        explicit MongoWorker(ConnectionSettings *connection, bool isLoadMongoRcJs, int batchSize,
                             double mongoTimeoutSec, int shellTimeoutSec, QObject *parent = nullptr);

        ~MongoWorker();
        void interrupt();
        void stopAndDelete();
        void changeTimeout(int newTimeout);

    protected Q_SLOTS:

        void init();
        void keepAlive();

        bool handle(EstablishConnectionRequest *event);
        void handle(RefreshReplicaSetFolderRequest *event);
        void handle(LoadDatabaseNamesRequest *event);
        void handle(LoadCollectionNamesRequest *event);
        void handle(LoadUsersRequest *event);
        void handle(LoadCollectionIndexesRequest *event);
        void handle(AddEditIndexRequest *event);
        void handle(DropCollectionIndexRequest *event);
        void handle(LoadFunctionsRequest *event);
        void handle(InsertDocumentRequest *event);
        void handle(RemoveDocumentRequest *event);
        void handle(ExecuteQueryRequest *event);
        void handle(ExecuteScriptRequest *event);
        void retry(ExecuteScriptRequest *event);
        void handle(StopScriptRequest *event);
        void handle(AutocompleteRequest *event);
        void handle(CreateDatabaseRequest *event);
        void handle(DropDatabaseRequest *event);
        void handle(CreateCollectionRequest *event);
        void handle(DropCollectionRequest *event);
        void handle(RenameCollectionRequest *event);
        void handle(DuplicateCollectionRequest *event);
        void handle(CopyCollectionToDiffServerRequest *event);
        void handle(CreateUserRequest *event);
        void handle(DropUserRequest *event);
        void handle(CreateFunctionRequest *event);
        void handle(DropFunctionRequest *event);

    protected:
        virtual void timerEvent(QTimerEvent *);

    public:
        mongocxx::client& mongoClient() { return _client; }

    private:
        void send(Event *event);

        std::vector<std::string> getDatabaseNamesSafe(EstablishConnectionRequest* event = nullptr);
        std::string getAuthBase() const;

        MongoClient* getClient();

        std::string buildConnectionUri() const;
        ReplicaSet getReplicaSetInfo() const;

        void reply(QObject *receiver, Event *event);

        QThread *_thread;
        QMutex _firstConnectionMutex;

        std::unique_ptr<MongoshEngine> _scriptEngine;

        const bool _isLoadMongoRcJs;
        const int _batchSize;
        int _timerId;
        int _dbAutocompleteCacheTimerId;
        double _mongoTimeoutSec;
        int _shellTimeoutSec;
        QAtomicInteger<int> _isQuiting;

        mongocxx::client _client;

        ConnectionSettings *_connSettings;

        std::unordered_set<std::string> _createdDbs;
    };

}
