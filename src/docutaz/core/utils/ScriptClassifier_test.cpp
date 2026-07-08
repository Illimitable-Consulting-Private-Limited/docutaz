#include "gtest/gtest.h"

#include "docutaz/core/utils/ScriptClassifier.h"

using Docutaz::ScriptClassifier::mayModifyData;
using Docutaz::ScriptClassifier::classify;
using WriteScope = Docutaz::ScriptClassifier::WriteScope;

// --- Reads should never be flagged -----------------------------------------

TEST(script_classifier, empty_or_whitespace_is_not_a_write)
{
    EXPECT_FALSE(mayModifyData(""));
    EXPECT_FALSE(mayModifyData("   \n\t  "));
}

TEST(script_classifier, plain_reads_are_not_writes)
{
    EXPECT_FALSE(mayModifyData("db.users.find({ name: 'x' })"));
    EXPECT_FALSE(mayModifyData("db.users.findOne({ _id: 1 })"));
    EXPECT_FALSE(mayModifyData("db.users.countDocuments({})"));
    EXPECT_FALSE(mayModifyData("db.users.aggregate([{ $match: { a: 1 } }, { $group: { _id: '$a' } }])"));
    EXPECT_FALSE(mayModifyData("db.getCollectionNames()"));
}

TEST(script_classifier, field_named_like_a_write_is_not_a_write)
{
    // "update" appears as data, not as a `.update(` call.
    EXPECT_FALSE(mayModifyData("db.audit.find({ update: true })"));
    EXPECT_FALSE(mayModifyData("var deleteCount = 5"));
}

// --- Writes should be flagged ----------------------------------------------

TEST(script_classifier, collection_writes_are_detected)
{
    EXPECT_TRUE(mayModifyData("db.users.insertOne({ a: 1 })"));
    EXPECT_TRUE(mayModifyData("db.users.insertMany([{ a: 1 }])"));
    EXPECT_TRUE(mayModifyData("db.users.updateMany({}, { $set: { a: 1 } })"));
    EXPECT_TRUE(mayModifyData("db.users.replaceOne({ _id: 1 }, { a: 2 })"));
    EXPECT_TRUE(mayModifyData("db.users.deleteMany({})"));
    EXPECT_TRUE(mayModifyData("db.users.remove({})"));
    EXPECT_TRUE(mayModifyData("db.users.save({ a: 1 })"));
    EXPECT_TRUE(mayModifyData("db.users.bulkWrite([])"));
    EXPECT_TRUE(mayModifyData("db.users.findOneAndUpdate({}, {})"));
    EXPECT_TRUE(mayModifyData("db.users.drop()"));
    EXPECT_TRUE(mayModifyData("db.users.createIndex({ a: 1 })"));
}

TEST(script_classifier, whitespace_between_dot_and_method_still_matches)
{
    EXPECT_TRUE(mayModifyData("db.users . deleteMany ({})"));
}

TEST(script_classifier, case_insensitive)
{
    EXPECT_TRUE(mayModifyData("db.users.DeleteOne({ _id: 1 })"));
}

TEST(script_classifier, aggregation_out_and_merge_stages_are_writes)
{
    EXPECT_TRUE(mayModifyData("db.users.aggregate([{ $match: {} }, { $out: 'archive' }])"));
    EXPECT_TRUE(mayModifyData("db.users.aggregate([{ $merge: { into: 'archive' } }])"));
}

// --- Write scope (single vs multi/mass) ------------------------------------

TEST(script_classifier, reads_have_no_write_scope)
{
    EXPECT_EQ(classify(""), WriteScope::None);
    EXPECT_EQ(classify("db.users.find({})"), WriteScope::None);
    EXPECT_EQ(classify("db.users.findOne({ _id: 1 })"), WriteScope::None);
}

TEST(script_classifier, single_document_writes_are_scope_single)
{
    EXPECT_EQ(classify("db.users.insertOne({ a: 1 })"), WriteScope::Single);
    EXPECT_EQ(classify("db.users.updateOne({}, { $set: { a: 1 } })"), WriteScope::Single);
    EXPECT_EQ(classify("db.users.replaceOne({ _id: 1 }, { a: 2 })"), WriteScope::Single);
    EXPECT_EQ(classify("db.users.deleteOne({ _id: 1 })"), WriteScope::Single);
    EXPECT_EQ(classify("db.users.save({ a: 1 })"), WriteScope::Single);
    EXPECT_EQ(classify("db.users.findOneAndUpdate({}, {})"), WriteScope::Single);
    EXPECT_EQ(classify("db.users.findOneAndDelete({})"), WriteScope::Single);
}

TEST(script_classifier, many_writes_are_scope_multi)
{
    EXPECT_EQ(classify("db.users.updateMany({}, { $set: { a: 1 } })"), WriteScope::Multi);
    EXPECT_EQ(classify("db.users.deleteMany({})"), WriteScope::Multi);
    EXPECT_EQ(classify("db.users.insertMany([{ a: 1 }])"), WriteScope::Multi);
    EXPECT_EQ(classify("db.users.bulkWrite([])"), WriteScope::Multi);
}

TEST(script_classifier, mass_and_structural_ops_are_scope_multi)
{
    EXPECT_EQ(classify("db.users.drop()"), WriteScope::Multi);
    EXPECT_EQ(classify("db.dropDatabase()"), WriteScope::Multi);
    EXPECT_EQ(classify("db.users.dropIndex('a_1')"), WriteScope::Multi);
    EXPECT_EQ(classify("db.users.createIndex({ a: 1 })"), WriteScope::Multi);
    EXPECT_EQ(classify("db.users.aggregate([{ $out: 'archive' }])"), WriteScope::Multi);
}

TEST(script_classifier, legacy_multi_capable_forms_are_scope_multi)
{
    // Bare legacy update()/remove()/insert() are multi-capable; err toward Multi.
    EXPECT_EQ(classify("db.users.update({}, { $set: { a: 1 } })"), WriteScope::Multi);
    EXPECT_EQ(classify("db.users.remove({})"), WriteScope::Multi);
    EXPECT_EQ(classify("db.users.insert([{ a: 1 }])"), WriteScope::Multi);
}

TEST(script_classifier, a_multi_write_anywhere_dominates_a_single_write)
{
    EXPECT_EQ(classify("db.a.updateOne({}, {}); db.b.deleteMany({})"), WriteScope::Multi);
}
