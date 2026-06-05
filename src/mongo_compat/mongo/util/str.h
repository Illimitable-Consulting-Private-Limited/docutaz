#pragma once
#include <string>

namespace mongo {
namespace str {

// JSON-escape a string value
inline std::string escape(const std::string& s, bool forRegex = false) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (forRegex) {
            // Minimal regex escaping
            if (c == '/' || c == '\\') { out += '\\'; out += c; }
            else out += c;
        } else {
            switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += c;
                }
            }
        }
    }
    return out;
}

} // str
} // mongo
