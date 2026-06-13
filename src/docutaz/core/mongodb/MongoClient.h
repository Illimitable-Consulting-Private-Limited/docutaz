#pragma once

#include <mongocxx/client.hpp>

#include <mongo/bson/bsonobj.h>
#include <mongo/client/dbclient_base.h>

#include "docutaz/core/Core.h"
#include "docutaz/core/domain/MongoQueryInfo.h"
#include "docutaz/core/domain/MongoUser.h"
#include "docutaz/core/domain/MongoFunction.h"
#include "docutaz/core/events/MongoEventsInfo.h"

namespace Docutaz
{
    class MongoClient
    {
    public:
        MongoClient(mongocxx::client& client);

        // Liveness check: runs {ping:1} on admin. Unlike getVersion()/
        // getStorageEngineType()/dbVersionStr() (which swallow exceptions and
        // return defaults), this DOES throw on connection failure, so callers can
        // detect an unreachable server before treating a connection as
        // established (mongocxx connects lazily on the first command).
        void ping() const;

        std::vector<std::string> getCollectionNamesWithDbname(const std::string &dbname) const;
        std::vector<std::string> getDatabaseNames() const;
        float getVersion() const;
        std::string dbVersionStr() const;
        std::string getStorageEngineType() const;

        std::vector<MongoUser> getUsers(const std::string &dbName);
        void createUser(const std::string &dbName, const MongoUser &user);
        void dropUser(const std::string &dbName, const std::string &user);

        std::vector<MongoFunction> getFunctions(const std::string &dbName) const;
        std::vector<IndexInfo> getIndexes(const MongoCollectionInfo &collection) const;
        void dropIndexFromCollection(const MongoCollectionInfo &collection, const std::string &indexName) const;
        void addEditIndex(const IndexInfo &oldInfo, const IndexInfo &newInfo) const;

        void renameIndexFromCollection(const MongoCollectionInfo &collection, const std::string &oldIndexName,
                                       const std::string &newIndexName) const;

        void createFunction(const std::string &dbName, const MongoFunction &fun,
                            const std::string &existingFunctionName = std::string());

        void dropFunction(const std::string &dbName, const std::string &name);
        void createDatabase(const std::string &dbName);
        void dropDatabase(const std::string &dbName);

        void createCollection(const std::string &ns, long long size, bool capped, int max,
                              const mongo::BSONObj &extraOptions);
        void renameCollection(const MongoNamespace &ns, const std::string &newCollectionName);
        void duplicateCollection(const MongoNamespace &ns, const std::string &newCollectionName);
        void dropCollection(const MongoNamespace &ns);
        void copyCollectionToDiffServer(mongocxx::client &fromClient, const MongoNamespace &from,
                                        const MongoNamespace &to);

        void insertDocument(const mongo::BSONObj &obj, const MongoNamespace &ns);
        void saveDocument(const mongo::BSONObj &obj, const MongoNamespace &ns);
        void removeDocuments(const MongoNamespace &ns, mongo::Query query, bool justOne = true);
        std::vector<MongoDocumentPtr> query(const MongoQueryInfo &info);

        MongoCollectionInfo runCollStatsCommand(const std::string &ns);
        std::vector<MongoCollectionInfo> runCollStatsCommand(const std::vector<std::string> &namespaces);

        void done();

    private:
        mongocxx::client& _client;
    };
}
