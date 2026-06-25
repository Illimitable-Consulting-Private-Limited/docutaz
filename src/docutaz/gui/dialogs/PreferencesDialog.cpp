#include "docutaz/gui/dialogs/PreferencesDialog.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QFontComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QFileDialog>
#include <QApplication>

#include "docutaz/gui/widgets/workarea/ScriptWidget.h"

#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/Theme.h"
#include "docutaz/gui/utils/ComboBoxUtils.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/EventBus.h"
#include "docutaz/core/events/MongoEvents.h"
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

        // Appearance: follow the OS colour scheme, or force light/dark. Order
        // matches Theme::Scheme (System=0, Light=1, Dark=2).
        QHBoxLayout *appearanceLayout = new QHBoxLayout(this);
        QLabel *appearanceLabel = new QLabel("Appearance:");
        appearanceLayout->addWidget(appearanceLabel);
        _appearanceComboBox = new QComboBox();
        _appearanceComboBox->addItems({ "System (follow OS)", "Light", "Dark" });
        appearanceLayout->addWidget(_appearanceComboBox);
        layout->addLayout(appearanceLayout);

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

        // Editor font — configured separately from the UI typeface. The combo
        // lists every font in the database, which includes the bundled Inter
        // (registered at startup) alongside the user's installed fonts; leaving
        // it on the monospace default is the recommended choice for code.
        QHBoxLayout *editorFontLayout = new QHBoxLayout(this);
        QLabel *editorFontLabel = new QLabel("Editor font:");
        editorFontLayout->addWidget(editorFontLabel);
        _editorFontComboBox = new QFontComboBox();
        editorFontLayout->addWidget(_editorFontComboBox, 1);
        _editorFontSizeSpinBox = new QSpinBox();
        _editorFontSizeSpinBox->setRange(0, 48);
        _editorFontSizeSpinBox->setSpecialValueText("Default");   // shown when 0
        _editorFontSizeSpinBox->setToolTip("Editor font size in points. 0 = platform default.");
        editorFontLayout->addWidget(_editorFontSizeSpinBox);
        layout->addLayout(editorFontLayout);

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
        Theme::markPrimary(buttonBox->button(QDialogButtonBox::Save));
        VERIFY(connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept())));
        VERIFY(connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject())));
        layout->addWidget(buttonBox);
        setLayout(layout);

        syncWithSettings();
    }

    void PreferencesDialog::syncWithSettings()
    {

        {
            const int pref = AppRegistry::instance().settingsManager()->colorSchemePreference();
            _appearanceComboBox->setCurrentIndex((pref >= 0 && pref <= 2) ? pref : 0);
        }
        utils::setCurrentText(_defDisplayModeComboBox, convertViewModeToString(Docutaz::AppRegistry::instance().settingsManager()->viewMode()));
        utils::setCurrentText(_timeZoneComboBox, convertTimesToString(Docutaz::AppRegistry::instance().settingsManager()->timeZone()));
        utils::setCurrentText(_uuidEncodingComboBox, convertUUIDEncodingToString(Docutaz::AppRegistry::instance().settingsManager()->uuidEncoding()));
        _loadMongoRcJsCheckBox->setChecked(AppRegistry::instance().settingsManager()->loadMongoRcJs());
        _disabelConnectionShortcutsCheckBox->setChecked(AppRegistry::instance().settingsManager()->disableConnectionShortcuts());
        _shareShellPerConnectionCheckBox->setChecked(AppRegistry::instance().settingsManager()->shareShellPerConnection());

        // Editor font: show the resolved family (falls back to the monospace
        // default when the user hasn't picked one) and the configured size (0 =
        // platform default, shown as "Default").
        _editorFontComboBox->setCurrentFont(GuiRegistry::instance().editorFont());
        const int editorPt = AppRegistry::instance().settingsManager()->editorFontPointSize();
        _editorFontSizeSpinBox->setValue(editorPt > 0 ? editorPt : 0);

        _mongoshPathEdit->setText(AppRegistry::instance().settingsManager()->mongoshPath());
    }

    void PreferencesDialog::accept()
    {
        // Appearance preference — persist and apply live (re-pick palette/QSS for
        // the new scheme, then notify the imperatively-themed widgets).
        const int scheme = _appearanceComboBox->currentIndex();
        AppRegistry::instance().settingsManager()->setColorSchemePreference(scheme);
        Theme::setSchemePreference(static_cast<Theme::Scheme>(scheme));
        Theme::apply();
        Theme::Notifier::instance()->notify();

        ViewMode mode = convertStringToViewMode(QtUtils::toStdString(_defDisplayModeComboBox->currentText()).c_str());
        Docutaz::AppRegistry::instance().settingsManager()->setViewMode(mode);

        SupportedTimes time = convertStringToTimes(QtUtils::toStdString(_timeZoneComboBox->currentText()).c_str());
        Docutaz::AppRegistry::instance().settingsManager()->setTimeZone(time);

        UUIDEncoding uuidC = convertStringToUUIDEncoding(QtUtils::toStdString(_uuidEncodingComboBox->currentText()).c_str());
        Docutaz::AppRegistry::instance().settingsManager()->setUuidEncoding(uuidC);

        AppRegistry::instance().settingsManager()->setLoadMongoRcJs(_loadMongoRcJsCheckBox->isChecked());
        AppRegistry::instance().settingsManager()->setDisableConnectionShortcuts(_disabelConnectionShortcutsCheckBox->isChecked());
        AppRegistry::instance().settingsManager()->setShareShellPerConnection(_shareShellPerConnectionCheckBox->isChecked());

        // Editor font (separate from the UI typeface). Size 0 → platform default.
        AppRegistry::instance().settingsManager()->setEditorFontFamily(
            _editorFontComboBox->currentFont().family());
        AppRegistry::instance().settingsManager()->setEditorFontPointSize(
            _editorFontSizeSpinBox->value());
        // Push the new editor font onto any open query tabs so it takes effect
        // immediately (other editors are modal dialogs, rebuilt on next open).
        for (QWidget *w : QApplication::allWidgets())
            if (auto *sw = qobject_cast<ScriptWidget*>(w))
                sw->reapplyEditorFont();

        AppRegistry::instance().settingsManager()->setMongoshPath(_mongoshPathEdit->text().trimmed());
        Docutaz::AppRegistry::instance().settingsManager()->save();

        // Notify the "mongosh not detected" indicators (status bar + Welcome
        // card) so they re-evaluate detection regardless of which screen this
        // dialog was opened from.
        AppRegistry::instance().bus()->publish(new MongoshSettingsChangedEvent(this));

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
