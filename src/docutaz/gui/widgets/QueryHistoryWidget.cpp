#include "docutaz/gui/widgets/QueryHistoryWidget.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

#include "docutaz/core/QueryHistory.h"

namespace Docutaz
{
    QueryHistoryWidget::QueryHistoryWidget(QWidget *parent)
        : QWidget(parent)
    {
        _search = new QLineEdit;
        _search->setPlaceholderText(tr("Search history (query, connection, database)…"));
        _search->setClearButtonEnabled(true);

        auto *clearBtn = new QPushButton(tr("Clear"));
        clearBtn->setToolTip(tr("Remove all history"));

        auto *top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        top->addWidget(_search, 1);
        top->addWidget(clearBtn);

        _list = new QListWidget;
        _list->setAlternatingRowColors(true);
        _list->setContextMenuPolicy(Qt::CustomContextMenu);
        _list->setUniformItemSizes(false);
        _list->setWordWrap(false);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->addLayout(top);
        lay->addWidget(_list, 1);

        connect(_search, &QLineEdit::textChanged, this, &QueryHistoryWidget::rebuild);
        connect(_list, &QListWidget::itemActivated, this, &QueryHistoryWidget::onActivated);
        connect(_list, &QListWidget::customContextMenuRequested, this, &QueryHistoryWidget::showContextMenu);
        connect(clearBtn, &QPushButton::clicked, this, &QueryHistoryWidget::clearAll);
        connect(&QueryHistoryManager::instance(), &QueryHistoryManager::changed,
                this, &QueryHistoryWidget::rebuild);

        rebuild();
    }

    int QueryHistoryWidget::entryIndex(QListWidgetItem *item) const
    {
        return item ? item->data(Qt::UserRole).toInt() : -1;
    }

    void QueryHistoryWidget::rebuild()
    {
        _list->clear();
        const QString needle = _search->text().trimmed();
        const QList<QueryHistoryEntry> &entries = QueryHistoryManager::instance().entries();

        for (int i = 0; i < entries.size(); ++i) {
            const QueryHistoryEntry &e = entries.at(i);
            if (!needle.isEmpty()
                && !e.query.contains(needle, Qt::CaseInsensitive)
                && !e.connection.contains(needle, Qt::CaseInsensitive)
                && !e.database.contains(needle, Qt::CaseInsensitive))
                continue;

            QString meta = e.timestamp.toString("MMM d HH:mm");
            if (!e.connection.isEmpty()) meta += " - " + e.connection;
            if (!e.database.isEmpty())   meta += "/" + e.database;
            if (!e.kind.isEmpty())       meta += " - " + e.kind;
            if (!e.success)
                meta += " - error";
            else if (e.resultCount >= 0)
                meta += QString(" - %1 result%2").arg(e.resultCount).arg(e.resultCount == 1 ? "" : "s");
            if (e.runCount > 1)
                meta += QString(" - x%1").arg(e.runCount);

            const QString prefix = e.pinned ? QStringLiteral("* ") : QString();
            auto *item = new QListWidgetItem(prefix + e.query.simplified() + "\n" + meta);
            item->setToolTip(e.query);
            item->setData(Qt::UserRole, i);
            _list->addItem(item);
        }
    }

    void QueryHistoryWidget::onActivated(QListWidgetItem *item)
    {
        const int i = entryIndex(item);
        const auto &entries = QueryHistoryManager::instance().entries();
        if (i >= 0 && i < entries.size())
            emit openQuery(entries.at(i).query);
    }

    void QueryHistoryWidget::showContextMenu(const QPoint &pos)
    {
        QListWidgetItem *item = _list->itemAt(pos);
        if (!item)
            return;
        const int i = entryIndex(item);
        const auto &entries = QueryHistoryManager::instance().entries();
        if (i < 0 || i >= entries.size())
            return;
        const QueryHistoryEntry &e = entries.at(i);
        const QString query = e.query;

        QMenu menu(this);
        QAction *openAct = menu.addAction(tr("Open in New Tab"));
        QAction *copyAct = menu.addAction(tr("Copy Query"));
        menu.addSeparator();
        QAction *pinAct = menu.addAction(e.pinned ? tr("Unpin") : tr("Pin"));
        QAction *delAct = menu.addAction(tr("Delete"));

        QAction *chosen = menu.exec(_list->viewport()->mapToGlobal(pos));
        if (chosen == openAct)
            emit openQuery(query);
        else if (chosen == copyAct)
            QApplication::clipboard()->setText(query);
        else if (chosen == pinAct)
            QueryHistoryManager::instance().setPinned(i, !e.pinned);
        else if (chosen == delAct)
            QueryHistoryManager::instance().remove(i);
    }

    void QueryHistoryWidget::clearAll()
    {
        if (QueryHistoryManager::instance().entries().isEmpty())
            return;
        if (QMessageBox::question(this, tr("Clear Query History"),
                                  tr("Remove all saved query history?"))
            == QMessageBox::Yes)
            QueryHistoryManager::instance().clear();
    }
}
