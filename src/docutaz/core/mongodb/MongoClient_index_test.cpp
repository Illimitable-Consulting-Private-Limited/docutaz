#include "gtest/gtest.h"

#include <string>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/types.hpp>

#include "docutaz/core/mongodb/MongoClient.h"
#include "docutaz/core/mongodb/MongoTestSupport.h"
#include "docutaz/core/domain/MongoCollectionInfo.h"
#include "docutaz/core/events/MongoEventsInfo.h"

// Live integration tests for index create/edit (MongoClient::addEditIndex +
// getIndexes). They run against a mongod on localhost:27017 and confine ALL
// writes to the scratch `tmp` database (collection tmp.docutaz_index_it), which
// each test drops on entry and exit. If no local mongod is reachable, every test
// SKIPS (so CI without a database stays green rather than red).
//
// Coverage: single/compound/ascending/descending keys, unique, sparse, TTL,
// text (with weights + language), 2dsphere, hashed, partial indexes, the
// unquoted-shell-key regression ({ sid: 1 }), edit (drop+recreate) of both keys
// and options, edit-failure recovery, and server-error propagation.

using Docutaz::MongoClient;
using Docutaz::IndexInfo;
using Docutaz::MongoCollectionInfo;

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

namespace
{
    const std::string kDb   = "tmp";
    const std::string kColl = "docutaz_index_it";
    const std::string kNs   = "tmp.docutaz_index_it";

    // Numeric index-direction values may come back as int32/int64/double
    // depending on how the shell literal was parsed and stored; compare as double.
    double numOf(bsoncxx::document::element e)
    {
        switch (e.type()) {
        case bsoncxx::type::k_int32:  return e.get_int32().value;
        case bsoncxx::type::k_int64:  return e.get_int64().value;
        case bsoncxx::type::k_double: return e.get_double().value;
        default: return 0.0;
        }
    }
}

class MongoIndexTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        docutaz_test::ensureMongocxxInstance();
        _client = mongocxx::client(mongocxx::uri(
            "mongodb://127.0.0.1:27017/"
            "?directConnection=true&serverSelectionTimeoutMS=1500&connectTimeoutMS=1500"));

        // Skip the whole suite when there's no local mongod, instead of failing.
        try {
            _client[kDb].run_command(make_document(kvp("ping", 1)).view());
        } catch (const std::exception &ex) {
            GTEST_SKIP() << "no local mongod on 127.0.0.1:27017 (" << ex.what() << ")";
        }

        coll().drop();   // start from a clean slate
    }

    void TearDown() override
    {
        try { coll().drop(); } catch (...) {}
    }

    mongocxx::collection coll() { return _client[kDb][kColl]; }

    MongoClient mc() { return MongoClient(_client); }

    // Create an index via the code under test. `newInfo` carries the spec; an
    // empty oldInfo means "add" (no drop-first).
    void createIndex(const IndexInfo &newInfo)
    {
        mc().addEditIndex(IndexInfo(MongoCollectionInfo(kNs)), newInfo);
    }

    // A fresh IndexInfo bound to the test collection.
    IndexInfo info(const std::string &name, const std::string &keys)
    {
        return IndexInfo(MongoCollectionInfo(kNs), name, keys);
    }

    // Raw server-side spec for a named index (ground truth), or an empty
    // optional if no such index exists.
    bsoncxx::stdx::optional<bsoncxx::document::value> spec(const std::string &name)
    {
        auto cursor = coll().indexes().list();
        for (auto &&doc : cursor) {
            auto n = doc["name"];
            if (n && std::string(n.get_string().value) == name)
                return bsoncxx::document::value(doc);
        }
        return {};
    }

    // The IndexInfo the app would show for a named index (exercises the
    // read-back path makeIndexInfoFromBsonObj).
    bsoncxx::stdx::optional<IndexInfo> readBack(const std::string &name)
    {
        for (const IndexInfo &i : mc().getIndexes(MongoCollectionInfo(kNs)))
            if (i._name == name)
                return i;
        return {};
    }

    mongocxx::client _client;
};

// --- keys ------------------------------------------------------------------

TEST_F(MongoIndexTest, create_single_ascending)
{
    createIndex(info("idx_a", "{ a: 1 }"));

    auto s = spec("idx_a");
    ASSERT_TRUE(s);
    auto key = (*s).view()["key"].get_document().view();
    EXPECT_DOUBLE_EQ(numOf(key["a"]), 1.0);
}

TEST_F(MongoIndexTest, create_descending)
{
    createIndex(info("idx_desc", "{ a: -1 }"));

    auto s = spec("idx_desc");
    ASSERT_TRUE(s);
    EXPECT_DOUBLE_EQ(numOf((*s).view()["key"].get_document().view()["a"]), -1.0);
}

TEST_F(MongoIndexTest, create_compound)
{
    createIndex(info("idx_ab", "{ a: 1, b: -1 }"));

    auto s = spec("idx_ab");
    ASSERT_TRUE(s);
    auto key = (*s).view()["key"].get_document().view();
    EXPECT_DOUBLE_EQ(numOf(key["a"]),  1.0);
    EXPECT_DOUBLE_EQ(numOf(key["b"]), -1.0);
}

// The regression this test suite was born from: the index dialog validates keys
// with the lenient shell parser, so unquoted field names ({ sid: 1 }) reach
// addEditIndex. It must accept them (strict JSON would reject "sid").
TEST_F(MongoIndexTest, create_unquoted_shell_keys)
{
    ASSERT_NO_THROW(createIndex(info("idx_sid", "{ sid: 1 }")));

    auto s = spec("idx_sid");
    ASSERT_TRUE(s);
    EXPECT_DOUBLE_EQ(numOf((*s).view()["key"].get_document().view()["sid"]), 1.0);
}

// --- options ---------------------------------------------------------------

TEST_F(MongoIndexTest, create_unique)
{
    IndexInfo i(MongoCollectionInfo(kNs), "idx_email", "{ email: 1 }", /*unique*/ true);
    createIndex(i);

    auto s = spec("idx_email");
    ASSERT_TRUE(s);
    ASSERT_TRUE((*s).view()["unique"]);
    EXPECT_TRUE((*s).view()["unique"].get_bool().value);

    auto rb = readBack("idx_email");
    ASSERT_TRUE(rb);
    EXPECT_TRUE(rb->_unique);
}

TEST_F(MongoIndexTest, create_sparse)
{
    IndexInfo i(MongoCollectionInfo(kNs), "idx_sparse", "{ opt: 1 }",
                /*unique*/ false, /*background*/ false, /*sparse*/ true);
    createIndex(i);

    auto s = spec("idx_sparse");
    ASSERT_TRUE(s);
    ASSERT_TRUE((*s).view()["sparse"]);
    EXPECT_TRUE((*s).view()["sparse"].get_bool().value);
}

TEST_F(MongoIndexTest, create_ttl)
{
    IndexInfo i(MongoCollectionInfo(kNs), "idx_ttl", "{ createdAt: 1 }",
                false, false, false, /*expireAfter*/ 3600);
    createIndex(i);

    auto s = spec("idx_ttl");
    ASSERT_TRUE(s);
    ASSERT_TRUE((*s).view()["expireAfterSeconds"]);
    EXPECT_DOUBLE_EQ(numOf((*s).view()["expireAfterSeconds"]), 3600.0);
}

TEST_F(MongoIndexTest, create_text_with_weights_and_language)
{
    IndexInfo i(MongoCollectionInfo(kNs), "idx_text",
                "{ title: \"text\", body: \"text\" }",
                false, false, false, -1,
                /*defaultLanguage*/ "english", /*languageOverride*/ "",
                /*textWeights*/ "{ title: 10, body: 1 }");
    createIndex(i);

    auto s = spec("idx_text");
    ASSERT_TRUE(s);
    ASSERT_TRUE((*s).view()["weights"]);
    auto w = (*s).view()["weights"].get_document().view();
    EXPECT_DOUBLE_EQ(numOf(w["title"]), 10.0);
    EXPECT_DOUBLE_EQ(numOf(w["body"]),   1.0);
    ASSERT_TRUE((*s).view()["default_language"]);
    EXPECT_EQ(std::string((*s).view()["default_language"].get_string().value), "english");
}

TEST_F(MongoIndexTest, create_2dsphere)
{
    createIndex(info("idx_geo", "{ loc: \"2dsphere\" }"));

    auto s = spec("idx_geo");
    ASSERT_TRUE(s);
    EXPECT_EQ(std::string((*s).view()["key"].get_document().view()["loc"].get_string().value),
              "2dsphere");
}

TEST_F(MongoIndexTest, create_hashed)
{
    createIndex(info("idx_hash", "{ h: \"hashed\" }"));

    auto s = spec("idx_hash");
    ASSERT_TRUE(s);
    EXPECT_EQ(std::string((*s).view()["key"].get_document().view()["h"].get_string().value),
              "hashed");
}

// --- partial indexes -------------------------------------------------------

TEST_F(MongoIndexTest, create_partial)
{
    IndexInfo i(MongoCollectionInfo(kNs), "idx_partial", "{ qty: 1 }",
                false, false, false, -1, "", "", "",
                /*partialFilterExpression*/ "{ qty: { $gt: 0 } }");
    createIndex(i);

    auto s = spec("idx_partial");
    ASSERT_TRUE(s);
    ASSERT_TRUE((*s).view()["partialFilterExpression"]);
    auto pfe = (*s).view()["partialFilterExpression"].get_document().view();
    ASSERT_TRUE(pfe["qty"]);   // filter round-tripped to the server

    // ...and the read-back path surfaces it back to the app (not empty).
    auto rb = readBack("idx_partial");
    ASSERT_TRUE(rb);
    EXPECT_FALSE(rb->_partialFilterExpression.empty());
}

// Partial filter written in shell syntax (unquoted keys) must also be accepted,
// same lenient-parser contract as the keys.
TEST_F(MongoIndexTest, create_partial_unquoted_filter)
{
    IndexInfo i(MongoCollectionInfo(kNs), "idx_partial2", "{ score: 1 }",
                false, false, false, -1, "", "", "",
                "{ score: { $gte: 50 } }");
    ASSERT_NO_THROW(createIndex(i));

    auto s = spec("idx_partial2");
    ASSERT_TRUE(s);
    EXPECT_TRUE((*s).view()["partialFilterExpression"]);
}

// --- edit (drop + recreate) ------------------------------------------------

TEST_F(MongoIndexTest, edit_changes_keys)
{
    createIndex(info("idx_edit", "{ a: 1 }"));
    ASSERT_TRUE(spec("idx_edit"));

    // Edit: same name, different keys. addEditIndex drops the old then recreates.
    mc().addEditIndex(info("idx_edit", "{ a: 1 }"), info("idx_edit", "{ b: 1 }"));

    auto s = spec("idx_edit");
    ASSERT_TRUE(s);
    auto key = (*s).view()["key"].get_document().view();
    EXPECT_FALSE(key["a"]);                       // old key gone
    EXPECT_DOUBLE_EQ(numOf(key["b"]), 1.0);       // new key present
}

TEST_F(MongoIndexTest, edit_changes_options)
{
    IndexInfo uniqueIdx(MongoCollectionInfo(kNs), "idx_opt", "{ u: 1 }", /*unique*/ true);
    createIndex(uniqueIdx);
    ASSERT_TRUE(spec("idx_opt"));

    // Edit to a plain (non-unique) index on the same key/name.
    IndexInfo plainIdx(MongoCollectionInfo(kNs), "idx_opt", "{ u: 1 }", /*unique*/ false);
    mc().addEditIndex(uniqueIdx, plainIdx);

    auto s = spec("idx_opt");
    ASSERT_TRUE(s);
    EXPECT_FALSE((*s).view()["unique"]);   // unique flag dropped
}

// When the recreate step of an edit fails, the original index must be restored
// (best-effort recovery) and the error surfaced to the caller.
TEST_F(MongoIndexTest, edit_failure_recovers_old_index)
{
    createIndex(info("idx_recover", "{ a: 1 }"));
    ASSERT_TRUE(spec("idx_recover"));

    // New spec has an invalid index type -> the server rejects create_index.
    IndexInfo bad = info("idx_recover", "{ a: \"bogus_index_type\" }");
    EXPECT_THROW(mc().addEditIndex(info("idx_recover", "{ a: 1 }"), bad), std::exception);

    // The original index survived the failed edit.
    auto s = spec("idx_recover");
    ASSERT_TRUE(s);
    EXPECT_DOUBLE_EQ(numOf((*s).view()["key"].get_document().view()["a"]), 1.0);
}

// --- error propagation -----------------------------------------------------

// A unique index over data that already contains duplicates must fail loudly
// (the worker relies on addEditIndex throwing to show the error dialog).
TEST_F(MongoIndexTest, unique_over_duplicate_data_throws)
{
    coll().insert_one(make_document(kvp("email", "dup@example.com")).view());
    coll().insert_one(make_document(kvp("email", "dup@example.com")).view());

    IndexInfo i(MongoCollectionInfo(kNs), "idx_dup", "{ email: 1 }", /*unique*/ true);
    EXPECT_THROW(createIndex(i), std::exception);
}
