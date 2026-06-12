#include "docutaz/gui/dialogs/EulaDialog.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QSettings>
#include <QLabel>
#include <QTextBrowser>
#include <QLineEdit>
#include <QRadioButton>
#include <QScreen>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/utils/Logger.h"
#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{
    EulaDialog::EulaDialog(QWidget *parent)
        : QWizard(parent)
    {
        setWindowTitle("EULA");

        //// First page
        auto firstPage = new QWizardPage;

        auto agreeButton = new QRadioButton("I agree");
        VERIFY(connect(agreeButton, SIGNAL(clicked()),
            this, SLOT(on_agreeButton_clicked())));

        auto notAgreeButton = new QRadioButton("I don't agree");
        notAgreeButton->setChecked(true);
        VERIFY(connect(notAgreeButton, SIGNAL(clicked()),
            this, SLOT(on_notAgreeButton_clicked())));

        auto radioButtonsLay = new QHBoxLayout;
        radioButtonsLay->setAlignment(Qt::AlignHCenter);
        radioButtonsLay->setSpacing(30);
        radioButtonsLay->addWidget(agreeButton);
        radioButtonsLay->addWidget(notAgreeButton);

        auto textBrowser = new QTextBrowser;
        textBrowser->setOpenExternalLinks(true);
        textBrowser->setOpenLinks(true);
        QFile file(":/docutaz/gnu_gpl3_license.html");
        if (file.open(QFile::ReadOnly | QFile::Text))
            textBrowser->setText(file.readAll());

        auto hline = new QFrame();
        hline->setFrameShape(QFrame::HLine);
        hline->setFrameShadow(QFrame::Sunken);

        auto mainLayout1 = new QVBoxLayout();
        mainLayout1->addWidget(new QLabel("<h3>End-User License Agreement</h3>"));
        mainLayout1->addWidget(new QLabel(""));
        mainLayout1->addWidget(textBrowser);
        mainLayout1->addWidget(new QLabel(""));
        mainLayout1->addLayout(radioButtonsLay, Qt::AlignCenter);
        mainLayout1->addWidget(new QLabel(""));
        mainLayout1->addWidget(hline);

        firstPage->setLayout(mainLayout1);

        addPage(firstPage);

        //// Buttons
        setButtonText(QWizard::CustomButton3, tr("Finish"));

        VERIFY(connect(button(QWizard::CustomButton3), SIGNAL(clicked()), this, SLOT(on_finish_clicked())));

        setButtonLayout(QList<WizardButton>{ QWizard::Stretch, QWizard::CancelButton, QWizard::CustomButton3});

        button(QWizard::CustomButton3)->setDisabled(true);

        setWizardStyle(QWizard::ModernStyle);

        QSettings const settings("Docutaz", "Docutaz");
        if (settings.contains("EulaDialog/size")) {
            restoreWindowSettings();
        }
        else {
            const auto mainScreenSize = QGuiApplication::primaryScreen()->availableGeometry().size();
            resize(mainScreenSize.width()*0.5, mainScreenSize.height()*0.6);
        }
    }

    void EulaDialog::accept()
    {
        saveWindowSettings();
        QDialog::accept();
    }

    void EulaDialog::reject()
    {
        saveWindowSettings();
        QDialog::reject();
    }

    void EulaDialog::closeEvent(QCloseEvent *event)
    {
        saveWindowSettings();
        QWidget::closeEvent(event);
    }

    void EulaDialog::on_agreeButton_clicked()
    {
        button(QWizard::CustomButton3)->setEnabled(true);
    }

    void EulaDialog::on_notAgreeButton_clicked()
    {
        button(QWizard::CustomButton3)->setEnabled(false);
    }

    void EulaDialog::on_finish_clicked()
    {
        accept();
    }

    void EulaDialog::saveWindowSettings() const
    {
        QSettings settings("Docutaz", "Docutaz");
        settings.setValue("EulaDialog/size", size());
    }

    void EulaDialog::restoreWindowSettings()
    {
        QSettings settings("Docutaz", "Docutaz");
        resize(settings.value("EulaDialog/size").toSize());
    }

}
