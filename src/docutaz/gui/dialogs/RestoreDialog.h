#pragma once

#include "docutaz/gui/dialogs/ToolRunnerDialog.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QGroupBox;
class QLabel;
class QWidget;
QT_END_NAMESPACE

namespace Docutaz
{
    class MongoServer;

    // Restore data INTO the connected server. A BSON dump directory or gzip
    // archive is restored with mongorestore; a JSON file is imported into a
    // chosen collection with mongoimport. Restore is a mass write, so it routes
    // through the production write-guard before running.
    class RestoreDialog : public ToolRunnerDialog
    {
        Q_OBJECT

    public:
        RestoreDialog(MongoServer* server,
                      const QString& preselectDb = {},
                      QWidget* parent = nullptr);

    protected:
        QString toolPath() const override;
        QString toolName() const override;
        bool validate(QString& error) const override;
        bool confirm() override;
        QList<QStringList> argumentSets() const override;
        QString successTitle() const override;
        QString successMessage() const override;
        QString notFoundMessage() const override;

    private Q_SLOTS:
        void updateSourceState();
        void browseInput();
        void refreshDirTarget();

    private:
        enum SourceType { DirectoryDump = 0, GzipArchive = 1, JsonFile = 2 };
        SourceType sourceType() const;
        bool jsonMode() const { return sourceType() == JsonFile; }
        QString buildUri() const;
        // True when the chosen directory holds loose .bson files (a single-database
        // dump from `mongodump --db`) rather than per-database subdirectories.
        bool dirIsSingleDbDump() const;

        MongoServer* _server;

        QComboBox*   _sourceType = nullptr;
        QLineEdit*   _inputEdit = nullptr;
        QPushButton* _browseButton = nullptr;
        QWidget*     _targetDbRow = nullptr;   // "Restore into database" row
        QLineEdit*   _targetDbEdit = nullptr;
        QLabel*      _targetDbHint = nullptr;
        QCheckBox*   _gzipCheck = nullptr;    // gzipped directory dump
        QCheckBox*   _dropCheck = nullptr;

        QGroupBox*   _jsonBox = nullptr;
        QLineEdit*   _dbEdit = nullptr;
        QLineEdit*   _collEdit = nullptr;
        QComboBox*   _modeCombo = nullptr;
        QCheckBox*   _jsonArrayCheck = nullptr;
    };
}
