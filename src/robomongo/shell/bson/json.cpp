// Replacement json.cpp — parses JSON using bsoncxx::from_json.
// This replaces the 1490-line embedded-shell JSON parser.

#include "robomongo/shell/bson/json.h"

#include <bsoncxx/json.hpp>
#include <bsoncxx/exception/exception.hpp>

#include <cstring>

namespace mongo {
namespace Docutaz {

static BSONObj parseWithBsoncxx(const char* str, int* outLen) {
    if (!str || *str == '\0') {
        if (outLen) *outLen = 0;
        return {};
    }

    try {
        auto val = bsoncxx::from_json(std::string(str));
        if (outLen) *outLen = static_cast<int>(strlen(str));
        return BSONObj(val.view());
    } catch (const bsoncxx::exception& e) {
        throw ParseMsgAssertionException(
            0, e.what(), 0, std::string(e.what()));
    } catch (const std::exception& e) {
        throw ParseMsgAssertionException(
            0, e.what(), 0, std::string(e.what()));
    }
}

BSONObj fromjson(const std::string& str) {
    return parseWithBsoncxx(str.c_str(), nullptr);
}

BSONObj fromjson(const char* str, int* len) {
    return parseWithBsoncxx(str, len);
}

}  // namespace Docutaz
}  // namespace mongo
