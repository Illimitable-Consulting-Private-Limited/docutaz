#include "docutaz/gui/dialogs/RestoreDialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>

#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/utils/ConnectionUri.h"
#include "docutaz/core/utils/ToolCommand.h"
#include "docutaz/core/utils/MongoTools.h"
#include "docutaz/core/utils/ScriptClassifier.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/gui/utils/DialogUtils.h"

namespace Docutaz
{
    RestoreDialog::RestoreDialog(MongoServer* server, const QString& preselectDb,
                                 QWidget* parent)
        : ToolRunnerDialog(parent), _server(server)
    {
        setWindowTitle("Restore — " +
            QtUtils::toQString(server->connectionRecord()->getReadableName()));
        setRunButtonText("Restore");

        auto* form = formLayout();

        auto* srcRow = new QHBoxLayout;
        srcRow->addWidget(new QLabel("Source:", this));
        _sourceType = new QComboBox(this);
        _sourceType->addItem("Directory dump (mongorestore)");
        _sourceType->addItem("Gzip archive (mongorestore)");
        _sourceType->addItem("JSON file (mongoimport, single collection)");
        srcRow->addWidget(_sourceType, 1);
        form->addLayout(srcRow);

        auto* inRow = new QHBoxLayout;
        inRow->addWidget(new QLabel("Path:", this));
        _inputEdit = new QLineEdit(this);
        _inputEdit->setPlaceholderText("Choose the dump folder, archive or JSON file…");
        inRow->addWidget(_inputEdit, 1);
        _browseButton = new QPushButton("Browse…", this);
        inRow->addWidget(_browseButton);
        form->addLayout(inRow);

        // Target database for a single-database directory dump (loose .bson files).
        _targetDbRow = new QWidget(this);
        auto* dbRow = new QHBoxLayout(_targetDbRow);
        dbRow->setContentsMargins(0, 0, 0, 0);
        dbRow->addWidget(new QLabel("Restore into database:", _targetDbRow));
        _targetDbEdit = new QLineEdit(preselectDb, _targetDbRow);
        _targetDbEdit->setPlaceholderText("Leave empty for a full dump (per-database folders)");
        dbRow->addWidget(_targetDbEdit, 1);
        form->addWidget(_targetDbRow);

        _targetDbHint = new QLabel(this);
        _targetDbHint->setWordWrap(true);
        _targetDbHint->setEnabled(false);   // rendered muted
        form->addWidget(_targetDbHint);

        _gzipCheck = new QCheckBox("Directory dump is gzip-compressed", this);
        form->addWidget(_gzipCheck);

        _dropCheck = new QCheckBox("Drop each collection before restoring (--drop)", this);
        _dropCheck->setToolTip("Existing documents in the target collections are removed first.");
        form->addWidget(_dropCheck);

        // JSON (mongoimport) target + options.
        _jsonBox = new QGroupBox("JSON import target", this);
        auto* jsonForm = new QFormLayout(_jsonBox);
        _dbEdit = new QLineEdit(preselectDb, _jsonBox);
        _collEdit = new QLineEdit(_jsonBox);
        _modeCombo = new QComboBox(_jsonBox);
        _modeCombo->addItems({ "insert", "upsert", "merge" });
        _jsonArrayCheck = new QCheckBox("Input is a single JSON array", _jsonBox);
        jsonForm->addRow("Database:", _dbEdit);
        jsonForm->addRow("Collection:", _collEdit);
        jsonForm->addRow("Mode:", _modeCombo);
        jsonForm->addRow(_jsonArrayCheck);
        _jsonBox->setVisible(false);
        form->addWidget(_jsonBox);

        VERIFY(connect(_sourceType, QOverload<int>::of(&QComboBox::currentIndexChanged),
                       this, &RestoreDialog::updateSourceState));
        VERIFY(connect(_browseButton, &QPushButton::clicked, this, &RestoreDialog::browseInput));
        VERIFY(connect(_inputEdit, &QLineEdit::editingFinished,
                       this, &RestoreDialog::refreshDirTarget));

        updateSourceState();
    }

    RestoreDialog::SourceType RestoreDialog::sourceType() const
    {
        return static_cast<SourceType>(_sourceType->currentIndex());
    }

    void RestoreDialog::updateSourceState()
    {
        const bool json = jsonMode();
        const bool dir = (sourceType() == DirectoryDump);
        _gzipCheck->setVisible(dir);
        _dropCheck->setVisible(!json);   // mongoimport uses --drop/--mode instead
        // The target-database field only applies to a directory dump; an archive
        // and a JSON file carry (or specify) their own database.
        _targetDbRow->setVisible(dir);
        _jsonBox->setVisible(json);
        refreshDirTarget();
    }

    void RestoreDialog::browseInput()
    {
        QString path;
        switch (sourceType()) {
        case DirectoryDump:
            path = QFileDialog::getExistingDirectory(this, "Choose dump folder", _inputEdit->text());
            break;
        case GzipArchive:
            path = QFileDialog::getOpenFileName(this, "Choose archive", _inputEdit->text(),
                "Gzip archive (*.gz *.archive);;All files (*)");
            break;
        case JsonFile:
            path = QFileDialog::getOpenFileName(this, "Choose JSON file", _inputEdit->text(),
                "JSON files (*.json);;All files (*)");
            break;
        }
        if (!path.isEmpty()) {
            _inputEdit->setText(path);
            refreshDirTarget();
        }
    }

    bool RestoreDialog::dirIsSingleDbDump() const
    {
        const QString path = _inputEdit->text().trimmed();
        if (path.isEmpty()) return false;
        QDir dir(path);
        if (!dir.exists()) return false;
        // A single-database dump has loose .bson files directly inside; a full
        // dump root instead has one subdirectory per database.
        return !dir.entryList({ "*.bson" }, QDir::Files).isEmpty();
    }

    void RestoreDialog::refreshDirTarget()
    {
        if (sourceType() != DirectoryDump) {
            _targetDbHint->clear();
            return;
        }
        if (dirIsSingleDbDump()) {
            // Suggest the folder name as the destination database when the field
            // is still empty; the user can rename or clear it.
            if (_targetDbEdit->text().trimmed().isEmpty()) {
                const QString base = QFileInfo(_inputEdit->text().trimmed()).fileName();
                _targetDbEdit->setText(base);
            }
            _targetDbHint->setText(
                "This folder is a single-database dump — the documents will be "
                "restored into the database named above.");
        } else {
            _targetDbHint->clear();
        }
    }

    QString RestoreDialog::buildUri() const
    {
        ConnectionUri::Options opts;
        opts.includeDatabasePath = false;
        return QString::fromStdString(
            ConnectionUri::build(_server->connectionRecord(), {}, opts));
    }

    QString RestoreDialog::toolPath() const
    {
        return jsonMode() ? MongoTools::findMongoimport() : MongoTools::findMongorestore();
    }

    QString RestoreDialog::toolName() const
    {
        return jsonMode() ? "mongoimport" : "mongorestore";
    }

    bool RestoreDialog::validate(QString& error) const
    {
        if (_inputEdit->text().trimmed().isEmpty()) {
            error = "Choose a source to restore from.";
            return false;
        }
        if (jsonMode()) {
            if (_dbEdit->text().trimmed().isEmpty() || _collEdit->text().trimmed().isEmpty()) {
                error = "A JSON import needs a target database and collection.";
                return false;
            }
        } else if (sourceType() == DirectoryDump && dirIsSingleDbDump() &&
                   _targetDbEdit->text().trimmed().isEmpty()) {
            error = "This folder is a single-database dump (loose .bson files). "
                    "Enter the database to restore it into.";
            return false;
        }
        return true;
    }

    bool RestoreDialog::confirm()
    {
        // Restore/import is a mass write — always route through the production
        // write-guard so a guarded (e.g. production) target forces confirmation.
        return utils::confirmGuardedWrite(this, _server->connectionRecord(),
            "restore data into this connection", ScriptClassifier::WriteScope::Multi);
    }

    QList<QStringList> RestoreDialog::argumentSets() const
    {
        const QString uri = buildUri();
        if (jsonMode()) {
            ToolCommand::ImportSpec s;
            s.uri = uri;
            s.db = _dbEdit->text().trimmed();
            s.collection = _collEdit->text().trimmed();
            s.inFile = _inputEdit->text().trimmed();
            s.jsonArray = _jsonArrayCheck->isChecked();
            s.drop = false;                       // mongoimport: mode governs writes
            s.mode = _modeCombo->currentText();
            return { ToolCommand::buildImportArgs(s) };
        }

        ToolCommand::RestoreSpec s;
        s.uri = uri;
        s.layout = (sourceType() == GzipArchive)
                       ? ToolCommand::Layout::GzipArchive
                       : ToolCommand::Layout::Directory;
        s.inPath = _inputEdit->text().trimmed();
        // Only a single-database directory dump takes a target database; a full
        // dump root carries its own per-database folders, so --db must be omitted
        // there even if the field holds a leftover (pre-filled) name.
        if (sourceType() == DirectoryDump && dirIsSingleDbDump())
            s.db = _targetDbEdit->text().trimmed();
        s.drop = _dropCheck->isChecked();
        s.gzip = (sourceType() == DirectoryDump) && _gzipCheck->isChecked();
        return { ToolCommand::buildRestoreArgs(s) };
    }

    QString RestoreDialog::successTitle() const
    {
        return "Restore complete";
    }

    QString RestoreDialog::successMessage() const
    {
        return "The restore finished successfully. See the output for the number "
               "of documents restored.";
    }

    QString RestoreDialog::notFoundMessage() const
    {
        return "Could not find " + toolName() + ". Install the MongoDB Database "
               "Tools and set their folder in Preferences (or add them to your PATH).\n\n"
               "Download: https://www.mongodb.com/try/download/database-tools";
    }
}
