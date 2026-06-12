#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPoint;

namespace Docutaz
{
    // Dockable panel listing executed queries (from QueryHistoryManager).
    // Search, re-open in a new tab, copy, pin and delete.
    class QueryHistoryWidget : public QWidget
    {
        Q_OBJECT
    public:
        explicit QueryHistoryWidget(QWidget *parent = nullptr);

    Q_SIGNALS:
        // Request to run this query in a new tab (handled by MainWindow on the
        // current connection).
        void openQuery(const QString &query);

    private Q_SLOTS:
        void rebuild();
        void onActivated(QListWidgetItem *item);
        void showContextMenu(const QPoint &pos);
        void clearAll();

    private:
        int entryIndex(QListWidgetItem *item) const;   // index into manager entries()

        QLineEdit   *_search;
        QListWidget *_list;
    };
}
