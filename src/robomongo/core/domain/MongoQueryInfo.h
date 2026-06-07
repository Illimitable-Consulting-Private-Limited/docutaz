#pragma once

#include <mongo/bson/bsonobj.h>
#include "robomongo/core/domain/MongoNamespace.h"

namespace Docutaz
{
    namespace detail
    {
        std::string prepareServerAddress(const std::string &address);
    }

    struct CollectionInfo
    {
        CollectionInfo();
        CollectionInfo(const std::string &server, const std::string &database, const std::string &collection);
        bool isValid() const;

        std::string _serverAddress;
        MongoNamespace _ns;
    };

    struct MongoQueryInfo
    {
        MongoQueryInfo();

        MongoQueryInfo(const CollectionInfo &info,
                  mongo::BSONObj query, mongo::BSONObj fields, int limit, int skip, int batchSize,
                  int options, bool special);

        // Setters used by MongoshEngine result parsing
        void setFilter(mongo::BSONObj q)     { _query = std::move(q); }
        void setProjection(mongo::BSONObj f) { _fields = std::move(f); }
        void setSort(mongo::BSONObj s)       { _sort = std::move(s); }
        void setBatchSize(int b)             { _batchSize = b; }
        void setSkip(int s)                  { _skip = s; }
        void setServerAddress(const std::string& addr) { _info._serverAddress = addr; }
        void setDatabaseName(const std::string& db) {
            _info._ns = MongoNamespace(db, _info._ns.collectionName());
        }
        void setCollectionName(const std::string& coll) {
            _info._ns = MongoNamespace(_info._ns.databaseName(), coll);
        }

        CollectionInfo _info;
        mongo::BSONObj _query;
        mongo::BSONObj _fields;
        mongo::BSONObj _sort;
        int _limit;
        int _skip;
        int _batchSize;
        int _options;
        bool _special; // flag, indicating that `query` contains special fields on
                      // first level, and query data in `query` field.

    };
}
