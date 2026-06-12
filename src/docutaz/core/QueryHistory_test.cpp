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
