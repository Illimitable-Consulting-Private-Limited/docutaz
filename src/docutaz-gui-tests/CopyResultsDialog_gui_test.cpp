#include "gtest/gtest.h"

#include "docutaz/gui/dialogs/CopyResultsDialog.h"
#include "docutaz/core/settings/ConnectionSettings.h"

#include <QComboBox>
#include <QLineEdit>
#include <QStringList>

using namespace Docutaz;

namespace {

ConnectionSettings *makeConn(const QString &name, const QString &uuid)
{
    auto *c = new ConnectionSettings(false);
    c->setConnectionName(name.toStdString());
    c->setUuid(uuid);
    return c;
}

QStringList comboItems(QComboBox *c)
{
    QStringList out;
    for (int i = 0; i < c->count(); ++i)
        out << c->itemText(i);
    return out;
}

} // namespace

// The bug that prompted the GUI suite: the target-database dropdown must be
// populated from the selected connection's databases.
TEST(CopyResultsDialogGui, PopulatesDatabaseDropdownFromConnection)
{
    ConnectionSettings *src = makeConn("local", "u1");
    {
        CopyResultsDialog dlg(src, "teaerp", "orders",
                              {{src, {"teaerp", "tmp", "admin"}}});
        auto *dbCombo = dlg.findChild<QComboBox *>("copyTargetDatabase");
        ASSERT_NE(dbCombo, nullptr);

        const QStringList items = comboItems(dbCombo);
        EXPECT_TRUE(items.contains("teaerp"));
        EXPECT_TRUE(items.contains("tmp"));
        EXPECT_TRUE(items.contains("admin"));
        // Defaults to the source database.
        EXPECT_EQ(dlg.targetDatabase(), QString("teaerp"));
    }
    delete src;
}

TEST(CopyResultsDialogGui, SwitchingConnectionRepopulatesDatabases)
{
    ConnectionSettings *a = makeConn("A", "ua");
    ConnectionSettings *b = makeConn("B", "ub");
    {
        CopyResultsDialog dlg(a, "dbA", "coll",
                              {{a, {"dbA", "x"}}, {b, {"dbB", "y"}}});
        auto *connCombo = dlg.findChild<QComboBox *>("copyTargetConnection");
        auto *dbCombo   = dlg.findChild<QComboBox *>("copyTargetDatabase");
        ASSERT_NE(connCombo, nullptr);
        ASSERT_NE(dbCombo, nullptr);
        EXPECT_TRUE(comboItems(dbCombo).contains("dbA"));

        connCombo->setCurrentIndex(1);   // select connection B
        EXPECT_EQ(dlg.targetConnection(), b);
        const QStringList items = comboItems(dbCombo);
        EXPECT_TRUE(items.contains("dbB"));
        EXPECT_TRUE(items.contains("y"));
        // A's databases are gone (the source-db default may carry over as an
        // editable entry, but A's other databases must not).
        EXPECT_FALSE(items.contains("x"));
    }
    delete a;
    delete b;
}

// Copying a collection onto itself (same connection + db + collection) must be
// rejected; a different collection is accepted.
TEST(CopyResultsDialogGui, RejectsCopyOntoItself)
{
    ConnectionSettings *src = makeConn("local", "u1");
    {
        CopyResultsDialog dlg(src, "db", "coll", {{src, {"db"}}});
        auto *dbCombo  = dlg.findChild<QComboBox *>("copyTargetDatabase");
        auto *collEdit = dlg.findChild<QLineEdit *>("copyTargetCollection");
        ASSERT_NE(dbCombo, nullptr);
        ASSERT_NE(collEdit, nullptr);

        dbCombo->setCurrentText("db");
        collEdit->setText("coll");
        EXPECT_FALSE(dlg.validate().isEmpty());   // same namespace -> rejected

        collEdit->setText("coll_debug");
        EXPECT_TRUE(dlg.validate().isEmpty());    // different collection -> ok
    }
    delete src;
}

TEST(CopyResultsDialogGui, RequiresTargetCollection)
{
    ConnectionSettings *src = makeConn("local", "u1");
    {
        CopyResultsDialog dlg(src, "db", "coll", {{src, {"db"}}});
        auto *collEdit = dlg.findChild<QLineEdit *>("copyTargetCollection");
        collEdit->setText("");
        EXPECT_FALSE(dlg.validate().isEmpty());
    }
    delete src;
}
