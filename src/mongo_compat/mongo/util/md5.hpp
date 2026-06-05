#pragma once
// MD5 utility stubs using OpenSSL under the hood.
// Used by MongoUtils.cpp to build password hashes.
#include <openssl/md5.h>
#include <string>
#include <cstdint>

namespace mongo {

using md5digest = unsigned char[MD5_DIGEST_LENGTH];  // 16 bytes

inline std::string digestToString(const md5digest& d) {
    static const char hex[] = "0123456789abcdef";
    std::string out(32, '\0');
    for (int i = 0; i < 16; ++i) {
        out[i * 2]     = hex[d[i] >> 4];
        out[i * 2 + 1] = hex[d[i] & 0xf];
    }
    return out;
}

} // mongo

// The actual md5_state_t / md5_init / md5_append / md5_finish come from OpenSSL
// which already defines MD5_CTX / MD5_Init / MD5_Update / MD5_Final.
using md5_state_t  = MD5_CTX;
using md5_byte_t   = unsigned char;
inline void md5_init(md5_state_t* st)  { MD5_Init(st); }
inline void md5_append(md5_state_t* st, const md5_byte_t* data, int len) {
    MD5_Update(st, data, static_cast<size_t>(len));
}
inline void md5_finish(md5_state_t* st, mongo::md5digest d) {
    MD5_Final(d, st);
}
