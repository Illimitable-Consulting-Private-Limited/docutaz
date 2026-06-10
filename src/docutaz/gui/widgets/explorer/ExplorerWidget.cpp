#include "docutaz/gui/widgets/explorer/ExplorerWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QKeyEvent>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/domain/App.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/gui/MainWindow.h"
#include "docutaz/gui/widgets/explorer/ExplorerTreeWidget.h"
#include "docutaz/gui/widgets/explorer/ExplorerServerTreeItem.h"
#include "docutaz/gui/widgets/explorer/ExplorerCollectionTreeItem.h"
#include "docutaz/gui/widgets/explorer/ExplorerCollectionIndexesDir.h"
#include "docutaz/gui/widgets/explorer/ExplorerDatabaseCategoryTreeItem.h"
#include "docutaz/gui/widgets/explorer/ExplorerReplicaSetTreeItem.h"
#include "docutaz/gui/widgets/explorer/ExplorerReplicaSetFolderItem.h"
#include "docutaz/gui/widgets/explorer/ExplorerUserTreeItem.h"
#include "docutaz/utils/common.h"

namespace Docutaz
{

    ExplorerWidget::ExplorerWidget(MainWindow *parentMainWindow) : BaseClass(parentMainWindow),
        _progress(0)
    {
        _treeWidget = new ExplorerTreeWidget(this);

        QHBoxLayout *vlaout = new QHBoxLayout();
        vlaout->setContentsMargins(0, 0, 0, 0);
        vlaout->addWidget(_treeWidget, Qt::AlignJustify);

        VERIFY(connect(_treeWidget, SIGNAL(itemExpanded(QTreeWidgetItem *)), this, SLOT(ui_itemExpanded(QTreeWidgetItem *))));
        VERIFY(connect(_treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), 
                       this, SLOT(ui_itemDoubleClicked(QTreeWidgetItem *, int))));

        setLayout(vlaout);

        QMovie *movie = new QMovie(":/docutaz/icons/loading.gif", QByteArray(), this);
        _progressLabel = new QLabel(this);
        _progressLabel->setMovie(movie);
        _progressLabel->hide();
        movie->start();        
    }

    ExplorerWidget::~ExplorerWidget()
    {
        saveSetting("ExplorerWidget/size", size());
    }

    QTreeWidgetItem* ExplorerWidget::getSelectedTreeItem() const
    {
        return _treeWidget->currentItem();
    }

    void ExplorerWidget::keyPressEvent(QKeyEvent *event)
    {
        if ((event->key() == Qt::Key_Return) || (event->key() == Qt::Key_Enter))
        {
            QList<QTreeWidgetItem*> items = _treeWidget->selectedItems();

            if (items.count() != 1) {
                BaseClass::keyPressEvent(event);
                return;
            }

            QTreeWidgetItem *item = items[0];

            if (!item) {
                BaseClass::keyPressEvent(event);
                return;
            }

            ui_itemDoubleClicked(item, 0);

            return;
        }

        BaseClass::keyPressEvent(event);
    }

    QSize ExplorerWidget::sizeHint() const
    {
        auto size { getSetting("ExplorerWidget/size").toSize() };        
        if(QSize(-1, -1) == size)
           size = QSize(180, -1);

        return(size);
    }

    void ExplorerWidget::increaseProgress()
    {
        ++_progress;
        _progressLabel->move(width() / 2 - 8, height() / 2 - 8);
        _progressLabel->show();
    }

    void ExplorerWidget::decreaseProgress()
    {
        --_progress;

        if (_progress < 0)
            _progress = 0;

        if (!_progress)
            _progressLabel->hide();
    }

    void ExplorerWidget::handle(ConnectingEvent *event)
    {
        increaseProgress();
    }

    void ExplorerWidget::handle(ConnectionEstablishedEvent *event)
    {
        // Do not make UI changes for non PRIMARY connections
        if (event->connectionType != ConnectionPrimary)
            return;

        decreaseProgress();

        auto item = new ExplorerServerTreeItem(_treeWidget, event->server, event->connInfo);
        _treeWidget->addTopLevelItem(item);
        _treeWidget->setCurrentItem(item);
        _treeWidget->setFocus();
    }

    void ExplorerWidget::handle(ConnectionFailedEvent *event)
    {
        decreaseProgress();
    }

    void ExplorerWidget::ui_itemExpanded(QTreeWidgetItem *item)
    {
        auto categoryItem = dynamic_cast<ExplorerDatabaseCategoryTreeItem *>(item);
        if (categoryItem) {
            categoryItem->expand();
            return;
        }

        auto serverItem = dynamic_cast<ExplorerServerTreeItem *>(item);
        if (serverItem) {
            serverItem->expand();
            return;
        }

        auto replicaSetFolder = dynamic_cast<ExplorerReplicaSetFolderItem *>(item);
        if (replicaSetFolder) {
            replicaSetFolder->expand();
            return;
        }
       
        auto dirItem = dynamic_cast<ExplorerCollectionIndexesDir *>(item);
        if (dirItem) {
            dirItem->expand();
        }
    }

    void ExplorerWidget::ui_itemDoubleClicked(QTreeWidgetItem *item, int column)
    {        
        if (auto collectionItem = dynamic_cast<ExplorerCollectionTreeItem *>(item)) {
            AppRegistry::instance().app()->openShell(collectionItem->collection());
            return;
        }

        if (auto userItem = dynamic_cast<ExplorerUserTreeItem *>(item)) {
            userItem->ui_viewUser();
            return;
        }

        auto replicaMemberItem = dynamic_cast<ExplorerReplicaSetTreeItem*>(item);
        if (replicaMemberItem && replicaMemberItem->isUp()) {
            AppRegistry::instance().app()->openShell(replicaMemberItem->server(), 
                replicaMemberItem->connectionSettings(), ScriptInfo("", true));
            return;
        }

        // Toggle expanded state
        item->setExpanded(!item->isExpanded());
    }
}
