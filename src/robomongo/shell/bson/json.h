#pragma once
// Replacement json.h — provides the public API the GUI uses.
// The heavy 1490-line JSON parser is replaced by bsoncxx::from_json.

#include <stdexcept>
#include <string>

#include "mongo/bson/bsonobj.h"
#include "robomongo/core/Enums.h"

namespace mongo {

// Stub Status used by legacy callers
struct Status {
    bool _ok = true;
    std::string _msg;
    bool ok() const { return _ok; }
    std::string toString() const { return _msg; }
    static Status OK() { return {true, ""}; }
};

namespace Robomongo {

class ParseMsgAssertionException : public std::exception {
public:
    ParseMsgAssertionException(int /*code*/, const std::string& /*msg*/,
                               int offset, const std::string& reason)
        : _reason(reason), _offset(offset) {}
    virtual ~ParseMsgAssertionException() noexcept = default;
    bool severe() const { return false; }
    std::string reason() const { return _reason; }
    int offset() const { return _offset; }
    const char* what() const noexcept override { return _reason.c_str(); }
private:
    std::string _reason;
    int _offset = 0;
};

// Parse a JSON string into a BSONObj.
// Throws ParseMsgAssertionException on parse error.
BSONObj fromjson(const std::string& str);

// Parse with length output (len = number of JSON chars consumed).
BSONObj fromjson(const char* str, int* len = nullptr);

}  // namespace Robomongo
}  // namespace mongo
