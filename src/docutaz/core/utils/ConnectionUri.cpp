#include "docutaz/core/utils/ConnectionUri.h"

#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/settings/CredentialSettings.h"
#include "docutaz/core/settings/SslSettings.h"
#include "docutaz/core/settings/ReplicaSetSettings.h"

#include <vector>

namespace Docutaz
{
    namespace ConnectionUri
    {
        std::string build(const ConnectionSettings* settings,
                          const std::string& dbName,
                          const Options& opts)
        {
            const bool isSrv = settings->isSrv();
            std::string uri = isSrv ? "mongodb+srv://" : "mongodb://";
            const CredentialSettings* cred = settings->primaryCredential();
            if (cred && !cred->userName().empty())
                uri += ConnectionSettings::percentEncodeUserInfo(cred->userName()) + ":" +
                       ConnectionSettings::percentEncodeUserInfo(cred->userPassword()) + "@";

            if (isSrv) {
                // DNS seed list (Atlas): SRV hostname only, no port; the driver
                // resolves hosts / replica-set / TLS from DNS.
                uri += settings->serverHost();
            } else if (settings->isReplicaSet()) {
                const auto& members = settings->replicaSetSettings()->members();
                for (int i = 0; i < members.size(); ++i) {
                    if (i) uri += ",";
                    uri += members[i];
                }
            } else {
                uri += settings->serverHost() + ":" +
                       std::to_string(settings->serverPort());
            }

            uri += "/";
            if (opts.includeDatabasePath)
                uri += (dbName.empty() ? settings->defaultDatabase() : dbName);

            std::vector<std::string> optsList;
            if (cred && !cred->userName().empty()) {
                // For SRV/Atlas, let the driver negotiate the auth mechanism (picks
                // SCRAM-SHA-256); forcing one (URI import defaults to SCRAM-SHA-1) fails.
                if (!isSrv) {
                    const std::string mech = cred->mechanism();
                    optsList.push_back("authMechanism=" + (mech.empty() ? "SCRAM-SHA-256" : mech));
                }
                if (!cred->databaseName().empty())
                    optsList.push_back("authSource=" + cred->databaseName());
            }
            if (settings->isReplicaSet()) {
                const std::string& entered = settings->replicaSetSettings()->setNameUserEntered();
                const std::string& cached  = settings->replicaSetSettings()->cachedSetName();
                const std::string& name = !entered.empty() ? entered : cached;
                if (!name.empty())
                    optsList.push_back("replicaSet=" + name);
            }
            if (settings->sslSettings() && settings->sslSettings()->sslEnabled()) {
                optsList.push_back("tls=true");
                if (!settings->sslSettings()->caFile().empty())
                    optsList.push_back("tlsCAFile=" + settings->sslSettings()->caFile());
                if (!settings->sslSettings()->pemKeyFile().empty())
                    optsList.push_back("tlsCertificateKeyFile=" +
                                       settings->sslSettings()->pemKeyFile());
                if (settings->sslSettings()->allowInvalidCertificates())
                    optsList.push_back("tlsAllowInvalidCertificates=true");
            }
            if (opts.includeTimeouts) {
                // Fail fast if the server is unreachable.
                optsList.push_back("serverSelectionTimeoutMS=10000");
                optsList.push_back("connectTimeoutMS=10000");
            }
            // Not for SRV/replica set: a seed list resolves to multiple hosts, so
            // directConnection would defeat discovery.
            if (opts.directConnectionForSingleHost &&
                !settings->isReplicaSet() && !isSrv)
                optsList.push_back("directConnection=true");

            if (!optsList.empty()) {
                uri += "?";
                for (size_t i = 0; i < optsList.size(); ++i) {
                    if (i) uri += "&";
                    uri += optsList[i];
                }
            }
            return uri;
        }
    }
}
