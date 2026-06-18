#include "docutaz/gui/dialogs/ExportResultsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{
    ExportResultsDialog::ExportResultsDialog(const QString &sourceLabel,
                                             const QString &defaultBaseName, QWidget *parent)
        : QDialog(parent), _baseName(defaultBaseName.isEmpty() ? "export" : defaultBaseName)
    {
        setWindowTitle("Export Results");
        setMinimumWidth(480);

        QLabel *from = new QLabel(sourceLabel, this);
        from->setStyleSheet("font-weight: bold;");
        from->setTextInteractionFlags(Qt::TextSelectableByMouse);

        _formatCombo = new QComboBox(this);
        _formatCombo->addItem("JSON (.json)", static_cast<int>(ExportFormat::Json));
        _formatCombo->addItem("CSV (.csv)",  static_cast<int>(ExportFormat::Csv));

        _pathEdit = new QLineEdit(this);
        QPushButton *browse = new QPushButton("Browse…", this);
        QHBoxLayout *pathRow = new QHBoxLayout();
        pathRow->setContentsMargins(0, 0, 0, 0);
        pathRow->addWidget(_pathEdit);
        pathRow->addWidget(browse);

        _limitSpin = new QSpinBox(this);
        _limitSpin->setRange(0, 100000000);
        _limitSpin->setValue(0);
        _limitSpin->setSpecialValueText("All");     // shown when value == 0
        _limitSpin->setToolTip("Maximum documents to export. 0 = all matching documents.");

        // JSON-only options
        _jsonGroup = new QGroupBox("JSON options", this);
        _jsonShapeCombo = new QComboBox(_jsonGroup);
        _jsonShapeCombo->addItem("Array  [ {…}, {…} ]");
        _jsonShapeCombo->addItem("One document per line (JSONL)");
        QFormLayout *jsonForm = new QFormLayout(_jsonGroup);
        jsonForm->addRow("Shape:", _jsonShapeCombo);

        // CSV-only options
        _csvGroup = new QGroupBox("CSV options", this);
        _csvNestedCombo = new QComboBox(_csvGroup);
        _csvNestedCombo->addItem("Nested objects/arrays as JSON text");
        _csvNestedCombo->addItem("Flatten nested objects (dot notation)");
        QFormLayout *csvForm = new QFormLayout(_csvGroup);
        csvForm->addRow("Nested:", _csvNestedCombo);

        QFormLayout *form = new QFormLayout();
        form->addRow("From:", from);
        QFrame *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        form->addRow(sep);
        form->addRow("Format:", _formatCombo);
        form->addRow("File:", pathRow);
        form->addRow("Limit:", _limitSpin);

        QDialogButtonBox *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setText("Export");

        QVBoxLayout *root = new QVBoxLayout(this);
        root->addLayout(form);
        root->addWidget(_jsonGroup);
        root->addWidget(_csvGroup);
        root->addWidget(buttons);

        VERIFY(connect(buttons, SIGNAL(accepted()), this, SLOT(accept())));
        VERIFY(connect(buttons, SIGNAL(rejected()), this, SLOT(reject())));
        VERIFY(connect(browse, SIGNAL(clicked()), this, SLOT(onBrowse())));
        VERIFY(connect(_formatCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onFormatChanged())));

        onFormatChanged();   // sets initial path + option visibility
    }

    QString ExportResultsDialog::defaultExtension() const
    {
        return format() == ExportFormat::Csv ? "csv" : "json";
    }

    void ExportResultsDialog::onFormatChanged()
    {
        const bool isJson = format() == ExportFormat::Json;
        _jsonGroup->setVisible(isJson);
        _csvGroup->setVisible(!isJson);

        // Keep the file path's extension in sync with the chosen format. If the
        // user hasn't typed a path yet, seed one from the collection name.
        QString path = _pathEdit->text().trimmed();
        if (path.isEmpty())
            path = _baseName + "." + defaultExtension();
        else {
            QFileInfo fi(path);
            path = (fi.path() == "." ? fi.completeBaseName()
                                     : fi.path() + "/" + fi.completeBaseName()) +
                   "." + defaultExtension();
        }
        _pathEdit->setText(path);
        adjustSize();
    }

    void ExportResultsDialog::onBrowse()
    {
        const QString filter = format() == ExportFormat::Csv ? "CSV files (*.csv)"
                                                             : "JSON files (*.json)";
        const QString start = _pathEdit->text().trimmed().isEmpty()
            ? _baseName + "." + defaultExtension()
            : _pathEdit->text().trimmed();
        const QString chosen = QFileDialog::getSaveFileName(this, "Export to file", start, filter);
        if (!chosen.isEmpty())
            _pathEdit->setText(chosen);
    }

    ExportFormat ExportResultsDialog::format() const
    {
        return static_cast<ExportFormat>(_formatCombo->currentData().toInt());
    }

    QString ExportResultsDialog::filePath() const { return _pathEdit->text().trimmed(); }
    long long ExportResultsDialog::limit() const { return _limitSpin->value(); }
    bool ExportResultsDialog::jsonArray() const { return _jsonShapeCombo->currentIndex() == 0; }
    bool ExportResultsDialog::flattenNested() const { return _csvNestedCombo->currentIndex() == 1; }

    void ExportResultsDialog::accept()
    {
        if (filePath().isEmpty()) {
            QMessageBox::warning(this, "Export", "Please choose a file to export to.");
            return;
        }
        QDialog::accept();
    }
}
