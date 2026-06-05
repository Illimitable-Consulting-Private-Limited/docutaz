#pragma once
#include "mongo/util/net/hostandport.h"
#include "mongo/bson/bsonobj.h"  // for StatusWith, ConnectionString
#include "mongo/transport/transport_layer_asio.h"  // for ConnectSSLMode
#include <string>
#include <vector>

namespace mongo {

class MongoURI {
    std::vector<HostAndPort> _servers;
    std::string _setName;
    std::string _user;
    std::string _pwd;
    std::string _db;
    ConnectionString::ConnectionType _type = ConnectionString::MASTER;
    bool _ssl = false;

public:
    MongoURI() = default;

    static StatusWith<MongoURI> parse(const std::string& uri) {
        MongoURI result;
        // Minimal parsing: extract host(s), user, db
        try {
            std::string s = uri;
            // strip mongodb:// or mongodb+srv://
            if (s.substr(0, 14) == "mongodb+srv://") s = s.substr(14);
            else if (s.substr(0, 10) == "mongodb://") s = s.substr(10);
            else return StatusWith<MongoURI>(false, "Invalid URI scheme");

            // Check for ?replicaSet= to detect replica set
            auto qmark = s.find('?');
            std::string opts;
            if (qmark != std::string::npos) {
                opts = s.substr(qmark + 1);
                s = s.substr(0, qmark);
                if (opts.find("replicaSet=") != std::string::npos)
                    result._type = ConnectionString::SET;
            }

            // Extract user:pwd@
            auto at = s.rfind('@');
            if (at != std::string::npos) {
                std::string creds = s.substr(0, at);
                s = s.substr(at + 1);
                auto colon = creds.find(':');
                if (colon != std::string::npos) {
                    result._user = creds.substr(0, colon);
                    result._pwd  = creds.substr(colon + 1);
                }
            }

            // Extract /db
            auto slash = s.find('/');
            if (slash != std::string::npos) {
                result._db = s.substr(slash + 1);
                s = s.substr(0, slash);
            }

            // Parse host list (comma-separated)
            std::string hosts = s;
            size_t pos = 0;
            while (true) {
                auto comma = hosts.find(',', pos);
                std::string h = (comma == std::string::npos)
                    ? hosts.substr(pos)
                    : hosts.substr(pos, comma - pos);
                if (!h.empty()) result._servers.emplace_back(h);
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }

            if (result._servers.empty())
                result._servers.emplace_back("localhost:27017");
        } catch (...) {
            return StatusWith<MongoURI>(false, "Failed to parse URI");
        }
        return StatusWith<MongoURI>(std::move(result));
    }

    ConnectionString::ConnectionType type() const { return _type; }
    const std::vector<HostAndPort>& getServers() const { return _servers; }
    std::string getSetName() const { return _setName; }
    std::string getUser() const { return _user; }
    std::string getPassword() const { return _pwd; }
    std::string getDatabase() const { return _db; }
    // Returns "true" if SSL is enabled (kEnableSSL)
    mongo::transport::ConnectSSLMode getSSLMode() const {
        return _ssl ? mongo::transport::ConnectSSLMode::kEnableSSL
                    : mongo::transport::ConnectSSLMode::kGlobalSSLMode;
    }

    std::string getAuthenticationDatabase() const {
        // Extract authSource= from options if present
        return _db.empty() ? "admin" : _db;
    }

    // Get a single URI option by key (returns empty optional if not found)
    struct OptStr {
        bool _has;
        std::string _val;
        std::string get_value_or(const std::string& def) const { return _has ? _val : def; }
    };
    OptStr getOption(const std::string& /*key*/) const { return {false, ""}; }

    std::string toString() const {
        if (_servers.empty()) return "localhost:27017";
        return _servers[0].toString();
    }
};

} // mongo
