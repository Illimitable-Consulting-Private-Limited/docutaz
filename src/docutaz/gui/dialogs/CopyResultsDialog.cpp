#include "docutaz/gui/dialogs/CopyResultsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/settings/SshSettings.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/gui/ConnectionEnvironment.h"

namespace Docutaz
{
    namespace
    {
        // A small coloured dot + label describing a connection's environment tag,
        // so the user can see at a glance whether a target is prod/staging.
        QString envSuffix(const ConnectionSettings *conn)
        {
            const std::string env = conn->environment();
            if (env.empty())
                return QString();
            return QString("  [%1]").arg(ConnectionEnvironment::displayName(env));
        }
    }

    CopyResultsDialog::CopyResultsDialog(ConnectionSettings *source, const QString &sourceDb,
                                         const QString &sourceCollection,
                                         const std::vector<CopyTarget> &openTargets,
                                         QWidget *parent)
        : QDialog(parent), _source(source), _sourceDb(sourceDb),
          _sourceCollection(sourceCollection), _targets(openTargets)
    {
        setWindowTitle("Copy Results To…");
        setMinimumWidth(460);

        // "From" — read-only summary of what is being copied.
        QString fromText = QString("%1  —  %2.%3")
            .arg(QtUtils::toQString(source->getReadableName()), sourceDb, sourceCollection);
        QLabel *fromLabel = new QLabel(fromText, this);
        fromLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        const QColor envColor = ConnectionEnvironment::color(source->environment());
        if (envColor.isValid())
            fromLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(envColor.name()));
        else
            fromLabel->setStyleSheet("font-weight: bold;");

        // "To" — target connection (only open ones) / db / collection / limit.
        _connectionCombo = new QComboBox(this);
        int sourceIdx = 0;
        for (size_t i = 0; i < _targets.size(); ++i) {
            ConnectionSettings *c = _targets[i].connection;
            _connectionCombo->addItem(QtUtils::toQString(c->getReadableName()) + envSuffix(c),
                                      QVariant(static_cast<int>(i)));
            if (c == _source || (!c->uuid().isEmpty() && c->uuid() == _source->uuid()))
                sourceIdx = static_cast<int>(i);
        }
        _connectionCombo->setCurrentIndex(sourceIdx);

        // Editable so the user can pick one of the selected connection's databases
        // or type a new one (the target db may not exist yet). Filled in by
        // onConnectionChanged() to track the chosen connection.
        _dbCombo = new QComboBox(this);
        _dbCombo->setEditable(true);

        _collectionEdit = new QLineEdit(sourceCollection, this);

        _limitSpin = new QSpinBox(this);
        _limitSpin->setRange(0, 100000000);
        _limitSpin->setValue(1000);
        _limitSpin->setSpecialValueText("No limit");   // shown when value == 0
        _limitSpin->setToolTip("Maximum number of documents to copy. 0 = no limit.");

        _dropCheck = new QCheckBox("Drop the target collection before copying", this);
        _dropCheck->setChecked(false);

        _indexCheck = new QCheckBox("Copy indexes (skips any already on the target)", this);
        _indexCheck->setChecked(false);
        _indexCheck->setToolTip(
            "Recreate the source collection's indexes on the target. A TTL index is "
            "copied as-is, so copied data will expire per its expireAfterSeconds.");

        QFormLayout *form = new QFormLayout();
        form->addRow("From:", fromLabel);

        QFrame *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        form->addRow(sep);

        form->addRow("To connection:", _connectionCombo);
        form->addRow("Target database:", _dbCombo);
        form->addRow("Target collection:", _collectionEdit);
        form->addRow("Limit:", _limitSpin);
        form->addRow("", _dropCheck);
        form->addRow("", _indexCheck);

        QLabel *note = new QLabel(
            "Only currently open connections are listed — open another connection "
            "first to copy into it. Documents matching the query are read from the "
            "source and inserted into the target (created if it doesn't exist); the "
            "source is never modified.", this);
        note->setWordWrap(true);
        note->setStyleSheet("color: gray;");

        QDialogButtonBox *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setText("Copy");
        VERIFY(connect(buttons, SIGNAL(accepted()), this, SLOT(accept())));
        VERIFY(connect(buttons, SIGNAL(rejected()), this, SLOT(reject())));
        VERIFY(connect(_connectionCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onConnectionChanged())));

        QVBoxLayout *root = new QVBoxLayout(this);
        root->addLayout(form);
        root->addWidget(note);
        root->addWidget(buttons);
        setLayout(root);

        onConnectionChanged();   // seed the database dropdown for the initial target
    }

    void CopyResultsDialog::onConnectionChanged()
    {
        const int idx = _connectionCombo->currentData().toInt();
        if (idx < 0 || idx >= static_cast<int>(_targets.size()))
            return;

        // Preserve whatever the user has typed; otherwise default to the source
        // db name (the common case: copy into a same-named db on the target).
        const QString keep = _dbCombo->currentText().trimmed();
        const QString desired = keep.isEmpty() ? _sourceDb : keep;

        _dbCombo->clear();
        _dbCombo->addItems(_targets[idx].databases);
        if (!desired.isEmpty() && _dbCombo->findText(desired) < 0)
            _dbCombo->insertItem(0, desired);
        _dbCombo->setCurrentText(desired);
    }

    ConnectionSettings *CopyResultsDialog::targetConnection() const
    {
        const int idx = _connectionCombo->currentData().toInt();
        if (idx < 0 || idx >= static_cast<int>(_targets.size()))
            return nullptr;
        return _targets[idx].connection;
    }

    QString CopyResultsDialog::targetDatabase() const { return _dbCombo->currentText().trimmed(); }
    QString CopyResultsDialog::targetCollection() const { return _collectionEdit->text().trimmed(); }
    int CopyResultsDialog::limit() const { return _limitSpin->value(); }
    bool CopyResultsDialog::dropFirst() const { return _dropCheck->isChecked(); }
    bool CopyResultsDialog::copyIndexes() const { return _indexCheck->isChecked(); }

    void CopyResultsDialog::accept()
    {
        ConnectionSettings *target = targetConnection();
        if (!target)
            return;

        if (targetDatabase().isEmpty()) {
            QMessageBox::warning(this, "Copy Results", "Please enter a target database name.");
            return;
        }
        if (targetCollection().isEmpty()) {
            QMessageBox::warning(this, "Copy Results", "Please enter a target collection name.");
            return;
        }

        const bool sameConnection =
            target == _source || (!target->uuid().isEmpty() && target->uuid() == _source->uuid());

        // Can't copy a collection onto itself: same connection + same db + same
        // collection would just re-insert into the source (duplicate-key errors).
        if (sameConnection && targetDatabase() == _sourceDb &&
            targetCollection() == _sourceCollection) {
            QMessageBox::warning(this, "Copy Results",
                "The target is the same collection as the source. Choose a "
                "different target collection (or a different database/connection).");
            return;
        }

        // The copy script runs inside the source connection's shell, which can't
        // route a second connection through the target's SSH tunnel. Block that
        // combination rather than fail confusingly at run time.
        if (!sameConnection && target->sshSettings() && target->sshSettings()->enabled()) {
            QMessageBox::warning(this, "Copy Results",
                "The selected target connection uses an SSH tunnel, which isn't "
                "supported for copying yet. Pick a directly reachable target "
                "(or the same connection as the source).");
            return;
        }

        // Writing into a production/staging-tagged connection must be deliberate:
        // require the user to type its name to confirm.
        const std::string env = target->environment();
        if (env == "production" || env == "staging") {
            const QString name = QtUtils::toQString(target->getReadableName());
            bool ok = false;
            const QString typed = QInputDialog::getText(this, "Confirm write to " +
                ConnectionEnvironment::displayName(env) + " connection",
                QString("You are about to write to \"%1\", tagged %2.\n\n"
                        "Type the connection name to confirm:")
                    .arg(name, ConnectionEnvironment::displayName(env)),
                QLineEdit::Normal, QString(), &ok);
            if (!ok)
                return;
            if (typed.trimmed() != name) {
                QMessageBox::warning(this, "Copy Results",
                    "The name didn't match — copy cancelled.");
                return;
            }
        }

        QDialog::accept();
    }
}
