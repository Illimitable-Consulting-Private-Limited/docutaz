#include "gtest/gtest.h"

#include <QRegularExpression>

#include "docutaz/core/utils/ToolCommand.h"

using namespace Docutaz::ToolCommand;

namespace
{
    const QString kUri = "mongodb://localhost:27017/?directConnection=true";
}

// --- mongodump -------------------------------------------------------------

TEST(tool_command, dump_all_databases_to_directory)
{
    DumpSpec s;
    s.uri = kUri;
    s.outPath = "/backups/full";
    const QStringList args = buildDumpArgs(s);

    EXPECT_EQ(args, (QStringList{ "--uri=" + kUri, "--out=/backups/full" }));
    // Empty db → no --db, dumps everything.
    EXPECT_TRUE(args.filter(QRegularExpression("^--db=")).isEmpty());
}

TEST(tool_command, dump_whole_database_uses_db_flag)
{
    DumpSpec s;
    s.uri = kUri;
    s.db = "shop";
    s.outPath = "/backups/shop";
    const QStringList args = buildDumpArgs(s);

    EXPECT_TRUE(args.contains("--db=shop"));
    EXPECT_TRUE(args.contains("--out=/backups/shop"));
    EXPECT_TRUE(args.filter(QRegularExpression("^--collection=")).isEmpty());
}

TEST(tool_command, dump_single_collection_uses_collection_flag)
{
    DumpSpec s;
    s.uri = kUri;
    s.db = "shop";
    s.collection = "orders";
    s.outPath = "/backups/sel";
    const QStringList args = buildDumpArgs(s);

    EXPECT_TRUE(args.contains("--db=shop"));
    EXPECT_TRUE(args.contains("--collection=orders"));
}

TEST(tool_command, dump_subset_excludes_unselected_collections)
{
    DumpSpec s;
    s.uri = kUri;
    s.db = "shop";
    s.excludeCollections = { "logs", "sessions" };
    s.outPath = "/backups/sel";
    const QStringList args = buildDumpArgs(s);

    EXPECT_TRUE(args.contains("--db=shop"));
    EXPECT_TRUE(args.contains("--excludeCollection=logs"));
    EXPECT_TRUE(args.contains("--excludeCollection=sessions"));
}

TEST(tool_command, dump_gzip_archive_sets_archive_and_gzip)
{
    DumpSpec s;
    s.uri = kUri;
    s.db = "shop";
    s.layout = Layout::GzipArchive;
    s.outPath = "/backups/shop.archive.gz";
    const QStringList args = buildDumpArgs(s);

    EXPECT_TRUE(args.contains("--archive=/backups/shop.archive.gz"));
    EXPECT_TRUE(args.contains("--gzip"));
    EXPECT_TRUE(args.filter(QRegularExpression("^--out=")).isEmpty());
}

TEST(tool_command, dump_directory_gzip_flag)
{
    DumpSpec s;
    s.uri = kUri;
    s.db = "shop";
    s.outPath = "/backups/full";
    s.gzip = true;
    const QStringList args = buildDumpArgs(s);

    EXPECT_TRUE(args.contains("--out=/backups/full"));
    EXPECT_TRUE(args.contains("--gzip"));
}

// --- mongorestore ----------------------------------------------------------

TEST(tool_command, restore_directory_without_drop)
{
    RestoreSpec s;
    s.uri = kUri;
    s.inPath = "/backups/full";
    const QStringList args = buildRestoreArgs(s);

    EXPECT_TRUE(args.contains("--dir=/backups/full"));
    EXPECT_FALSE(args.contains("--drop"));
}

TEST(tool_command, restore_archive_with_drop)
{
    RestoreSpec s;
    s.uri = kUri;
    s.layout = Layout::GzipArchive;
    s.inPath = "/backups/shop.archive.gz";
    s.drop = true;
    const QStringList args = buildRestoreArgs(s);

    EXPECT_TRUE(args.contains("--archive=/backups/shop.archive.gz"));
    EXPECT_TRUE(args.contains("--gzip"));
    EXPECT_TRUE(args.contains("--drop"));
}

TEST(tool_command, restore_single_db_directory_names_target)
{
    // A folder of loose .bson files needs --db so mongorestore knows the target
    // database; without it the files are skipped.
    RestoreSpec s;
    s.uri = kUri;
    s.inPath = "/backups/test22";
    s.db = "test22";
    const QStringList args = buildRestoreArgs(s);

    EXPECT_TRUE(args.contains("--db=test22"));
    EXPECT_TRUE(args.contains("--dir=/backups/test22"));
}

TEST(tool_command, restore_full_root_omits_db)
{
    RestoreSpec s;
    s.uri = kUri;
    s.inPath = "/backups/full";     // per-database subdirectories, no --db
    const QStringList args = buildRestoreArgs(s);

    EXPECT_TRUE(args.filter(QRegularExpression("^--db=")).isEmpty());
}

// --- mongoexport (single-collection JSON) ----------------------------------

TEST(tool_command, export_defaults_to_canonical_jsonlines)
{
    ExportSpec s;
    s.uri = kUri;
    s.db = "shop";
    s.collection = "orders";
    s.outFile = "/out/orders.json";
    const QStringList args = buildExportArgs(s);

    EXPECT_TRUE(args.contains("--db=shop"));
    EXPECT_TRUE(args.contains("--collection=orders"));
    EXPECT_TRUE(args.contains("--out=/out/orders.json"));
    EXPECT_TRUE(args.contains("--jsonFormat=canonical"));
    EXPECT_FALSE(args.contains("--jsonArray"));   // JSON-lines by default
}

TEST(tool_command, export_relaxed_array)
{
    ExportSpec s;
    s.uri = kUri; s.db = "shop"; s.collection = "orders"; s.outFile = "/out/o.json";
    s.canonical = false;
    s.jsonArray = true;
    const QStringList args = buildExportArgs(s);

    EXPECT_TRUE(args.contains("--jsonFormat=relaxed"));
    EXPECT_TRUE(args.contains("--jsonArray"));
}

// --- mongoimport (single-collection JSON) ----------------------------------

TEST(tool_command, import_defaults_omit_insert_mode)
{
    ImportSpec s;
    s.uri = kUri; s.db = "shop"; s.collection = "orders"; s.inFile = "/in/orders.json";
    const QStringList args = buildImportArgs(s);

    EXPECT_TRUE(args.contains("--file=/in/orders.json"));
    EXPECT_FALSE(args.contains("--drop"));
    // insert is the tool default; don't pass it redundantly.
    EXPECT_TRUE(args.filter(QRegularExpression("^--mode=")).isEmpty());
}

TEST(tool_command, import_upsert_with_drop_and_array)
{
    ImportSpec s;
    s.uri = kUri; s.db = "shop"; s.collection = "orders"; s.inFile = "/in/o.json";
    s.jsonArray = true;
    s.drop = true;
    s.mode = "upsert";
    const QStringList args = buildImportArgs(s);

    EXPECT_TRUE(args.contains("--jsonArray"));
    EXPECT_TRUE(args.contains("--drop"));
    EXPECT_TRUE(args.contains("--mode=upsert"));
}
