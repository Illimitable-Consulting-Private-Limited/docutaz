#include "docutaz/gui/dialogs/PreferencesDialog.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QFileDialog>

#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/AppStyle.h"
#include "docutaz/gui/utils/ComboBoxUtils.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"

namespace Docutaz
{
    PreferencesDialog::PreferencesDialog(QWidget *parent)
        : BaseClass(parent)
    {
        setWindowIcon(GuiRegistry::instance().mainWindowIcon());

        setWindowTitle("Preferences " PROJECT_NAME_TITLE);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
        setFixedSize(height, width);

        QVBoxLayout *layout = new QVBoxLayout(this);

        QHBoxLayout *defLayout = new QHBoxLayout(this);
        QLabel *defDisplayModeLabel = new QLabel("Default display mode:");
        defLayout->addWidget(defDisplayModeLabel);
        _defDisplayModeComboBox = new QComboBox();
        QStringList modes;
        for (int i = Text; i <= Custom; ++i)
        {
            modes.append(convertViewModeToString(static_cast<ViewMode>(i)));
        }
        _defDisplayModeComboBox->addItems(modes);
        defLayout->addWidget(_defDisplayModeComboBox);
        layout->addLayout(defLayout);

        QHBoxLayout *timeZoneLayout = new QHBoxLayout(this);
        QLabel *timeZoneLabel = new QLabel("Display Dates in:");
        timeZoneLayout->addWidget(timeZoneLabel);
        _timeZoneComboBox = new QComboBox();
        QStringList times;
        for (int i = Utc; i <= LocalTime; ++i)
        {
            times.append(convertTimesToString(static_cast<SupportedTimes>(i)));
        }
        _timeZoneComboBox->addItems(times);
        timeZoneLayout->addWidget(_timeZoneComboBox);
        layout->addLayout(timeZoneLayout);

        QHBoxLayout *uuidEncodingLayout = new QHBoxLayout(this);
        QLabel *uuidEncodingLabel = new QLabel("Legacy UUID Encoding:");
        uuidEncodingLayout->addWidget(uuidEncodingLabel);
        _uuidEncodingComboBox = new QComboBox();
        QStringList uuids;
        for (int i = DefaultEncoding; i <= PythonLegacy; ++i)
        {
            uuids.append(convertUUIDEncodingToString(static_cast<UUIDEncoding>(i)));
        }
        _uuidEncodingComboBox->addItems(uuids);
        uuidEncodingLayout->addWidget(_uuidEncodingComboBox);
        layout->addLayout(uuidEncodingLayout);        

        _loadMongoRcJsCheckBox = new QCheckBox("Load .mongorc.js");
        layout->addWidget(_loadMongoRcJsCheckBox);

        _disabelConnectionShortcutsCheckBox = new QCheckBox("Disable connection shortcuts");
        layout->addWidget(_disabelConnectionShortcutsCheckBox);

        _shareShellPerConnectionCheckBox = new QCheckBox("Share one shell process across a connection's tabs");
        _shareShellPerConnectionCheckBox->setToolTip(
            "Reuse a single mongosh process for all tabs of the same connection.\n"
            "Lower memory and instant tab opening, but queries serialize — a long\n"
            "query in one tab blocks its sibling tabs. Takes effect for new tabs.");
        layout->addWidget(_shareShellPerConnectionCheckBox);

        QHBoxLayout *stylesLayout = new QHBoxLayout(this);
        QLabel *stylesLabel = new QLabel("Styles:");
        stylesLayout->addWidget(stylesLabel);
        _stylesComboBox = new QComboBox();
        _stylesComboBox->addItems(AppStyleUtils::getSupportedStyles());
        stylesLayout->addWidget(_stylesComboBox);
        layout->addLayout(stylesLayout);

        QHBoxLayout *mongoshLayout = new QHBoxLayout(this);
        QLabel *mongoshLabel = new QLabel("mongosh path:");
        mongoshLayout->addWidget(mongoshLabel);
        _mongoshPathEdit = new QLineEdit();
        _mongoshPathEdit->setPlaceholderText("Auto-detect (leave empty)");
        mongoshLayout->addWidget(_mongoshPathEdit);
        QPushButton *mongoshBrowseButton = new QPushButton("Browse...");
        VERIFY(connect(mongoshBrowseButton, SIGNAL(clicked()), this, SLOT(browseMongoshPath())));
        mongoshLayout->addWidget(mongoshBrowseButton);
        layout->addLayout(mongoshLayout);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
        VERIFY(connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept())));
        VERIFY(connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject())));
        layout->addWidget(buttonBox);
        setLayout(layout);

        syncWithSettings();
    }

    void PreferencesDialog::syncWithSettings()
    {

        utils::setCurrentText(_defDisplayModeComboBox, convertViewModeToString(Docutaz::AppRegistry::instance().settingsManager()->viewMode()));
        utils::setCurrentText(_timeZoneComboBox, convertTimesToString(Docutaz::AppRegistry::instance().settingsManager()->timeZone()));
        utils::setCurrentText(_uuidEncodingComboBox, convertUUIDEncodingToString(Docutaz::AppRegistry::instance().settingsManager()->uuidEncoding()));
        _loadMongoRcJsCheckBox->setChecked(AppRegistry::instance().settingsManager()->loadMongoRcJs());
        _disabelConnectionShortcutsCheckBox->setChecked(AppRegistry::instance().settingsManager()->disableConnectionShortcuts());
        _shareShellPerConnectionCheckBox->setChecked(AppRegistry::instance().settingsManager()->shareShellPerConnection());
        utils::setCurrentText(_stylesComboBox, Docutaz::AppRegistry::instance().settingsManager()->currentStyle());
        _mongoshPathEdit->setText(AppRegistry::instance().settingsManager()->mongoshPath());
    }

    void PreferencesDialog::accept()
    {
        ViewMode mode = convertStringToViewMode(QtUtils::toStdString(_defDisplayModeComboBox->currentText()).c_str());
        Docutaz::AppRegistry::instance().settingsManager()->setViewMode(mode);

        SupportedTimes time = convertStringToTimes(QtUtils::toStdString(_timeZoneComboBox->currentText()).c_str());
        Docutaz::AppRegistry::instance().settingsManager()->setTimeZone(time);

        UUIDEncoding uuidC = convertStringToUUIDEncoding(QtUtils::toStdString(_uuidEncodingComboBox->currentText()).c_str());
        Docutaz::AppRegistry::instance().settingsManager()->setUuidEncoding(uuidC);

        AppRegistry::instance().settingsManager()->setLoadMongoRcJs(_loadMongoRcJsCheckBox->isChecked());
        AppRegistry::instance().settingsManager()->setDisableConnectionShortcuts(_disabelConnectionShortcutsCheckBox->isChecked());
        AppRegistry::instance().settingsManager()->setShareShellPerConnection(_shareShellPerConnectionCheckBox->isChecked());
        Docutaz::AppRegistry::instance().settingsManager()->setCurrentStyle(_stylesComboBox->currentText());
        AppStyleUtils::applyStyle(_stylesComboBox->currentText());
        AppRegistry::instance().settingsManager()->setMongoshPath(_mongoshPathEdit->text().trimmed());
        Docutaz::AppRegistry::instance().settingsManager()->save();

        return BaseClass::accept();
    }

    void PreferencesDialog::browseMongoshPath()
    {
        QString path = QFileDialog::getOpenFileName(this, "Select mongosh executable",
            _mongoshPathEdit->text().isEmpty() ? "/usr/bin" : _mongoshPathEdit->text());
        if (!path.isEmpty())
            _mongoshPathEdit->setText(path);
    }
}
