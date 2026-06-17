#include <gtest/gtest.h>

#include "docutaz/gui/editors/JsBeautifier.h"

using Docutaz::JsBeautifier;

namespace
{
    // Default indent in these tests matches the editor (4 spaces).
    std::string fmt(const std::string& s) { return JsBeautifier::format(s); }
}

// Queries that fit on one line are only whitespace-normalised (indent width is
// irrelevant for these, so the expected output is exact and deterministic).
TEST(JsBeautifier, NormalisesShortQueries)
{
    EXPECT_EQ(fmt("db.t.find({a:1})"), "db.t.find({ a: 1 })");
    EXPECT_EQ(fmt("db.coll.find( {  a :1,b:   2 } )\n.sort({a:1})"),
              "db.coll.find({ a: 1, b: 2 }).sort({ a: 1 })");
}

TEST(JsBeautifier, PreservesObjectIdRegexAndDate)
{
    EXPECT_EQ(fmt(R"(db.users.find({_id:ObjectId("5f9d88b9c1a2b3c4d5e6f7a8")}))"),
              R"(db.users.find({ _id: ObjectId("5f9d88b9c1a2b3c4d5e6f7a8") }))");
    EXPECT_EQ(fmt("db.products.find({name:/^widget/i,price:{$lt:100}})"),
              "db.products.find({ name: /^widget/i, price: { $lt: 100 } })");
    EXPECT_EQ(fmt("db.c.insertOne({ts:new Date()})"),
              "db.c.insertOne({ ts: new Date() })");
}

TEST(JsBeautifier, KeepsTrailingCommentInline)
{
    EXPECT_EQ(fmt("db.t.find({a:1}) // find everything"),
              "db.t.find({ a: 1 }) // find everything");
}

TEST(JsBeautifier, SplitsTopLevelStatements)
{
    EXPECT_EQ(fmt("use analytics\ndb.events.insertOne({type:\"click\",meta:{x:1,y:2}})"),
              "use analytics\ndb.events.insertOne({ type: \"click\", meta: { x: 1, y: 2 } })");
}

// A long method chain breaks one .call() per line, indented one level.
TEST(JsBeautifier, BreaksLongMethodChain)
{
    const std::string in =
        R"(db.users.find({name:"foo",age:{$gt:21,$lt:65},tags:["a","b","c"]}).sort({age:-1}).limit(10))";
    const std::string out = fmt(in);
    EXPECT_NE(out.find("\n    .sort({ age: -1 })"), std::string::npos) << out;
    EXPECT_NE(out.find("\n    .limit(10)"), std::string::npos) << out;
}

// A call whose sole argument is an array "hugs" the parenthesis: aggregate([ ...
TEST(JsBeautifier, HugsSingleArrayArgument)
{
    const std::string in =
        R"(db.orders.aggregate([{$match:{status:"A"}},{$group:{_id:"$cust_id",total:{$sum:"$amount"},count:{$sum:1}}},{$sort:{total:-1}},{$limit:5}]))";
    const std::string out = fmt(in);
    EXPECT_EQ(out.rfind("db.orders.aggregate([\n", 0), 0u) << out;  // starts with hug
    EXPECT_NE(out.find("\n])"), std::string::npos) << out;
}

// Formatting is idempotent: formatting the output again changes nothing.
TEST(JsBeautifier, Idempotent)
{
    const std::vector<std::string> samples = {
        R"(db.users.find({name:"foo",age:{$gt:21,$lt:65},tags:["a","b","c"]}).sort({age:-1}).limit(10))",
        R"(db.orders.aggregate([{$match:{status:"A",qty:{$gte:10}}},{$group:{_id:"$cust_id",total:{$sum:"$amount"}}},{$sort:{total:-1}}]))",
        R"(db.coll.updateMany({status:"active"},{$set:{verified:true,updatedAt:new Date()}},{upsert:false}))",
        R"(db.inventory.insertMany([{item:"journal",qty:25,tags:["blank","red"]},{item:"mat",qty:85}]))",
        "use analytics\ndb.events.insertOne({type:\"click\",meta:{x:1,y:2}})",
    };
    for (const auto& s : samples) {
        const std::string once = fmt(s);
        EXPECT_EQ(fmt(once), once) << "not idempotent for: " << s;
    }
}

// Malformed input (unbalanced brackets, unterminated string) must be returned
// verbatim rather than corrupted.
TEST(JsBeautifier, ReturnsMalformedInputUnchanged)
{
    EXPECT_EQ(fmt("db.t.find({a:1)"), "db.t.find({a:1)");          // mismatched
    EXPECT_EQ(fmt("db.t.find({a:1}"), "db.t.find({a:1}");          // missing ')'
    EXPECT_EQ(fmt("db.t.find({a:\"unterminated"), "db.t.find({a:\"unterminated");
}

TEST(JsBeautifier, EmptyInput)
{
    EXPECT_EQ(fmt(""), "");
}
