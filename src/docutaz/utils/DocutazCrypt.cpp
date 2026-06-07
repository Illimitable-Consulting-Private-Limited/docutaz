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

        const auto KEY_FILE = QString("%1/.3T/robo-3t/robo3t.key").arg(QDir::homePath()).toStdString();
        QString fileContent;
        QFileInfo const fileInfo{ QString::fromStdString(KEY_FILE) };
        if (fileInfo.exists() && fileInfo.isFile()) {   // a) Read existing key from file
            QFile keyFile{ QString::fromStdString(KEY_FILE) };
            if (!keyFile.open(QIODevice::ReadOnly))
                addToDocutazCryptLogs("DocutazCrypt: Failed to open key file: " + KEY_FILE, MongoSeverity::Error());

            QTextStream in{ &keyFile };
            fileContent = in.readAll();
            if(fileContent.isEmpty())
                addToDocutazCryptLogs("DocutazCrypt: Key file is empty: " + KEY_FILE, MongoSeverity::Error());

            _KEY = fileContent.toLongLong();
        }
        else {  // b) Generate a new key and save it into file
            addToDocutazCryptLogs("DocutazCrypt: No key found, generating a new key and saving it into file", 
                                MongoSeverity::Warning());
            // Generate a new key
            std::random_device randomDevice;
            std::mt19937_64 engine{ randomDevice() };
            std::uniform_int_distribution<long long int> dist{ std::llround(std::pow(2,61)), 
                                                               std::llround(std::pow(2,62)) };
            _KEY = dist(engine);
            // Save the key into file
            QFile file{ QString::fromStdString(KEY_FILE) };
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                addToDocutazCryptLogs("DocutazCrypt: Failed to save the key into file: " + KEY_FILE, MongoSeverity::Error());
                            
            QTextStream out(&file);
            out << QString::number(_KEY);
        }
    }

}