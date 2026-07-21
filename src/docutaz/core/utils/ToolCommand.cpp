#include "docutaz/core/utils/ToolCommand.h"

namespace Docutaz
{
    namespace ToolCommand
    {
        QStringList buildDumpArgs(const DumpSpec& spec)
        {
            QStringList args;
            args << ("--uri=" + spec.uri);
            if (!spec.db.isEmpty())
                args << ("--db=" + spec.db);
            if (!spec.collection.isEmpty())
                args << ("--collection=" + spec.collection);
            for (const QString& c : spec.excludeCollections)
                args << ("--excludeCollection=" + c);

            if (spec.layout == Layout::GzipArchive) {
                args << ("--archive=" + spec.outPath);
                args << "--gzip";
            } else {
                args << ("--out=" + spec.outPath);
                if (spec.gzip)
                    args << "--gzip";
            }
            return args;
        }

        QStringList buildRestoreArgs(const RestoreSpec& spec)
        {
            QStringList args;
            args << ("--uri=" + spec.uri);
            // Name the target database for a single-database directory dump so
            // mongorestore maps the loose .bson files to a database instead of
            // skipping them.
            if (!spec.db.isEmpty())
                args << ("--db=" + spec.db);
            if (spec.drop)
                args << "--drop";
            for (const QString& ns : spec.nsInclude)
                args << ("--nsInclude=" + ns);

            if (spec.layout == Layout::GzipArchive) {
                args << ("--archive=" + spec.inPath);
                args << "--gzip";
            } else {
                args << ("--dir=" + spec.inPath);
                if (spec.gzip)
                    args << "--gzip";
            }
            return args;
        }

        QStringList buildExportArgs(const ExportSpec& spec)
        {
            QStringList args;
            args << ("--uri=" + spec.uri);
            args << ("--db=" + spec.db);
            args << ("--collection=" + spec.collection);
            args << ("--out=" + spec.outFile);
            args << ("--jsonFormat=" + QString(spec.canonical ? "canonical" : "relaxed"));
            if (spec.jsonArray)
                args << "--jsonArray";
            return args;
        }

        QStringList buildImportArgs(const ImportSpec& spec)
        {
            QStringList args;
            args << ("--uri=" + spec.uri);
            args << ("--db=" + spec.db);
            args << ("--collection=" + spec.collection);
            args << ("--file=" + spec.inFile);
            if (spec.jsonArray)
                args << "--jsonArray";
            if (spec.drop)
                args << "--drop";
            if (!spec.mode.isEmpty() && spec.mode != "insert")
                args << ("--mode=" + spec.mode);
            return args;
        }
    }
}
