#include "DocutazCrypt.h"

#include "docutaz/core/utils/Logger.h"

#include <cmath>
#include <iostream>
#include <random>

#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QString>
#include <QTextStream>

namespace Docutaz {

    long long DocutazCrypt::_KEY = 0;
    std::vector<DocutazCrypt::LogAndSeverity> DocutazCrypt::_roboCryptLogs;

    void DocutazCrypt::initKey()
    {
        using MongoSeverity = mongo::logger::LogSeverity;
        auto addToDocutazCryptLogs = [](std::string msg, MongoSeverity severity) {
            _roboCryptLogs.push_back({ msg, severity });
        };

        // Current Docutaz key location. Kept outside the version-scoped config
        // directory so it survives upgrades. The key encrypts saved connection
        // passwords, so its *value* must be preserved across versions/renames.
        const QString NEW_KEY_FILE = QString("%1/.Docutaz/docutaz.key").arg(QDir::homePath());
        // Legacy Robo 3T location. Passwords imported from a Robo 3T install were
        // encrypted with the key stored here, so we migrate its value forward.
        const QString OLD_KEY_FILE = QString("%1/.3T/robo-3t/robo3t.key").arg(QDir::homePath());

        // Reads a key from `path`. Returns true and sets `key` on success.
        auto readKey = [&](QString const& path, long long& key) -> bool {
            QFileInfo const fileInfo{ path };
            if (!fileInfo.exists() || !fileInfo.isFile())
                return false;

            QFile keyFile{ path };
            if (!keyFile.open(QIODevice::ReadOnly)) {
                addToDocutazCryptLogs("DocutazCrypt: Failed to open key file: " + path.toStdString(),
                                      MongoSeverity::Error());
                return false;
            }

            QTextStream in{ &keyFile };
            QString const fileContent = in.readAll();
            if (fileContent.isEmpty()) {
                addToDocutazCryptLogs("DocutazCrypt: Key file is empty: " + path.toStdString(),
                                      MongoSeverity::Error());
                return false;
            }

            key = fileContent.toLongLong();
            return true;
        };

        // Writes `key` to `path`, creating the parent directory if it is missing.
        auto writeKey = [&](QString const& path, long long key) {
            QDir const parentDir = QFileInfo(path).absoluteDir();
            if (!parentDir.exists() && !QDir().mkpath(parentDir.absolutePath())) {
                addToDocutazCryptLogs("DocutazCrypt: Failed to create key directory: "
                                          + parentDir.absolutePath().toStdString(),
                                      MongoSeverity::Error());
                return;
            }

            QFile file{ path };
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                addToDocutazCryptLogs("DocutazCrypt: Failed to save the key into file: " + path.toStdString(),
                                      MongoSeverity::Error());
                return;
            }

            QTextStream out(&file);
            out << QString::number(key);
        };

        // a) Use the current Docutaz key if it already exists.
        if (readKey(NEW_KEY_FILE, _KEY))
            return;

        // b) Migrate a legacy Robo 3T key so existing saved passwords still decrypt.
        if (readKey(OLD_KEY_FILE, _KEY)) {
            addToDocutazCryptLogs("DocutazCrypt: Migrating key from legacy Robo 3T location into "
                                      + NEW_KEY_FILE.toStdString(),
                                  MongoSeverity::Warning());
            writeKey(NEW_KEY_FILE, _KEY);
            return;
        }

        // c) First run: generate a new key and save it into the Docutaz location.
        addToDocutazCryptLogs("DocutazCrypt: No key found, generating a new key and saving it into file",
                              MongoSeverity::Warning());
        std::random_device randomDevice;
        std::mt19937_64 engine{ randomDevice() };
        std::uniform_int_distribution<long long int> dist{ std::llround(std::pow(2,61)),
                                                           std::llround(std::pow(2,62)) };
        _KEY = dist(engine);
        writeKey(NEW_KEY_FILE, _KEY);
    }

}
