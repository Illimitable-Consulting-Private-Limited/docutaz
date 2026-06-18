#include "gtest/gtest.h"

#include "docutaz/gui/dialogs/ExportResultsDialog.h"

#include <QComboBox>
#include <QLineEdit>

using namespace Docutaz;

// Selecting a format updates the file extension and the reported format.
TEST(ExportResultsDialogGui, FormatDrivesExtension)
{
    ExportResultsDialog dlg("local — db.coll", "orders");
    auto *fmt  = dlg.findChild<QComboBox *>("exportFormat");
    auto *path = dlg.findChild<QLineEdit *>("exportPath");
    ASSERT_NE(fmt, nullptr);
    ASSERT_NE(path, nullptr);

    EXPECT_EQ(static_cast<int>(dlg.format()), static_cast<int>(ExportFormat::Json));
    EXPECT_TRUE(path->text().endsWith(".json"));

    fmt->setCurrentIndex(1);   // CSV
    EXPECT_EQ(static_cast<int>(dlg.format()), static_cast<int>(ExportFormat::Csv));
    EXPECT_TRUE(dlg.filePath().endsWith(".csv"));

    fmt->setCurrentIndex(2);   // Excel
    EXPECT_EQ(static_cast<int>(dlg.format()), static_cast<int>(ExportFormat::Xlsx));
    EXPECT_TRUE(dlg.filePath().endsWith(".xlsx"));
}

// The base name carries across a format switch (orders.json -> orders.csv).
TEST(ExportResultsDialogGui, KeepsBaseNameAcrossFormatChange)
{
    ExportResultsDialog dlg("x", "orders");
    auto *fmt = dlg.findChild<QComboBox *>("exportFormat");
    fmt->setCurrentIndex(1);   // CSV
    EXPECT_TRUE(dlg.filePath().endsWith("orders.csv"));
}

TEST(ExportResultsDialogGui, ShapeAndNestedGetters)
{
    ExportResultsDialog dlg("x", "orders");
    auto *shape  = dlg.findChild<QComboBox *>("exportJsonShape");
    auto *nested = dlg.findChild<QComboBox *>("exportCsvNested");
    ASSERT_NE(shape, nullptr);
    ASSERT_NE(nested, nullptr);

    EXPECT_TRUE(dlg.jsonArray());          // index 0 = array
    shape->setCurrentIndex(1);             // JSONL
    EXPECT_FALSE(dlg.jsonArray());

    EXPECT_FALSE(dlg.flattenNested());     // index 0 = JSON string
    nested->setCurrentIndex(1);            // flatten
    EXPECT_TRUE(dlg.flattenNested());
}
