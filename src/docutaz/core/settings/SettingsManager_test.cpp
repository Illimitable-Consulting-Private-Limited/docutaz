// Tests for SettingsManager's single-directory layout + schema migration.
//
// The config base dir is redirected to a per-test QTemporaryDir via the
// $DOCUTAZ_CONFIG_DIR override (see configBaseDir() in SettingsManager.h), so
// none of these touch the developer's real ~/.Docutaz.

#include <gtest/gtest.h>

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include "docutaz/core/settings/SettingsManager.h"

using namespace Docutaz;

namespace
{
    class SettingsManagerTest : public ::testing::Test
    {
    protected:
        QTemporaryDir tmp;

        void SetUp() override
        {
            ASSERT_TRUE(tmp.isValid());
            qputenv("DOCUTAZ_CONFIG_DIR", tmp.path().toLocal8Bit());
        }
        void TearDown() override { qunsetenv("DOCUTAZ_CONFIG_DIR"); }

        // Write a docutaz.json under <tmp>/<sub>/ (sub empty => the base config).
        void writeConfig(const QString& sub, const QJsonObject& obj)
        {
            QString dir = sub.isEmpty() ? tmp.path() : tmp.path() + "/" + sub;
            QDir().mkpath(dir);
            QFile f(dir + "/docutaz.json");
            ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(QJsonDocument(obj).toJson());
        }

        QJsonObject readBaseConfig() const
        {
            QFile f(configFilePath());
            if (!f.open(QIODevice::ReadOnly))
                return {};
            return QJsonDocument::fromJson(f.readAll()).object();
        }

        // A minimal legacy (pre-schemaVersion) config with a recognizable marker.
        static QJsonObject legacyConfig(int batchSize)
        {
            QJsonObject o;
            o["version"] = "2.0";
            o["imported"] = true;   // mirrors real configs => no Robo 3T re-import
            o["batchSize"] = batchSize;
            return o;
        }
    };

    TEST_F(SettingsManagerTest, MigratesFromNewestVersionDir)
    {
        writeConfig("2.2.0", legacyConfig(22));
        writeConfig("2.3.0", legacyConfig(33));

        SettingsManager sm;

        // Newest version (2.3.0) wins, copied into the single config location.
        EXPECT_TRUE(QFile::exists(configFilePath()));
        EXPECT_EQ(sm.batchSize(), 33);
    }

    TEST_F(SettingsManagerTest, EulaAcceptanceCarriedForward)
    {
        QJsonObject cfg = legacyConfig(50);
        cfg["acceptedEulaVersions"] = QJsonArray{ "2.3.0" };
        writeConfig("2.3.0", cfg);

        SettingsManager sm;

        // The license text didn't change, so the prior acceptance should count
        // as accepting the current text -> no re-prompt after upgrade.
        EXPECT_TRUE(sm.acceptedEulaVersions().contains(QString::fromLatin1(CurrentEulaVersion)));
    }

    TEST_F(SettingsManagerTest, ExistingConfigIsNotReMigrated)
    {
        writeConfig("", legacyConfig(77));      // base config already present
        writeConfig("2.3.0", legacyConfig(33)); // should be ignored

        SettingsManager sm;

        EXPECT_EQ(sm.batchSize(), 77);
    }

    TEST_F(SettingsManagerTest, FreshInstallCreatesStampedConfig)
    {
        // No prior config anywhere -> empty config created and stamped current.
        SettingsManager sm;

        EXPECT_TRUE(QFile::exists(configFilePath()));
        EXPECT_EQ(readBaseConfig().value("schemaVersion").toInt(), CurrentConfigSchema);
    }

    TEST_F(SettingsManagerTest, SaveStampsCurrentSchema)
    {
        writeConfig("2.3.0", legacyConfig(44)); // legacy, no schemaVersion

        SettingsManager sm;
        ASSERT_TRUE(sm.save());

        EXPECT_EQ(readBaseConfig().value("schemaVersion").toInt(), CurrentConfigSchema);
    }

    TEST_F(SettingsManagerTest, DowngradeFromNewerSchemaIsBackedUp)
    {
        QJsonObject cfg = legacyConfig(55);
        cfg["schemaVersion"] = 99;   // written by a hypothetical newer build
        writeConfig("", cfg);

        SettingsManager sm;

        // Guarded: backed up, and loaded best-effort rather than reinterpreted.
        EXPECT_TRUE(QFile::exists(configFilePath() + ".from-schema-99.bak"));
        EXPECT_EQ(sm.batchSize(), 55);
    }
}
