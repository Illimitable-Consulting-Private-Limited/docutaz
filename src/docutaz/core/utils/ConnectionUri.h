#pragma once

#include <string>

namespace Docutaz
{
    class ConnectionSettings;

    // Builds a mongodb://… / mongodb+srv://… connection string from a
    // ConnectionSettings, covering credentials, auth mechanism/source, replica
    // sets, SRV seed lists and TLS. Extracted from MongoshEngine so mongosh and
    // the MongoDB Database Tools (mongodump/mongorestore/mongoexport/mongoimport)
    // share one implementation of the connection story.
    namespace ConnectionUri
    {
        struct Options
        {
            // Append "/<db>" (or "/<defaultDatabase>") to the URI. mongosh wants
            // this; the tools do NOT — they reject a --uri that carries a database
            // path alongside --db/--nsInclude, so they build a path-less URI and
            // select namespaces via flags. When false the URI ends with "/".
            bool includeDatabasePath = true;
            // Add serverSelectionTimeoutMS/connectTimeoutMS so a dead host fails
            // fast instead of hanging.
            bool includeTimeouts = true;
            // Add directConnection=true for a single (non-replica-set, non-SRV)
            // host so the client doesn't spin on topology discovery.
            bool directConnectionForSingleHost = true;
        };

        // `dbName` is used only when opts.includeDatabasePath is true (empty →
        // the connection's defaultDatabase()).
        std::string build(const ConnectionSettings* settings,
                          const std::string& dbName = {},
                          const Options& opts = {});
    }
}
