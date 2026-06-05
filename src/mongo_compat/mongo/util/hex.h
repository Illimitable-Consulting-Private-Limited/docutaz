#pragma once
#include <string>
#include <stdexcept>

namespace mongo {

inline std::string toHexLower(const void* buf, std::size_t len) {
    static const char hex[] = "0123456789abcdef";
    const auto* b = static_cast<const unsigned char*>(buf);
    std::string out;
    out.resize(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out[i * 2]     = hex[b[i] >> 4];
        out[i * 2 + 1] = hex[b[i] & 0xf];
    }
    return out;
}

// Returns value of one hex char, wrapped so callers can do .getValue()
struct HexChar {
    unsigned char c;
    unsigned char getValue() const { return c; }
};

inline HexChar fromHex(const char* p) {
    auto hexDigit = [](char c) -> unsigned char {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("invalid hex digit");
    };
    return {static_cast<unsigned char>((hexDigit(p[0]) << 4) | hexDigit(p[1]))};
}

} // mongo
