#include "gtest/gtest.h"

#include "docutaz/core/QueryHistory.h"

using Docutaz::QueryHistoryManager;

// Pins the dedup-key behaviour: different filter VALUES stay distinct (only an
// identical re-run collapses), and entries are scoped per connection + database.

TEST(query_history, normalize_trims_and_collapses_runs)
{
    EXPECT_EQ(QueryHistoryManager::normalizeQuery("  db.c.find({age:25})  \n"),
              QString("db.c.find({age:25})"));
    // runs of whitespace (newlines/indent/double spaces) collapse to one space...
    EXPECT_EQ(QueryHistoryManager::normalizeQuery("db.c\n  .find(   {age:25} )"),
              QString("db.c .find( {age:25} )"));
}

TEST(query_history, dedup_key_distinguishes_filter_values)
{
    // Different filter -> different entry (we never merge by "shape").
    EXPECT_NE(QueryHistoryManager::dedupKey("db.users.find({age:25})", "Prod", "sam"),
              QueryHistoryManager::dedupKey("db.users.find({age:30})", "Prod", "sam"));
}

TEST(query_history, dedup_key_ignores_reformatting)
{
    // Same query, just reindented / trailing newline -> same entry.
    EXPECT_EQ(QueryHistoryManager::dedupKey("db.users.find({age:25})", "Prod", "sam"),
              QueryHistoryManager::dedupKey("  db.users.find({age:25})\n", "Prod", "sam"));
}

TEST(query_history, dedup_key_scopes_by_connection_and_database)
{
    const QString q = "db.users.find({age:25})";
    EXPECT_EQ(QueryHistoryManager::dedupKey(q, "Prod", "sam"),
              QueryHistoryManager::dedupKey(q, "Prod", "sam"));   // identical
    EXPECT_NE(QueryHistoryManager::dedupKey(q, "Prod", "sam"),
              QueryHistoryManager::dedupKey(q, "Staging", "sam")); // different connection
    EXPECT_NE(QueryHistoryManager::dedupKey(q, "Prod", "sam"),
              QueryHistoryManager::dedupKey(q, "Prod", "other"));  // different database
}

TEST(query_history, derive_kind)
{
    EXPECT_EQ(QueryHistoryManager::deriveKind("db.c.aggregate([{$match:{}}])"), "aggregate");
    EXPECT_EQ(QueryHistoryManager::deriveKind("db.c.find({})"),                  "find");
    EXPECT_EQ(QueryHistoryManager::deriveKind("db.c.updateOne({},{$set:{}})"),   "update");
    EXPECT_EQ(QueryHistoryManager::deriveKind("db.c.deleteMany({})"),            "delete");
    EXPECT_EQ(QueryHistoryManager::deriveKind("db.c.insertOne({})"),             "insert");
    EXPECT_EQ(QueryHistoryManager::deriveKind("db.c.countDocuments({})"),        "count");
    EXPECT_EQ(QueryHistoryManager::deriveKind("db.serverStatus()"),             "other");
}

TEST(query_history, collections_of)
{
    // getCollection('x'), db.x. and db['x'] forms; shell helpers excluded; deduped.
    EXPECT_EQ(QueryHistoryManager::collectionsOf("db.getCollection('user').find({})"),
              (QStringList{"user"}));
    EXPECT_EQ(QueryHistoryManager::collectionsOf("db.orders.aggregate([])"),
              (QStringList{"orders"}));
    EXPECT_EQ(QueryHistoryManager::collectionsOf("db['my-coll'].find({})"),
              (QStringList{"my-coll"}));
    // Multiple distinct collections, first-seen order, helper db.getSiblingDB ignored.
    EXPECT_EQ(QueryHistoryManager::collectionsOf(
                  "var x = db.getCollection('company').findOne({}); db.order.find({c:x._id})"),
              (QStringList{"company", "order"}));
    // db helper methods are not collections.
    EXPECT_TRUE(QueryHistoryManager::collectionsOf("db.runCommand({ping:1})").isEmpty());
}

TEST(query_history, is_script)
{
    // Single statements (incl. multi-line / trailing semicolon) are NOT scripts.
    EXPECT_FALSE(QueryHistoryManager::isScript("db.user.find({})"));
    EXPECT_FALSE(QueryHistoryManager::isScript("db.user.find({});"));
    EXPECT_FALSE(QueryHistoryManager::isScript("db.user.aggregate([\n  {$match:{}}\n])"));
    // Multiple statements (semicolon- or newline-separated) ARE scripts.
    EXPECT_TRUE(QueryHistoryManager::isScript(
                    "var c = db.company.findOne({}); db.order.find({c:c._id})"));
    EXPECT_TRUE(QueryHistoryManager::isScript("db.a.find({})\ndb.b.find({})"));
}
