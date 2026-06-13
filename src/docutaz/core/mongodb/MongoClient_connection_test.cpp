#include "gtest/gtest.h"

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include "docutaz/core/mongodb/MongoClient.h"

// Regression guard for the "connect to an unavailable server" handling.
//
// Bug: an authenticated connection to a DOWN server was reported as successful
// (no "Cannot connect" error dialog, only a misleading "manually specify
// visible databases" hint). Root cause: getVersion()/getStorageEngineType()/
// dbVersionStr() swallow exceptions and return defaults, and the worker inferred
// connection success from a non-empty database list — which always contained the
// auth database. So nothing detected the dead server.
//
// The fix added MongoClient::ping(), a liveness check that THROWS on failure,
// and the worker now calls it before treating a connection as established. These
// tests pin both halves of that contract so the regression can't return silently.

using Docutaz::MongoClient;

namespace {
    // mongocxx requires exactly one instance per process, created before any
    // client and outliving them all. A function-local static gives us that
    // without fighting gtest_main over a custom main().
    void ensureMongocxxInstance() {
        static mongocxx::instance instance{};
    }

    // An address that reliably refuses: port 1 is never a mongod. directConnection
    // skips topology discovery and the short selection timeout bounds the wait, so
    // the failure is fast and deterministic (connection refused, not a hang).
    mongocxx::client unreachableClient() {
        return mongocxx::client(mongocxx::uri(
            "mongodb://127.0.0.1:1/"
            "?directConnection=true&serverSelectionTimeoutMS=1500&connectTimeoutMS=1500"));
    }
}

// ping() MUST throw on an unreachable server — this is what lets the worker fail
// the connection cleanly and surface a proper error instead of a false success.
TEST(connection_failure, ping_throws_on_unreachable_server)
{
    ensureMongocxxInstance();
    mongocxx::client client = unreachableClient();
    MongoClient mc(client);
    EXPECT_THROW(mc.ping(), std::exception);
}

// getVersion() must NOT throw (it swallows and returns 0) — this is precisely why
// a dedicated throwing ping() is necessary. If this ever started throwing, the
// rationale for ping() would change; if ping() ever started swallowing like this,
// the dead-server regression would silently return.
TEST(connection_failure, getVersion_swallows_errors_on_unreachable_server)
{
    ensureMongocxxInstance();
    mongocxx::client client = unreachableClient();
    MongoClient mc(client);
    float version = 1.0f;
    EXPECT_NO_THROW(version = mc.getVersion());
    EXPECT_FLOAT_EQ(version, 0.0f);
}
