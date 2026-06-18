#include "gtest/gtest.h"

#include "docutaz/core/utils/Exporter.h"
#include "docutaz/core/utils/BsonBridge.h"

#include <mongo/bson/bsonobj.h>

#include <sstream>
#include <string>
#include <vector>

using namespace Docutaz;

namespace {

mongo::BSONObj doc(const std::string &ejson)
{
    return BsonBridge::ejsonToBson(ejson);
}

std::string run(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts)
{
    std::ostringstream out;
    Exporter::write(docs, opts, out);
    return out.str();
}

bool contains(const std::string &hay, const std::string &needle)
{
    return hay.find(needle) != std::string::npos;
}

} // namespace

TEST(Exporter, JsonArrayWrapsAndSeparates)
{
    ExportOptions o; o.format = ExportFormat::Json; o.jsonArray = true;
    const std::string s = run({doc(R"({"a":1})"), doc(R"({"a":2})")}, o);
    EXPECT_EQ('[', s.front());
    EXPECT_TRUE(contains(s, "\"a\""));
    EXPECT_TRUE(contains(s, ",\n"));        // separator between elements
    EXPECT_TRUE(contains(s, "]"));
}

TEST(Exporter, JsonLinesOneDocPerLine)
{
    ExportOptions o; o.format = ExportFormat::Json; o.jsonArray = false;
    const std::string s = run({doc(R"({"a":1})"), doc(R"({"a":2})")}, o);
    EXPECT_NE('[', s.front());              // not an array
    // Two non-empty lines, no surrounding brackets.
    int lines = 0;
    std::istringstream in(s); std::string line;
    while (std::getline(in, line)) if (!line.empty()) ++lines;
    EXPECT_EQ(2, lines);
}

TEST(Exporter, CsvHasBomHeaderAndIdFirst)
{
    ExportOptions o; o.format = ExportFormat::Csv;
    const std::string s = run({doc(R"({"name":"x","_id":1,"age":3})")}, o);
    EXPECT_EQ(0u, s.find("\xEF\xBB\xBF"));          // BOM at start
    // Header: _id pinned first even though it wasn't first in the document.
    EXPECT_TRUE(contains(s, "\xEF\xBB\xBF" "_id,name,age\r\n"));
    EXPECT_TRUE(contains(s, "1,x,3\r\n"));
}

TEST(Exporter, CsvUnionsColumnsAcrossDocs)
{
    ExportOptions o; o.format = ExportFormat::Csv;
    const std::string s = run({doc(R"({"a":1})"), doc(R"({"b":2})")}, o);
    EXPECT_TRUE(contains(s, "a,b\r\n"));    // union of keys
    EXPECT_TRUE(contains(s, "1,\r\n"));     // first doc: b missing -> empty
    EXPECT_TRUE(contains(s, ",2\r\n"));     // second doc: a missing -> empty
}

TEST(Exporter, CsvEscapesCommaAndQuote)
{
    ExportOptions o; o.format = ExportFormat::Csv;
    const std::string s = run({doc(R"({"v":"a,\"b\""})")}, o);
    EXPECT_TRUE(contains(s, "\"a,\"\"b\"\"\""));   // a,"b"  ->  "a,""b"""
}

TEST(Exporter, CsvNestedAsJsonStringByDefault)
{
    ExportOptions o; o.format = ExportFormat::Csv; o.flattenNested = false;
    const std::string s = run({doc(R"({"addr":{"city":"NY"}})")}, o);
    EXPECT_TRUE(contains(s, "addr\r\n"));          // single column for the object
    EXPECT_TRUE(contains(s, "city"));              // value is a JSON string
}

TEST(Exporter, CsvFlattenExpandsDotColumns)
{
    ExportOptions o; o.format = ExportFormat::Csv; o.flattenNested = true;
    const std::string s = run({doc(R"({"addr":{"city":"NY","zip":"10001"}})")}, o);
    EXPECT_TRUE(contains(s, "addr.city,addr.zip\r\n"));
    EXPECT_TRUE(contains(s, "NY,10001\r\n"));
}

TEST(Exporter, CsvScalarTypeMapping)
{
    ExportOptions o; o.format = ExportFormat::Csv;
    const std::string s = run({doc(
        R"({"oid":{"$oid":"507f1f77bcf86cd799439011"},"ok":true,"n":42})")}, o);
    EXPECT_TRUE(contains(s, "507f1f77bcf86cd799439011")); // ObjectId as plain hex
    EXPECT_TRUE(contains(s, "true"));                     // bool plain
    EXPECT_TRUE(contains(s, "42"));                       // number plain
}

TEST(Exporter, EmptyInput)
{
    ExportOptions o; o.format = ExportFormat::Json; o.jsonArray = true;
    EXPECT_EQ("[\n]\n", run({}, o));
}
