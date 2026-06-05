#pragma once
#include "mongo/bson/bsonobj.h"
#include <string>

namespace mongo {
// Stub: fromjson is implemented in shell/bson/json.cpp
BSONObj fromjson(const std::string& jsonStr);
}
