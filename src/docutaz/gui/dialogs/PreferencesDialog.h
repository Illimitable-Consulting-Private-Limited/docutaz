#pragma  once

#include <QDialog>
#include <QMap>
QT_BEGIN_NAMESPACE
class QComboBox;
class QFontComboBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;
QT_END_NAMESPACE

namespace Docutaz
{
    class PreferencesDialog : public QDialog
    {
        Q_OBJECT

    public:
        typedef QDialog BaseClass;
        explicit PreferencesDialog(QWidget *parent);
        // Fixed dialog size. Tall enough to hold the destructive-operation
        // safety group (master toggle + per-environment checkboxes) without
        // the QVBoxLayout squeezing it to a clipped, empty strip.
        enum { dialogWidth = 640, dialogHeight = 680 };
    public Q_SLOTS:
        virtual void accept();
        void browseMongoshPath();
    private Q_SLOTS:
        // Enable/disable the per-environment checkboxes with the master toggle.
        void updateGuardedEnvEnabled();
    private:
        void syncWithSettings();
    private:
        QComboBox *_appearanceComboBox;
        QComboBox *_defDisplayModeComboBox;
        QComboBox *_timeZoneComboBox;
        QComboBox *_uuidEncodingComboBox;
        QCheckBox *_loadMongoRcJsCheckBox;
        QCheckBox *_disabelConnectionShortcutsCheckBox;
        QCheckBox *_shareShellPerConnectionCheckBox;
        QFontComboBox *_uiFontComboBox;
        QFontComboBox *_editorFontComboBox;
        QSpinBox *_editorFontSizeSpinBox;
        QLineEdit *_mongoshPathEdit;
        QCheckBox *_confirmDestructiveOpsCheckBox;
        // Environment-key (production/staging/...) -> "guard this environment"
        // checkbox. Built from ConnectionEnvironment::presets() minus "None".
        QMap<QString, QCheckBox*> _guardedEnvChecks;
    };
}
