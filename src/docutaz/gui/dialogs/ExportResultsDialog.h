#pragma once

#include <QDialog>

#include "docutaz/core/utils/Exporter.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QSpinBox;
class QGroupBox;
QT_END_NAMESPACE

namespace Docutaz
{
    // Collects the destination + options for exporting a query's results to a
    // file. Gathers choices only — the worker does the actual streaming write.
    //
    // Phase 1 supports JSON (array or JSONL) and CSV (nested as a JSON string or
    // dot-notation flatten). Excel (.xlsx) is added in phase 2.
    class ExportResultsDialog : public QDialog
    {
        Q_OBJECT

    public:
        // sourceLabel: read-only "conn — db.collection" shown at the top.
        // defaultBaseName: file-name stem (typically the collection name).
        ExportResultsDialog(const QString &sourceLabel, const QString &defaultBaseName,
                            QWidget *parent = nullptr);

        ExportFormat format() const;
        QString filePath() const;
        long long limit() const;       // 0 == all
        bool jsonArray() const;        // JSON: true = array, false = JSONL
        bool flattenNested() const;    // CSV: true = dot-flatten, false = JSON string

    public Q_SLOTS:
        void accept() override;

    private Q_SLOTS:
        void onFormatChanged();
        void onBrowse();

    private:
        QString defaultExtension() const;

        QString _baseName;
        QComboBox *_formatCombo;
        QLineEdit *_pathEdit;
        QSpinBox  *_limitSpin;
        QGroupBox *_jsonGroup;
        QComboBox *_jsonShapeCombo;
        QGroupBox *_csvGroup;
        QComboBox *_csvNestedCombo;
    };
}
