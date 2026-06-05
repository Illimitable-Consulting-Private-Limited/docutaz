#pragma once
#include <stdexcept>

namespace mongo {

struct Status {
    bool ok() const { return _ok; }
    std::string toString() const { return _msg; }
    bool _ok = true;
    std::string _msg;
};

inline void uassertStatusOK(const Status& s) {
    if (!s.ok()) throw std::runtime_error(s.toString());
}

} // mongo

#define uassertStatusOK(s) ::mongo::uassertStatusOK(s)
