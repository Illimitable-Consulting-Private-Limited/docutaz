#include "gtest/gtest.h"

#include "docutaz/core/utils/BsonBridge.h"

#include <mongo/bson/bsonobj.h>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/types.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace Docutaz;

// Regression test for the document field-order fix.
//
// mongosh results used to be round-tripped through Qt's QJsonObject, which
// sorts object keys alphabetically — so documents displayed in sorted key
// order instead of MongoDB's stored order. The fix feeds the EJSON straight to
// BsonBridge::ejsonToBson (bsoncxx), which preserves parse order. This pins
// that guarantee (the JS-side half of the fix needs a live mongosh and is not
// unit-testable here).

namespace {
std::vector<std::string> fieldOrder(const mongo::BSONObj& obj)
{
    std::vector<std::string> names;
    mongo::BSONObjIterator it(obj);
    while (it.more())
        names.push_back(it.next().fieldName());
    return names;
}
}

TEST(bson_bridge, preserves_top_level_field_order)
{
    // Deliberately non-alphabetical input.
    const mongo::BSONObj obj = BsonBridge::ejsonToBson(
        R"({"_id":"x","companyId":"y","version":1,"Name":"n","Location":{}})");

    const std::vector<std::string> expected{
        "_id", "companyId", "version", "Name", "Location"};
    EXPECT_EQ(fieldOrder(obj), expected);
}

TEST(bson_bridge, preserves_nested_field_order)
{
    const mongo::BSONObj obj = BsonBridge::ejsonToBson(
        R"({"Location":{"type":"Point","coordinates":[1,2]}})");

    const mongo::BSONObj loc = obj.getField("Location").embeddedObject();
    const std::vector<std::string> expected{"type", "coordinates"};
    EXPECT_EQ(fieldOrder(loc), expected);
}

// ── Round-trip type coverage ─────────────────────────────────────────────────
//
// Every EJSON type mongosh can emit must survive the
//   bsoncxx canonical EJSON  ->  BsonBridge::ejsonToBson  ->  mongo::BSONObj
// round-trip with byte-identical BSON. (Ported from the old standalone
// src/tests/bson_bridge_roundtrip.cpp so it runs as part of the suite.)

namespace {
using namespace bsoncxx::builder::basic;
using bsoncxx::document::value;

// bsoncxx value -> canonical EJSON -> BSONObj, then compare raw BSON bytes.
::testing::AssertionResult roundTrips(const value& original)
{
    const std::string ejson =
        bsoncxx::to_json(original.view(), bsoncxx::ExtendedJsonMode::k_canonical);
    mongo::BSONObj obj;
    try {
        obj = BsonBridge::ejsonToBson(ejson);
    } catch (const std::exception& e) {
        return ::testing::AssertionFailure() << "ejsonToBson threw: " << e.what()
                                             << " for " << ejson;
    }
    const std::size_t origLen = original.view().length();
    const std::size_t bsonLen = static_cast<std::size_t>(obj.objsize());
    if (origLen != bsonLen)
        return ::testing::AssertionFailure()
               << "size mismatch: " << origLen << " vs " << bsonLen << " for " << ejson;
    if (std::memcmp(original.view().data(), obj.objdata(), origLen) != 0)
        return ::testing::AssertionFailure() << "byte mismatch for " << ejson;
    return ::testing::AssertionSuccess();
}

// EJSON -> BSON -> EJSON, then compare the reparsed BSON bytes. Exercises the
// mongosh EJSON forms (relaxed $date string, $binary v2, $numberDecimal, ...).
::testing::AssertionResult ejsonRoundTrips(const std::string& ejson)
{
    try {
        const mongo::BSONObj obj = BsonBridge::ejsonToBson(ejson);
        const std::string back  = BsonBridge::bsonToEjson(obj);
        const auto v1 = bsoncxx::from_json(ejson);
        const auto v2 = bsoncxx::from_json(back);
        if (v1.view().length() != v2.view().length())
            return ::testing::AssertionFailure() << "size mismatch; back=" << back;
        if (std::memcmp(v1.view().data(), v2.view().data(), v1.view().length()) != 0)
            return ::testing::AssertionFailure() << "byte mismatch; back=" << back;
        return ::testing::AssertionSuccess();
    } catch (const std::exception& e) {
        return ::testing::AssertionFailure() << "exception: " << e.what();
    }
}
}  // namespace

TEST(bson_bridge_roundtrip, scalars)
{
    EXPECT_TRUE(roundTrips(make_document(kvp("k", "hello world"))));
    EXPECT_TRUE(roundTrips(make_document(kvp("k", std::int32_t{42}))));
    // beyond JS Number.MAX_SAFE_INTEGER
    EXPECT_TRUE(roundTrips(make_document(kvp("k", std::int64_t{9007199254740993LL}))));
    EXPECT_TRUE(roundTrips(make_document(kvp("k", 3.14159265358979))));
    EXPECT_TRUE(roundTrips(make_document(kvp("t", true), kvp("f", false))));
    EXPECT_TRUE(roundTrips(make_document(kvp("k", bsoncxx::types::b_null{}))));
}

TEST(bson_bridge_roundtrip, objectid_date_timestamp)
{
    EXPECT_TRUE(roundTrips(make_document(kvp("_id", bsoncxx::oid{"507f1f77bcf86cd799439011"}))));
    EXPECT_TRUE(roundTrips(make_document(
        kvp("d", bsoncxx::types::b_date{std::chrono::milliseconds{1704067200000LL}}))));
    // mongosh relaxed EJSON emits dates as ISO strings
    EXPECT_TRUE(ejsonRoundTrips(R"({"d":{"$date":"2024-01-01T00:00:00.000Z"}})"));
    EXPECT_TRUE(roundTrips(make_document(kvp("t", bsoncxx::types::b_timestamp{1, 1704067200}))));
    EXPECT_TRUE(ejsonRoundTrips(R"({"t":{"$timestamp":{"t":1704067200,"i":1}}})"));
}

TEST(bson_bridge_roundtrip, binary)
{
    const std::uint8_t bytes[] = {0x68, 0x65, 0x6c, 0x6c, 0x6f};
    bsoncxx::types::b_binary bin;
    bin.sub_type = bsoncxx::binary_sub_type::k_binary;
    bin.bytes    = bytes;
    bin.size     = sizeof(bytes);
    EXPECT_TRUE(roundTrips(make_document(kvp("b", bin))));
    EXPECT_TRUE(ejsonRoundTrips(R"({"b":{"$binary":{"base64":"aGVsbG8=","subType":"00"}}})"));
    // UUID, subtype 04
    EXPECT_TRUE(ejsonRoundTrips(R"({"u":{"$binary":{"base64":"c//SZESHQiGFRvnRFQgPlA==","subType":"04"}}})"));
}

TEST(bson_bridge_roundtrip, decimal128)
{
    EXPECT_TRUE(ejsonRoundTrips(R"({"n":{"$numberDecimal":"3.14159265358979323846"}})"));
    EXPECT_TRUE(ejsonRoundTrips(R"({"n":{"$numberDecimal":"NaN"}})"));
    EXPECT_TRUE(ejsonRoundTrips(R"({"n":{"$numberDecimal":"Infinity"}})"));
}

TEST(bson_bridge_roundtrip, regex_nested_array)
{
    EXPECT_TRUE(roundTrips(make_document(kvp("r", bsoncxx::types::b_regex{"^foo.*bar$", "i"}))));
    EXPECT_TRUE(roundTrips(make_document(
        kvp("outer", make_document(kvp("inner", std::int32_t{99}), kvp("str", "nested"))))));
    EXPECT_TRUE(roundTrips(make_document(
        kvp("arr", make_array(std::int32_t{1}, std::int32_t{2}, std::int32_t{3})))));
}

TEST(bson_bridge_roundtrip, empty_documents)
{
    const mongo::BSONObj empty;
    EXPECT_TRUE(empty.isEmpty());
    EXPECT_EQ(BsonBridge::bsonToEjson(empty), "{}");
    EXPECT_TRUE(BsonBridge::ejsonToBson("{}").isEmpty());
    EXPECT_TRUE(BsonBridge::ejsonToBson("").isEmpty());
}

TEST(bson_bridge_roundtrip, large_string)
{
    EXPECT_TRUE(roundTrips(make_document(kvp("big", std::string(64 * 1024, 'x')))));
}

TEST(bson_bridge_roundtrip, mixed_types)
{
    const std::uint8_t bin_bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    bsoncxx::types::b_binary bin;
    bin.sub_type = bsoncxx::binary_sub_type::k_binary;
    bin.bytes    = bin_bytes;
    bin.size     = sizeof(bin_bytes);
    EXPECT_TRUE(roundTrips(make_document(
        kvp("_id",  bsoncxx::oid{}),
        kvp("date", bsoncxx::types::b_date{std::chrono::milliseconds{1704067200000LL}}),
        kvp("ts",   bsoncxx::types::b_timestamp{2, 1704067201}),
        kvp("bin",  bin),
        kvp("num",  std::int64_t{123456789012345LL}),
        kvp("dbl",  1.23456789),
        kvp("str",  "mixed"),
        kvp("bool", true),
        kvp("null", bsoncxx::types::b_null{}))));
}
