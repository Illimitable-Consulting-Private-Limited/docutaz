#pragma once
#include "mongo/client/dbclient_base.h"
#include <string>
#include <vector>

namespace mongo {

class DBClientReplicaSet : public DBClientBase {
public:
    DBClientReplicaSet(const std::string& /*setName*/,
                       const std::vector<HostAndPort>& /*servers*/,
                       double /*timeout*/ = 0) {}
};

} // mongo
