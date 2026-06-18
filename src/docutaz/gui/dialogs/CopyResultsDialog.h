#pragma once

#include <QDialog>
#include <vector>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
QT_END_NAMESPACE

namespace Docutaz
{
    class ConnectionSettings;

    // Collects the target of a "copy query results" operation: which connection
    // to write to, the destination db/collection (both editable; the collection
    // may not exist yet), a row limit and whether to drop the target first.
    //
    // Safety: the dialog never writes anything itself — it only gathers choices.
    // It blocks accepting a target that needs an SSH tunnel (unsupported in v1,
    // since the generated script runs inside the source connection's shell), and
    // requires a typed confirmation when the chosen target is tagged
    // production/staging so prod writes are always deliberate.
    class CopyResultsDialog : public QDialog
    {
        Q_OBJECT

    public:
        CopyResultsDialog(ConnectionSettings *source, const QString &sourceDb,
                          const QString &sourceCollection,
                          const std::vector<ConnectionSettings *> &connections,
                          QWidget *parent = nullptr);

        ConnectionSettings *targetConnection() const;
        QString targetDatabase() const;
        QString targetCollection() const;
        int limit() const;      // 0 == no limit
        bool dropFirst() const;
        bool copyIndexes() const;

    public Q_SLOTS:
        void accept() override;

    private:
        ConnectionSettings *_source;
        QString _sourceDb;
        QString _sourceCollection;
        std::vector<ConnectionSettings *> _connections;

        QComboBox *_connectionCombo;
        QLineEdit *_dbEdit;
        QLineEdit *_collectionEdit;
        QSpinBox  *_limitSpin;
        QCheckBox *_dropCheck;
        QCheckBox *_indexCheck;
    };
}
