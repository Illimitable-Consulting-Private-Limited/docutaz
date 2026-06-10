#include "gtest/gtest.h"

#include "docutaz/core/utils/BsonBridge.h"

#include <mongo/bson/bsonobj.h>

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
