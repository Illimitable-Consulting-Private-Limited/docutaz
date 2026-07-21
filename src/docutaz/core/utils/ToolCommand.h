#pragma once

#include <QString>
#include <QStringList>

namespace Docutaz
{
    // Builds the argument vectors for the MongoDB Database Tools from a small,
    // pure spec (no QProcess, no settings, no I/O) so the command construction is
    // unit-testable. The connection is always passed as a single --uri; callers
    // build it path-less via ConnectionUri (the tools reject a --uri that carries
    // a database path alongside --db/--nsInclude).
    namespace ToolCommand
    {
        // Directory dump vs single gzipped archive file — the two mongodump output
        // shapes, mirrored on restore.
        enum class Layout { Directory, GzipArchive };

        // --- mongodump (ONE invocation). mongodump has no --nsInclude; it selects
        // at most one database per run (optionally one collection, or excluding
        // some), so a multi-database/collection backup is several invocations into
        // the same output. Empty db = dump every database.
        struct DumpSpec
        {
            QString uri;
            QString db;                     // empty = all databases
            QString collection;             // single-collection subset (empty = none)
            QStringList excludeCollections; // subset-by-exclusion (whole-db minus these)
            Layout layout = Layout::Directory;
            // Output directory (Directory) or archive file path (GzipArchive).
            QString outPath;
            // Compress a directory dump's BSON with --gzip. Ignored for archives,
            // which are always written with --gzip.
            bool gzip = false;
        };
        QStringList buildDumpArgs(const DumpSpec& spec);

        // --- mongorestore (BSON restore)
        struct RestoreSpec
        {
            QString uri;
            Layout layout = Layout::Directory;
            // Input directory (Directory) or archive file path (GzipArchive).
            QString inPath;
            // Target database for a single-database directory dump (a folder of
            // loose .bson files, as produced by `mongodump --db`). Without it,
            // mongorestore can't tell which database those files belong to and
            // skips them ("don't know what to do with file"). Leave empty for a
            // full dump root (per-database subdirectories) or an archive, which
            // carry their own database names.
            QString db;
            bool drop = false;                 // --drop (drop each collection first)
            bool gzip = false;                 // gzipped directory dump
            QStringList nsInclude;             // restrict restore to these namespaces
        };
        QStringList buildRestoreArgs(const RestoreSpec& spec);

        // --- mongoexport (single-collection JSON, data-only)
        struct ExportSpec
        {
            QString uri;
            QString db;
            QString collection;
            QString outFile;
            bool canonical = true;             // canonical Extended JSON (lossless)
            bool jsonArray = false;            // one array vs JSON-lines (default)
        };
        QStringList buildExportArgs(const ExportSpec& spec);

        // --- mongoimport (single-collection JSON, data-only)
        struct ImportSpec
        {
            QString uri;
            QString db;
            QString collection;
            QString inFile;
            bool jsonArray = false;
            bool drop = false;                 // --drop before import
            QString mode = "insert";           // insert | upsert | merge
        };
        QStringList buildImportArgs(const ImportSpec& spec);
    }
}
