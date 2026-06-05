#pragma once
// Base64 encoding utility — used by BsonUtils.cpp for BinData elements.
#include <string>
#include <sstream>

namespace mongo {
namespace base64 {

static const char kTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline void encode(std::ostringstream& out, const char* data, int len) {
    for (int i = 0; i < len; i += 3) {
        unsigned char b0 = (unsigned char)data[i];
        unsigned char b1 = (i + 1 < len) ? (unsigned char)data[i + 1] : 0;
        unsigned char b2 = (i + 2 < len) ? (unsigned char)data[i + 2] : 0;
        out << kTable[b0 >> 2];
        out << kTable[((b0 & 3) << 4) | (b1 >> 4)];
        out << ((i + 1 < len) ? kTable[((b1 & 0xf) << 2) | (b2 >> 6)] : '=');
        out << ((i + 2 < len) ? kTable[b2 & 0x3f] : '=');
    }
}

// Overload that takes mongo::StringBuilder (which wraps ostringstream)
template<typename SB>
inline void encode(SB& sb, const char* data, int len) {
    std::ostringstream tmp;
    encode(tmp, data, len);
    sb << tmp.str();
}

} // base64
} // mongo
