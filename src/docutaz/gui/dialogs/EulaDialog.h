#pragma once

#include <QWizard>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QDialogButtonBox;
class QComboBox;
QT_END_NAMESPACE

namespace Docutaz
{

    class EulaDialog : public QWizard
    {
        Q_OBJECT

    public:
        static const QSize minimumSize;

        explicit EulaDialog(QWidget *parent = nullptr);

    public Q_SLOTS:
        void accept() override;
        void reject() override;

    protected:
        void closeEvent(QCloseEvent *event) override;

    private Q_SLOTS:
        void on_agreeButton_clicked();
        void on_notAgreeButton_clicked();
        void on_finish_clicked();

    private:
        void restoreWindowSettings();
        void saveWindowSettings() const;
    };
}

