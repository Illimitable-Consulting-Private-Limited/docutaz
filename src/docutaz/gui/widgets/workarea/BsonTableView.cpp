#include "docutaz/gui/widgets/workarea/BsonTableView.h"

#include <QHeaderView>
#include <QAction>
#include <QMenu>
#include <QKeyEvent>
#include <QPalette>

#include "docutaz/gui/widgets/workarea/BsonTreeItem.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{
    BsonTableView::BsonTableView(MongoShell *shell, const MongoQueryInfo &queryInfo, QWidget *parent) 
        :BaseClass(parent), _notifier(this, shell, queryInfo)
    {
#if defined(Q_OS_MAC)
        setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
        GuiRegistry::instance().setAlternatingColor(this);

        verticalHeader()->setDefaultAlignment(Qt::AlignLeft);
        horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
        // Light borders/gridlines glare against a dark palette; derive both from
        // the window colour when dark.
        if (QtUtils::isDarkPalette(this)) {
            const QColor bg = palette().window().color();
            setStyleSheet(QString("QTableView { border-left: 1px solid %1; border-top: 1px solid %1;"
                                  " gridline-color: %2;}")
                          .arg(bg.lighter(140).name(), bg.lighter(125).name()));
        } else {
            setStyleSheet("QTableView { border-left: 1px solid #c7c5c4; border-top: 1px solid #c7c5c4; gridline-color: #edebea;}");
        }

        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setSelectionBehavior(QAbstractItemView::SelectItems);
        setContextMenuPolicy(Qt::CustomContextMenu);
        VERIFY(connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&))));
    }

    void BsonTableView::keyPressEvent(QKeyEvent *event)
    {
        if (event->key() == Qt::Key_Delete) {
            _notifier.onDeleteDocuments();
        }
        return BaseClass::keyPressEvent(event);
    }

    QModelIndex BsonTableView::selectedIndex() const
    {
        QModelIndexList indexes = detail::uniqueRows(selectionModel()->selectedIndexes());

        if (indexes.count() != 1)
            return QModelIndex();

        return indexes[0];
    }

    QModelIndexList BsonTableView::selectedIndexes() const
    {
        return detail::uniqueRows(selectionModel()->selectedIndexes());
    }

    void BsonTableView::showContextMenu( const QPoint &point )
    {
        QPoint menuPoint = mapToGlobal(point);
        menuPoint.setY(menuPoint.y() + horizontalHeader()->height());
        menuPoint.setX(menuPoint.x() + verticalHeader()->width());

        QModelIndexList indexes = selectedIndexes();
        if (detail::isMultiSelection(indexes)) {
            QMenu menu(this);
            _notifier.initMultiSelectionMenu(&menu);
            menu.exec(menuPoint);
        }
        else{
            QModelIndex selectedInd = selectedIndex();
            BsonTreeItem *documentItem = QtUtils::item<BsonTreeItem*>(selectedInd);
            QMenu menu(this);
            _notifier.initMenu(&menu, documentItem);
            menu.exec(menuPoint);
        }
    }

}
