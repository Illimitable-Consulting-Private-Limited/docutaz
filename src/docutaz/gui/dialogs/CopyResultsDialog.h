#pragma once

#include <QDialog>
#include <QStringList>
#include <vector>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QCheckBox;
QT_END_NAMESPACE

namespace Docutaz
{
    class ConnectionSettings;

    // Collects the target of a "copy query results" operation: which connection
    // to write to, the destination database (a dropdown of that connection's
    // databases, editable for a new one) and collection (may not exist yet), a
    // row limit and whether to drop the target first.
    //
    // Only currently open connections are offered as targets — so the database
    // dropdown can always list real databases, and the copy (which connects to
    // the target from the source's shell) targets something already reachable.
    //
    // Safety: the dialog never writes anything itself — it only gathers choices.
    // It blocks accepting a target that needs an SSH tunnel (unsupported here,
    // since the generated script runs inside the source connection's shell), and
    // requires a typed confirmation when the chosen target is tagged
    // production/staging so prod writes are always deliberate.
    class CopyResultsDialog : public QDialog
    {
        Q_OBJECT

    public:
        // One selectable destination: an open connection and its databases.
        struct CopyTarget
        {
            ConnectionSettings *connection = nullptr;
            QStringList databases;
        };

        CopyResultsDialog(ConnectionSettings *source, const QString &sourceDb,
                          const QString &sourceCollection,
                          const std::vector<CopyTarget> &openTargets,
                          QWidget *parent = nullptr);

        ConnectionSettings *targetConnection() const;
        QString targetDatabase() const;
        QString targetCollection() const;
        int limit() const;      // 0 == no limit
        bool dropFirst() const;
        bool copyIndexes() const;

        // Non-interactive validation of the current selection. Returns an empty
        // string when the inputs are acceptable, otherwise a user-facing reason
        // (empty db/collection, copying a collection onto itself, SSH-tunnelled
        // cross-connection target). Separated from accept() so it can be tested
        // without a modal popup. The production/staging typed-confirmation is
        // interactive and stays in accept().
        QString validate() const;

    public Q_SLOTS:
        void accept() override;

    private Q_SLOTS:
        // Repopulate the database dropdown from the selected target connection.
        void onConnectionChanged();

    private:
        ConnectionSettings *_source;
        QString _sourceDb;
        QString _sourceCollection;
        std::vector<CopyTarget> _targets;

        QComboBox *_connectionCombo;
        QComboBox *_dbCombo;        // editable: pick a known db or type a new one
        QLineEdit *_collectionEdit;
        QSpinBox  *_limitSpin;
        QCheckBox *_dropCheck;
        QCheckBox *_indexCheck;
    };
}
