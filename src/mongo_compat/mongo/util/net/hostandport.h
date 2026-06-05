#pragma once
#include <string>
#include <ostream>

namespace mongo {

class HostAndPort {
    std::string _host;
    int _port;
public:
    HostAndPort() : _port(27017) {}
    explicit HostAndPort(const std::string& hostport) {
        auto colon = hostport.rfind(':');
        if (colon != std::string::npos) {
            _host = hostport.substr(0, colon);
            _port = std::stoi(hostport.substr(colon + 1));
        } else {
            _host = hostport;
            _port = 27017;
        }
    }
    HostAndPort(const std::string& host, int port) : _host(host), _port(port) {}
    std::string host() const { return _host; }
    int port() const { return _port; }
    bool empty() const { return _host.empty(); }
    std::string toString() const { return _host + ":" + std::to_string(_port); }
    bool operator==(const HostAndPort& o) const { return _host == o._host && _port == o._port; }
    bool operator!=(const HostAndPort& o) const { return !(*this == o); }
    bool operator<(const HostAndPort& o) const {
        if (_host != o._host) return _host < o._host;
        return _port < o._port;
    }
    friend std::ostream& operator<<(std::ostream& os, const HostAndPort& h) {
        return os << h.toString();
    }
};

} // mongo
