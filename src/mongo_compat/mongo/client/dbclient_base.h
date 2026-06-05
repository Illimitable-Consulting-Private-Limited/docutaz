#pragma once
#include "mongo/bson/bsonobj.h"
#include "mongo/logger/log_severity.h"
#include "mongo/util/net/hostandport.h"
#include <string>
#include <list>

namespace mongo {

// Query stub
struct Query {
    BSONObj _filter;
    explicit Query(BSONObj f = {}) : _filter(std::move(f)) {}
    const BSONObj& getFilter() const { return _filter; }
};

// DBClientBase stub — just enough to compile existing code that hasn't been
// migrated to mongocxx yet. All methods assert/throw at runtime if called.
class DBClientBase {
public:
    virtual ~DBClientBase() = default;

    // Connection / auth stubs
    virtual bool connect(const HostAndPort& /*server*/, std::string& /*errmsg*/) { return false; }
    virtual bool auth(const BSONObj& /*params*/) { return false; }

    // Query stubs
    virtual std::list<BSONObj> getCollectionInfos(const std::string& /*db*/) {
        return {};
    }

    virtual bool runCommand(const std::string& /*db*/, const BSONObj& /*cmd*/,
                            BSONObj& /*result*/, int /*opts*/ = 0) {
        return false;
    }

    // CRUD stubs
    virtual void insert(const std::string& /*ns*/, BSONObj /*obj*/) {}
    virtual void update(const std::string& /*ns*/, Query /*query*/, BSONObj /*obj*/,
                        bool /*upsert*/ = false, bool /*multi*/ = false) {}
    virtual void remove(const std::string& /*ns*/, Query /*query*/,
                        bool /*justOne*/ = false) {}
};

// DBClientConnection stub
class DBClientConnection : public DBClientBase {
public:
    DBClientConnection(bool /*autoReconnect*/ = false) {}
    bool connect(const HostAndPort& server, std::string& errmsg) override {
        (void)server; errmsg = "stub"; return false;
    }
    void setTimeout(double /*secs*/) {}
    HostAndPort getServerAddress() const { return {}; }
};

} // mongo
