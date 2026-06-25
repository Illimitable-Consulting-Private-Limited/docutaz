#pragma  once

#include <QDialog>
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
        enum { height = 640, width = 480};
    public Q_SLOTS:
        virtual void accept();
        void browseMongoshPath();
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
    };
}
