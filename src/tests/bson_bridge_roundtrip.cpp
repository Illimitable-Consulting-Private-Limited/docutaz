// Standalone round-trip verification for BsonBridge.
// Tests that every EJSON type mongosh can emit survives the
//   bsoncxx::from_json → mongo::BSONObj → bsoncxx::to_json(canonical)
// round-trip with identical BSON bytes on both sides.
//
// Build:  ninja bson_bridge_test   (EXCLUDE_FROM_ALL)
// Run:    ./src/tests/bson_bridge_test

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/bson_value/value.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Pull in BsonBridge (header-only, just needs bsoncxx + mongo/bson/bsonobj.h)
#include "mongo/bson/bsonobj.h"
#include "robomongo/core/utils/BsonBridge.h"

using namespace bsoncxx::builder::basic;
using bsoncxx::document::value;
using bsoncxx::document::view;

// ── helpers ──────────────────────────────────────────────────────────────────

static int gPass = 0, gFail = 0;

static void check(const char* name, bool ok, const std::string& detail = {}) {
    if (ok) {
        std::cout << "  PASS  " << name << "\n";
        ++gPass;
    } else {
        std::cout << "  FAIL  " << name;
        if (!detail.empty()) std::cout << " (" << detail << ")";
        std::cout << "\n";
        ++gFail;
    }
}

// Round-trip: bsoncxx value → canonical EJSON → BSONObj → bsoncxx view → compare bytes
static bool roundTrip(const value& original) {
    // Step 1: canonical EJSON
    const std::string ejson = bsoncxx::to_json(original.view(),
                                               bsoncxx::ExtendedJsonMode::k_canonical);
    // Step 2: parse back via BsonBridge
    mongo::BSONObj obj;
    try {
        obj = Robomongo::BsonBridge::ejsonToBson(ejson);
    } catch (const std::exception& e) {
        std::cout << "    ejsonToBson threw: " << e.what() << "\n";
        return false;
    }

    // Step 3: compare raw BSON bytes
    const std::size_t origLen = original.view().length();
    const std::size_t bsonLen = static_cast<std::size_t>(obj.objsize());
    if (origLen != bsonLen) {
        std::cout << "    size mismatch: original=" << origLen
                  << " roundtripped=" << bsonLen << "\n";
        std::cout << "    ejson: " << ejson << "\n";
        return false;
    }
    if (std::memcmp(original.view().data(), obj.objdata(), origLen) != 0) {
        std::cout << "    byte mismatch\n";
        std::cout << "    ejson: " << ejson << "\n";
        return false;
    }
    return true;
}

// bsonToEjson convenience wrapper
static bool ejsonRoundTrip(const std::string& ejson) {
    try {
        mongo::BSONObj obj = Robomongo::BsonBridge::ejsonToBson(ejson);
        const std::string back = Robomongo::BsonBridge::bsonToEjson(obj);
        // Parse both and compare bytes
        auto v1 = bsoncxx::from_json(ejson);
        auto v2 = bsoncxx::from_json(back);
        if (v1.view().length() != v2.view().length()) return false;
        return std::memcmp(v1.view().data(), v2.view().data(), v1.view().length()) == 0;
    } catch (const std::exception& e) {
        std::cout << "    exception: " << e.what() << "\n";
        return false;
    }
}

// ── test cases ────────────────────────────────────────────────────────────────

static void test_string() {
    auto doc = make_document(kvp("k", "hello world"));
    check("string", roundTrip(doc));
}

static void test_int32() {
    auto doc = make_document(kvp("k", int32_t{42}));
    check("int32", roundTrip(doc));
}

static void test_int64() {
    auto doc = make_document(kvp("k", int64_t{9007199254740993LL}));
    check("int64 (beyond JS MAX_SAFE_INTEGER)", roundTrip(doc));
}

static void test_double() {
    auto doc = make_document(kvp("k", 3.14159265358979));
    check("double", roundTrip(doc));
}

static void test_bool() {
    auto doc = make_document(kvp("t", true), kvp("f", false));
    check("bool", roundTrip(doc));
}

static void test_null() {
    auto doc = make_document(kvp("k", bsoncxx::types::b_null{}));
    check("null", roundTrip(doc));
}

static void test_objectid() {
    bsoncxx::oid oid{"507f1f77bcf86cd799439011"};
    auto doc = make_document(kvp("_id", oid));
    check("ObjectId", roundTrip(doc));
}

static void test_date() {
    // 2024-01-01T00:00:00Z = 1704067200000 ms
    bsoncxx::types::b_date d{std::chrono::milliseconds{1704067200000LL}};
    auto doc = make_document(kvp("d", d));
    check("Date", roundTrip(doc));
}

static void test_date_from_mongosh_relaxed() {
    // mongosh relaxed EJSON uses ISO string for dates
    const std::string ejson = R"({"d":{"$date":"2024-01-01T00:00:00.000Z"}})";
    check("Date (mongosh relaxed $date string)", ejsonRoundTrip(ejson));
}

static void test_timestamp() {
    bsoncxx::types::b_timestamp ts{1, 1704067200};  // {increment, timestamp}
    auto doc = make_document(kvp("t", ts));
    check("Timestamp", roundTrip(doc));
}

static void test_timestamp_from_mongosh() {
    const std::string ejson = R"({"t":{"$timestamp":{"t":1704067200,"i":1}}})";
    check("Timestamp (mongosh EJSON)", ejsonRoundTrip(ejson));
}

static void test_binary_generic() {
    const uint8_t bytes[] = {0x68, 0x65, 0x6c, 0x6c, 0x6f};
    bsoncxx::types::b_binary bin;
    bin.sub_type = bsoncxx::binary_sub_type::k_binary;
    bin.bytes    = bytes;
    bin.size     = sizeof(bytes);
    auto doc = make_document(kvp("b", bin));
    check("Binary (generic)", roundTrip(doc));
}

static void test_binary_from_mongosh() {
    // mongosh EJSON v2 format
    const std::string ejson =
        R"({"b":{"$binary":{"base64":"aGVsbG8=","subType":"00"}}})";
    check("Binary (mongosh EJSON v2)", ejsonRoundTrip(ejson));
}

static void test_binary_uuid() {
    // UUID subtype 4
    const std::string ejson =
        R"({"u":{"$binary":{"base64":"c//SZESHQiGFRvnRFQgPlA==","subType":"04"}}})";
    check("Binary UUID (subtype 04)", ejsonRoundTrip(ejson));
}

static void test_decimal128() {
    const std::string ejson = R"({"n":{"$numberDecimal":"3.14159265358979323846"}})";
    check("Decimal128", ejsonRoundTrip(ejson));
}

static void test_decimal128_special() {
    // NaN, Infinity
    check("Decimal128 NaN",
          ejsonRoundTrip(R"({"n":{"$numberDecimal":"NaN"}})"));
    check("Decimal128 Infinity",
          ejsonRoundTrip(R"({"n":{"$numberDecimal":"Infinity"}})"));
}

static void test_regex() {
    bsoncxx::types::b_regex rx{"^foo.*bar$", "i"};
    auto doc = make_document(kvp("r", rx));
    check("Regex", roundTrip(doc));
}

static void test_nested_doc() {
    auto doc = make_document(
        kvp("outer", make_document(
            kvp("inner", int32_t{99}),
            kvp("str", "nested"))));
    check("nested document", roundTrip(doc));
}

static void test_array() {
    auto doc = make_document(
        kvp("arr", make_array(int32_t{1}, int32_t{2}, int32_t{3})));
    check("array", roundTrip(doc));
}

static void test_empty() {
    mongo::BSONObj empty;
    const bool ok = empty.isEmpty()
                    && Robomongo::BsonBridge::bsonToEjson(empty) == "{}"
                    && Robomongo::BsonBridge::ejsonToBson("{}").isEmpty()
                    && Robomongo::BsonBridge::ejsonToBson("").isEmpty();
    check("empty/null documents", ok);
}

static void test_large_string() {
    std::string big(64 * 1024, 'x');
    auto doc = make_document(kvp("big", big));
    check("large string (64 KiB)", roundTrip(doc));
}

static void test_mixed_types() {
    bsoncxx::oid oid;
    bsoncxx::types::b_date d{std::chrono::milliseconds{1704067200000LL}};
    bsoncxx::types::b_timestamp ts{2, 1704067201};  // {increment, timestamp}
    const uint8_t bin_bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    bsoncxx::types::b_binary bin;
    bin.sub_type = bsoncxx::binary_sub_type::k_binary;
    bin.bytes = bin_bytes; bin.size = sizeof(bin_bytes);

    auto doc = make_document(
        kvp("_id",   oid),
        kvp("date",  d),
        kvp("ts",    ts),
        kvp("bin",   bin),
        kvp("num",   int64_t{123456789012345LL}),
        kvp("dbl",   1.23456789),
        kvp("str",   "mixed"),
        kvp("bool",  true),
        kvp("null",  bsoncxx::types::b_null{}));
    check("mixed types document", roundTrip(doc));
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "BsonBridge round-trip verification\n"
              << "===================================\n";

    test_string();
    test_int32();
    test_int64();
    test_double();
    test_bool();
    test_null();
    test_objectid();
    test_date();
    test_date_from_mongosh_relaxed();
    test_timestamp();
    test_timestamp_from_mongosh();
    test_binary_generic();
    test_binary_from_mongosh();
    test_binary_uuid();
    test_decimal128();
    test_decimal128_special();
    test_regex();
    test_nested_doc();
    test_array();
    test_empty();
    test_large_string();
    test_mixed_types();

    std::cout << "\n";
    if (gFail == 0)
        std::cout << "All " << gPass << " tests PASSED.\n";
    else
        std::cout << gPass << " passed, " << gFail << " FAILED.\n";

    return gFail ? 1 : 0;
}
