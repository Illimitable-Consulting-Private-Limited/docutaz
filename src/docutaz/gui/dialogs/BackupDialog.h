#pragma once

#include "docutaz/gui/dialogs/ToolRunnerDialog.h"

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
class QRadioButton;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QGroupBox;
QT_END_NAMESPACE

namespace Docutaz
{
    class MongoServer;

    // Backup one or more databases/collections with mongodump (BSON, full
    // fidelity). When exactly one collection is selected, an optional
    // "JSON (data only)" mode runs mongoexport instead. Launched from the server
    // node (all databases), a database node, or a collection node — the matching
    // node is pre-selected.
    class BackupDialog : public ToolRunnerDialog
    {
        Q_OBJECT

    public:
        BackupDialog(MongoServer* server,
                     const QString& preselectDb = {},
                     const QString& preselectCollection = {},
                     QWidget* parent = nullptr);

    protected:
        QString toolPath() const override;
        QString toolName() const override;
        bool validate(QString& error) const override;
        QList<QStringList> argumentSets() const override;
        void onSucceeded() override;
        QString successTitle() const override;
        QString successMessage() const override;
        QString notFoundMessage() const override;

    private Q_SLOTS:
        void onItemChanged(QTreeWidgetItem* item, int column);
        void updateFormatState();
        void browseOutput();

    private:
        void buildTree(const QString& preselectDb, const QString& preselectCollection);
        // Number of individually-selected collections across the tree; -1 when a
        // whole database with an unknown (unloaded) collection set is selected
        // (which disqualifies single-collection JSON mode).
        int selectedCollectionCount() const;
        // When exactly one collection is selected, its (db, collection).
        bool singleSelection(QString& db, QString& collection) const;
        bool jsonMode() const;
        bool anySelected() const;
        QString buildUri() const;
        // Per-database archive filename when more than one DB is dumped to an
        // archive (inserts ".<db>" before the extension); the plain path otherwise.
        QString archivePathForDb(const QString& db, bool multiple) const;

        MongoServer* _server;

        QTreeWidget*  _tree = nullptr;
        bool          _propagating = false;

        QRadioButton* _dirRadio = nullptr;
        QRadioButton* _archiveRadio = nullptr;
        QCheckBox*    _gzipCheck = nullptr;
        QCheckBox*    _jsonCheck = nullptr;
        QGroupBox*    _jsonOptions = nullptr;
        QCheckBox*    _canonicalCheck = nullptr;
        QCheckBox*    _jsonArrayCheck = nullptr;
        QLineEdit*    _outputEdit = nullptr;
        QPushButton*  _browseButton = nullptr;
    };
}
