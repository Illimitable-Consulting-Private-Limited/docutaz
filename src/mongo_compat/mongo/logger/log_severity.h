#pragma once
#include <string>

namespace mongo {
namespace logger {

struct StringData {
    std::string _s;
    std::string toString() const { return _s; }
};

class LogSeverity {
    int _level;
    explicit LogSeverity(int l) : _level(l) {}
public:
    static LogSeverity Error()   { return LogSeverity{0}; }
    static LogSeverity Warning() { return LogSeverity{1}; }
    static LogSeverity Info()    { return LogSeverity{2}; }
    static LogSeverity Log()     { return LogSeverity{2}; }
    static LogSeverity Debug(int = 1) { return LogSeverity{3}; }

    bool operator==(const LogSeverity& o) const { return _level == o._level; }
    bool operator!=(const LogSeverity& o) const { return _level != o._level; }
    bool operator<(const LogSeverity& o)  const { return _level < o._level;  }
    std::string toStringShort() const {
        switch (_level) {
        case 0: return "E";
        case 1: return "W";
        case 2: return "I";
        default: return "D";
        }
    }
    StringData toStringData() const {
        switch (_level) {
        case 0: return {"Error"};
        case 1: return {"Warning"};
        case 2: return {"Info"};
        default: return {"Debug"};
        }
    }
};

} // logger
} // mongo
