#include "gtest/gtest.h"

#include "docutaz/core/utils/BsonUtils.h"
#include "docutaz/core/utils/BsonBridge.h"
#include "docutaz/core/Enums.h"

#include <mongo/bson/bsonobj.h>

#include <string>

using namespace Docutaz;

// Regression tests for the BSON-array rendering use-after-free.
//
// BsonUtils::jsonString built its array iterator from a temporary:
//   BSONObjIterator i( elem.embeddedObject() );
// In the modernized mongo_compat layer BSONObj owns its bytes (shared_ptr) and
// embeddedObject() returns a copy, so the temporary's buffer was freed at the
// end of the statement and the iterator read freed heap. Nested arrays then
// rendered as garbage ObjectId(...) values (or malformed JSON) in both text
// mode and the "Copy JSON" action — both of which go through jsonString().

TEST(bson_utils_json, nested_array_of_doubles_renders_values_not_garbage)
{
    const mongo::BSONObj obj = BsonBridge::ejsonToBson(
        R"({"Location":{"type":"Point","coordinates":[88.4397817,22.6333446]}})");

    const std::string json =
        BsonUtils::jsonString(obj, mongo::TenGen, 1, DefaultEncoding, Utc);

    EXPECT_NE(json.find("88.4397817"), std::string::npos) << json;
    EXPECT_NE(json.find("22.6333446"), std::string::npos) << json;
    // The use-after-free surfaced as bogus ObjectId(...) tokens.
    EXPECT_EQ(json.find("ObjectId"), std::string::npos) << json;
}

TEST(bson_utils_json, top_level_array_of_strings_renders_values_not_garbage)
{
    const mongo::BSONObj obj =
        BsonBridge::ejsonToBson(R"({"Status":["Break Down"]})");

    const std::string json =
        BsonUtils::jsonString(obj, mongo::TenGen, 1, DefaultEncoding, Utc);

    EXPECT_NE(json.find("Break Down"), std::string::npos) << json;
    EXPECT_EQ(json.find("ObjectId"), std::string::npos) << json;
}

TEST(bson_utils_json, empty_array_renders_as_empty_brackets)
{
    const mongo::BSONObj obj = BsonBridge::ejsonToBson(R"({"a":[]})");
    const std::string json =
        BsonUtils::jsonString(obj, mongo::TenGen, 1, DefaultEncoding, Utc);
    EXPECT_NE(json.find("[]"), std::string::npos) << json;
}

// buildJsonString is the leaf-value renderer used by the tree model. Its array
// branch recurses through a function argument (kept alive for the call), so it
// was not affected by the bug, but pin the correct behavior anyway.
TEST(bson_utils_json, build_json_string_array_renders_values)
{
    const mongo::BSONObj obj =
        BsonBridge::ejsonToBson(R"({"coordinates":[88.4397817,22.6333446]})");

    std::string out;
    BsonUtils::buildJsonString(obj, out, DefaultEncoding, Utc);

    EXPECT_NE(out.find("88.4397817"), std::string::npos) << out;
    EXPECT_NE(out.find("22.6333446"), std::string::npos) << out;
    EXPECT_EQ(out.find("ObjectId"), std::string::npos) << out;
}

// Type predicates that gate which context-menu actions appear (Notifier):
// Copy Value is offered for simple types, Copy JSON for documents/arrays, etc.
TEST(bson_utils_types, simple_document_and_array_classification)
{
    using namespace mongo;
    // Simple (scalar) types -> eligible for "Copy Value".
    EXPECT_TRUE(BsonUtils::isSimpleType(String));
    EXPECT_TRUE(BsonUtils::isSimpleType(NumberDouble));
    EXPECT_TRUE(BsonUtils::isSimpleType(NumberInt));
    EXPECT_TRUE(BsonUtils::isSimpleType(jstOID));
    EXPECT_TRUE(BsonUtils::isSimpleType(Date));
    EXPECT_TRUE(BsonUtils::isSimpleType(Bool));

    // Containers are not "simple".
    EXPECT_FALSE(BsonUtils::isSimpleType(Object));
    EXPECT_FALSE(BsonUtils::isSimpleType(Array));

    // isDocument covers both embedded documents and arrays (both expandable
    // and both offer "Copy JSON"); isArray is the narrower check.
    EXPECT_TRUE(BsonUtils::isDocument(Object));
    EXPECT_TRUE(BsonUtils::isDocument(Array));
    EXPECT_FALSE(BsonUtils::isDocument(String));

    EXPECT_TRUE(BsonUtils::isArray(Array));
    EXPECT_FALSE(BsonUtils::isArray(Object));
    EXPECT_FALSE(BsonUtils::isArray(String));
}

// Regression test for the custom-UI / collection-stats routing. db.coll.stats()
// output is recognised by its ns + count + storageSize fields and sent to the
// dedicated stats panel; ordinary documents must not be misrouted there.
TEST(bson_utils_types, collection_stats_detection)
{
    // Minimal stats-shaped document.
    EXPECT_TRUE(BsonUtils::isCollectionStats(BsonBridge::ejsonToBson(
        R"({"ns":"db.c","count":42,"size":100,"storageSize":200,"nindexes":1})")));

    // Ordinary documents are not stats, even if they share one or two fields.
    EXPECT_FALSE(BsonUtils::isCollectionStats(
        BsonBridge::ejsonToBson(R"({"_id":"x","name":"y"})")));
    EXPECT_FALSE(BsonUtils::isCollectionStats(
        BsonBridge::ejsonToBson(R"({"ns":"db.c","count":42})")));  // no storageSize
    EXPECT_FALSE(BsonUtils::isCollectionStats(BsonBridge::ejsonToBson("{}")));
}
