#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QComboBox;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QPushButton;
class QLabel;
class QPoint;
QT_END_NAMESPACE

namespace Docutaz
{
    class DocutazScintilla;

    // Full work-area tab presenting the executed-query history (from
    // QueryHistoryManager) as a master/detail view: a filterable list on the
    // left and a read-only, syntax-highlighted preview of the selected query on
    // the right. Filters can be driven either by field dropdowns or a tokenized
    // search box (toggled by the mode selector). Opens a selected query in a new
    // shell tab on its own connection + database.
    class QueryHistoryTab : public QWidget
    {
        Q_OBJECT
    public:
        explicit QueryHistoryTab(QWidget *parent = nullptr);

    private Q_SLOTS:
        void rebuild();              // re-filter + repopulate the list
        void refreshFilterValues();  // repopulate dropdown options from history
        void onSelectionChanged();   // update the preview pane + action buttons
        void onActivated(QListWidgetItem *item);
        void showListMenu(const QPoint &pos);
        void toggleFilterMode(int index);
        void clearAll();
        void openSelected();
        void copySelected();
        void pinSelected();
        void deleteSelected();

    private:
        int  selectedEntryIndex() const;             // index into manager entries()
        bool entryMatches(int entryIndex) const;     // against the active filter
        void setActionsEnabled(bool on);
        void notify(const QString &text);            // transient status line

        // Filter mode selector + stacked filter bars
        QComboBox      *_modeCombo   = nullptr;
        QStackedWidget *_filterStack = nullptr;

        // Fields mode
        QLineEdit *_search    = nullptr;
        QComboBox *_connCombo = nullptr;
        QComboBox *_dbCombo   = nullptr;
        QComboBox *_collCombo = nullptr;
        QComboBox *_typeCombo = nullptr;
        QCheckBox *_pinnedOnly = nullptr;

        // Search (tokenized) mode
        QLineEdit *_tokenSearch = nullptr;

        // Shared
        QComboBox        *_sortCombo = nullptr;
        QListWidget      *_list      = nullptr;
        DocutazScintilla *_preview   = nullptr;
        QPushButton      *_openBtn   = nullptr;
        QPushButton      *_copyBtn   = nullptr;
        QPushButton      *_pinBtn    = nullptr;
        QPushButton      *_delBtn    = nullptr;
        QLabel           *_status    = nullptr;
    };
}
