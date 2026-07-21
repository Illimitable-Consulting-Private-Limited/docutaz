#pragma once

#include <QString>

namespace Docutaz
{
    // Locates the MongoDB Database Tools (mongodump / mongorestore / mongoexport
    // / mongoimport). They ship as one bundle in a single bin/ directory, so a
    // single user-configured "Database Tools folder" (SettingsManager::
    // databaseToolsPath) locates all four. Mirrors MongoshEngine::findMongosh:
    // configured path → DOCUTAZ_DBTOOLS_DIR env → standard install locations →
    // PATH. Returns an absolute executable path, or empty if not found.
    namespace MongoTools
    {
        QString findMongodump();
        QString findMongorestore();
        QString findMongoexport();
        QString findMongoimport();

        // True when at least mongodump and mongorestore are present (the core
        // backup/restore pair). Used to nudge the user before they hit a spawn
        // failure.
        bool areToolsAvailable();

        // Resolve a single tool by its base name ("mongodump", …). Exposed for
        // callers/tests that need an arbitrary tool.
        QString findTool(const QString& baseName);
    }
}
