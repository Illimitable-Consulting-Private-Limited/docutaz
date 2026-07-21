#include "docutaz/gui/dialogs/BackupDialog.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>

#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/domain/MongoDatabase.h"
#include "docutaz/core/domain/MongoCollection.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/utils/ConnectionUri.h"
#include "docutaz/core/utils/ToolCommand.h"
#include "docutaz/core/utils/MongoTools.h"
#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{
    // Role storing whether a tree item is a database (top-level) row.
    namespace { const int kIsDatabaseRole = Qt::UserRole + 1; }

    BackupDialog::BackupDialog(MongoServer* server, const QString& preselectDb,
                               const QString& preselectCollection, QWidget* parent)
        : ToolRunnerDialog(parent), _server(server)
    {
        setWindowTitle("Backup — " +
            QtUtils::toQString(server->connectionRecord()->getReadableName()));
        setRunButtonText("Back Up");

        auto* form = formLayout();

        form->addWidget(new QLabel("Select databases and collections to back up:", this));
        _tree = new QTreeWidget(this);
        _tree->setHeaderHidden(true);
        _tree->setUniformRowHeights(true);
        // Cap the tree height so it doesn't starve the output log below it.
        _tree->setMinimumHeight(120);
        _tree->setMaximumHeight(220);
        form->addWidget(_tree);
        VERIFY(connect(_tree, &QTreeWidget::itemChanged, this, &BackupDialog::onItemChanged));

        // Output format.
        auto* fmtBox = new QGroupBox("Format", this);
        auto* fmtLayout = new QVBoxLayout(fmtBox);
        _dirRadio = new QRadioButton("Directory dump (full backup)", fmtBox);
        _dirRadio->setChecked(true);
        _archiveRadio = new QRadioButton("Single gzip archive (full backup)", fmtBox);
        _gzipCheck = new QCheckBox("Compress directory dump (gzip)", fmtBox);
        _jsonCheck = new QCheckBox("Export as JSON (data only — single collection)", fmtBox);
        _jsonCheck->setEnabled(false);
        _jsonCheck->setToolTip(
            "Available when exactly one collection is selected. JSON is data-only:\n"
            "no indexes or collection options, unlike a BSON dump.");
        fmtLayout->addWidget(_dirRadio);
        fmtLayout->addWidget(_archiveRadio);
        fmtLayout->addWidget(_gzipCheck);
        fmtLayout->addWidget(_jsonCheck);
        form->addWidget(fmtBox);

        // JSON sub-options, shown only in JSON mode.
        _jsonOptions = new QGroupBox("JSON options", this);
        auto* jsonLayout = new QVBoxLayout(_jsonOptions);
        _canonicalCheck = new QCheckBox("Canonical Extended JSON (lossless types)", _jsonOptions);
        _canonicalCheck->setChecked(true);
        _jsonArrayCheck = new QCheckBox("Single JSON array (else one document per line)", _jsonOptions);
        jsonLayout->addWidget(_canonicalCheck);
        jsonLayout->addWidget(_jsonArrayCheck);
        _jsonOptions->setVisible(false);
        form->addWidget(_jsonOptions);

        // Output destination.
        auto* outRow = new QHBoxLayout;
        outRow->addWidget(new QLabel("Output:", this));
        _outputEdit = new QLineEdit(this);
        _outputEdit->setPlaceholderText("Choose an output folder or file…");
        outRow->addWidget(_outputEdit, 1);
        _browseButton = new QPushButton("Browse…", this);
        outRow->addWidget(_browseButton);
        form->addLayout(outRow);
        VERIFY(connect(_browseButton, &QPushButton::clicked, this, &BackupDialog::browseOutput));

        VERIFY(connect(_dirRadio, &QRadioButton::toggled, this, &BackupDialog::updateFormatState));
        VERIFY(connect(_archiveRadio, &QRadioButton::toggled, this, &BackupDialog::updateFormatState));
        VERIFY(connect(_jsonCheck, &QCheckBox::toggled, this, &BackupDialog::updateFormatState));

        buildTree(preselectDb, preselectCollection);
        updateFormatState();
    }

    void BackupDialog::buildTree(const QString& preselectDb, const QString& preselectCollection)
    {
        _propagating = true;
        const bool serverLevel = preselectDb.isEmpty();
        for (MongoDatabase* db : _server->databases()) {
            const QString dbName = QtUtils::toQString(db->name());
            auto* dbItem = new QTreeWidgetItem(_tree);
            dbItem->setText(0, dbName);
            dbItem->setIcon(0, GuiRegistry::instance().databaseIcon());
            dbItem->setData(0, kIsDatabaseRole, true);
            dbItem->setFlags(dbItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);

            const bool dbSelected = serverLevel || (dbName == preselectDb);

            const auto& colls = db->collections();
            for (MongoCollection* c : colls) {
                const QString collName = QtUtils::toQString(c->name());
                auto* cItem = new QTreeWidgetItem(dbItem);
                cItem->setText(0, collName);
                cItem->setIcon(0, GuiRegistry::instance().collectionIcon());
                cItem->setFlags(cItem->flags() | Qt::ItemIsUserCheckable);
                const bool collSelected = dbSelected &&
                    (preselectCollection.isEmpty() || collName == preselectCollection);
                cItem->setCheckState(0, collSelected ? Qt::Checked : Qt::Unchecked);
            }

            if (colls.empty()) {
                // Collections not loaded — the database is a whole-DB leaf.
                dbItem->setCheckState(0, dbSelected ? Qt::Checked : Qt::Unchecked);
            }
            dbItem->setExpanded(dbSelected && !preselectCollection.isEmpty());
        }
        _propagating = false;
    }

    void BackupDialog::onItemChanged(QTreeWidgetItem* item, int column)
    {
        if (_propagating || column != 0) return;
        // Propagate a database check down to its collections (Qt only auto-updates
        // the parent tri-state from children, not the reverse).
        if (item->data(0, kIsDatabaseRole).toBool() && item->childCount() > 0) {
            const Qt::CheckState st = item->checkState(0);
            if (st == Qt::PartiallyChecked) return;   // user is toggling a child
            _propagating = true;
            for (int i = 0; i < item->childCount(); ++i)
                item->child(i)->setCheckState(0, st);
            _propagating = false;
        }
        updateFormatState();
    }

    int BackupDialog::selectedCollectionCount() const
    {
        int count = 0;
        for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* dbItem = _tree->topLevelItem(i);
            const Qt::CheckState st = dbItem->checkState(0);
            if (st == Qt::Unchecked) continue;
            if (dbItem->childCount() == 0) {
                // Whole DB with unknown collection set — not a single collection.
                return -1;
            }
            for (int j = 0; j < dbItem->childCount(); ++j)
                if (dbItem->child(j)->checkState(0) == Qt::Checked)
                    ++count;
        }
        return count;
    }

    bool BackupDialog::singleSelection(QString& db, QString& collection) const
    {
        for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* dbItem = _tree->topLevelItem(i);
            if (dbItem->checkState(0) == Qt::Unchecked) continue;
            for (int j = 0; j < dbItem->childCount(); ++j) {
                if (dbItem->child(j)->checkState(0) == Qt::Checked) {
                    db = dbItem->text(0);
                    collection = dbItem->child(j)->text(0);
                    return true;
                }
            }
        }
        return false;
    }

    bool BackupDialog::jsonMode() const
    {
        return _jsonCheck->isChecked() && _jsonCheck->isEnabled() &&
               selectedCollectionCount() == 1;
    }

    bool BackupDialog::anySelected() const
    {
        for (int i = 0; i < _tree->topLevelItemCount(); ++i)
            if (_tree->topLevelItem(i)->checkState(0) != Qt::Unchecked)
                return true;
        return false;
    }

    QString BackupDialog::archivePathForDb(const QString& db, bool multiple) const
    {
        const QString base = _outputEdit->text().trimmed();
        if (!multiple)
            return base;
        // Insert ".<db>" before the last extension so each DB gets its own file.
        const int dot = base.lastIndexOf('.');
        const int slash = base.lastIndexOf('/');
        if (dot > slash)
            return base.left(dot) + "." + db + base.mid(dot);
        return base + "." + db;
    }

    QString BackupDialog::buildUri() const
    {
        ConnectionUri::Options opts;
        opts.includeDatabasePath = false;   // tools select namespaces via flags
        return QString::fromStdString(
            ConnectionUri::build(_server->connectionRecord(), {}, opts));
    }

    void BackupDialog::updateFormatState()
    {
        const int count = selectedCollectionCount();
        const bool singleColl = (count == 1);
        _jsonCheck->setEnabled(singleColl);
        if (!singleColl && _jsonCheck->isChecked())
            _jsonCheck->setChecked(false);

        const bool json = jsonMode();
        _jsonOptions->setVisible(json);
        // BSON layout controls only matter for a dump.
        _dirRadio->setEnabled(!json);
        _archiveRadio->setEnabled(!json);
        _gzipCheck->setEnabled(!json && _dirRadio->isChecked());
    }

    void BackupDialog::browseOutput()
    {
        QString path;
        if (jsonMode()) {
            path = QFileDialog::getSaveFileName(this, "Save JSON export as",
                _outputEdit->text(), "JSON files (*.json);;All files (*)");
        } else if (_archiveRadio->isChecked()) {
            path = QFileDialog::getSaveFileName(this, "Save archive as",
                _outputEdit->text(), "Gzip archive (*.gz *.archive);;All files (*)");
        } else {
            path = QFileDialog::getExistingDirectory(this, "Choose output folder",
                _outputEdit->text());
        }
        if (!path.isEmpty())
            _outputEdit->setText(path);
    }

    QString BackupDialog::toolPath() const
    {
        return jsonMode() ? MongoTools::findMongoexport() : MongoTools::findMongodump();
    }

    QString BackupDialog::toolName() const
    {
        return jsonMode() ? "mongoexport" : "mongodump";
    }

    bool BackupDialog::validate(QString& error) const
    {
        if (!anySelected()) {
            error = "Select at least one database or collection to back up.";
            return false;
        }
        if (_outputEdit->text().trimmed().isEmpty()) {
            error = "Choose an output destination.";
            return false;
        }
        return true;
    }

    QList<QStringList> BackupDialog::argumentSets() const
    {
        const QString uri = buildUri();

        // JSON (data-only) export of the single selected collection.
        if (jsonMode()) {
            QString db, coll;
            singleSelection(db, coll);
            ToolCommand::ExportSpec s;
            s.uri = uri;
            s.db = db;
            s.collection = coll;
            s.outFile = _outputEdit->text().trimmed();
            s.canonical = _canonicalCheck->isChecked();
            s.jsonArray = _jsonArrayCheck->isChecked();
            return { ToolCommand::buildExportArgs(s) };
        }

        // BSON dump: mongodump handles one database per run, so build one
        // invocation per selected database (all writing into the same output).
        const bool archive = _archiveRadio->isChecked();

        // Count selected databases up front (drives per-DB archive naming).
        int dbCount = 0;
        for (int i = 0; i < _tree->topLevelItemCount(); ++i)
            if (_tree->topLevelItem(i)->checkState(0) != Qt::Unchecked)
                ++dbCount;

        QList<QStringList> sets;
        for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* dbItem = _tree->topLevelItem(i);
            const Qt::CheckState st = dbItem->checkState(0);
            if (st == Qt::Unchecked) continue;
            const QString dbName = dbItem->text(0);

            ToolCommand::DumpSpec s;
            s.uri = uri;
            s.db = dbName;
            s.layout = archive ? ToolCommand::Layout::GzipArchive
                               : ToolCommand::Layout::Directory;
            s.outPath = archive ? archivePathForDb(dbName, dbCount > 1)
                                : _outputEdit->text().trimmed();
            s.gzip = _gzipCheck->isChecked();

            if (st == Qt::PartiallyChecked && dbItem->childCount() > 0) {
                // Subset of collections: one selected → --collection; several →
                // exclude the unselected ones.
                QStringList selected, unselected;
                for (int j = 0; j < dbItem->childCount(); ++j) {
                    QTreeWidgetItem* c = dbItem->child(j);
                    (c->checkState(0) == Qt::Checked ? selected : unselected)
                        << c->text(0);
                }
                if (selected.size() == 1)
                    s.collection = selected.first();
                else
                    s.excludeCollections = unselected;
            }
            sets << ToolCommand::buildDumpArgs(s);
        }
        return sets;
    }

    void BackupDialog::onSucceeded()
    {
        appendLog("\nBackup written to: " + _outputEdit->text().trimmed());
    }

    QString BackupDialog::successTitle() const
    {
        return "Backup complete";
    }

    QString BackupDialog::successMessage() const
    {
        return "The backup finished successfully.\n\nWritten to:\n" +
               _outputEdit->text().trimmed();
    }

    QString BackupDialog::notFoundMessage() const
    {
        return "Could not find " + toolName() + ". Install the MongoDB Database "
               "Tools and set their folder in Preferences (or add them to your PATH).\n\n"
               "Download: https://www.mongodb.com/try/download/database-tools";
    }
}
