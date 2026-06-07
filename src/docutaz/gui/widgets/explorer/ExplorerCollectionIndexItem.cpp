#include "ExplorerCollectionIndexItem.h"

#include <QAction>
#include <QMenu>

#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/utils/QtUtils.h"

#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/utils/DialogUtils.h"
#include "docutaz/gui/widgets/explorer/AddEditIndexDialog.h"
#include "docutaz/gui/widgets/explorer/ExplorerCollectionIndexesDir.h"
#include "docutaz/gui/widgets/explorer/ExplorerCollectionTreeItem.h"
#include "docutaz/gui/widgets/explorer/ExplorerDatabaseTreeItem.h"

namespace Docutaz
{
    ExplorerCollectionIndexItem::ExplorerCollectionIndexItem(
        ExplorerCollectionIndexesDir *parent, const IndexInfo &info)
        : BaseClass(parent), _info(info)
    {
        auto dropIndex = new QAction("Drop Index...", this);
        connect(dropIndex, SIGNAL(triggered()), SLOT(ui_dropIndex()));
        auto editIndex = new QAction("Edit Index...", this);
        connect(editIndex, SIGNAL(triggered()), SLOT(ui_edit()));

        BaseClass::_contextMenu->addAction(editIndex);
        BaseClass::_contextMenu->addAction(dropIndex);

        setText(0, QtUtils::toQString(_info._name));
        setIcon(0, Docutaz::GuiRegistry::instance().indexIcon());
    }

    void ExplorerCollectionIndexItem::ui_dropIndex()
    {
        // Ask user
        auto const answer = utils::questionDialog(treeWidget(), "Drop", "Index", text(0));
        if (answer != QMessageBox::Yes)
            return;

        auto const par = dynamic_cast<ExplorerCollectionIndexesDir *>(parent());
        if (!par)
            return;
        
        auto const grandParent = dynamic_cast<ExplorerCollectionTreeItem *>(par->parent());
        if (!grandParent)
            return;

        grandParent->dropIndex(this);
    }

    void ExplorerCollectionIndexItem::ui_edit()
    {
        auto const par = dynamic_cast<ExplorerCollectionIndexesDir *>(parent());
        if (par) {
            auto const grPar = dynamic_cast<ExplorerCollectionTreeItem *const>(par->parent());
            if (!grPar)
                return;

            auto const& db { grPar->databaseItem()->database() };
            AddEditIndexDialog dlg {
                _info, 
                QtUtils::toQString(db->name()), 
                QtUtils::toQString(db->server()->connectionRecord()->getFullAddress()),
                false,
                treeWidget()
            };

            auto const result = dlg.exec();
            if (result != QDialog::Accepted)
                return;

            auto const databaseTreeItem = dynamic_cast<ExplorerDatabaseTreeItem *>(grPar->databaseItem());
            if (!databaseTreeItem)
                return;

            databaseTreeItem->addEditIndex(grPar, _info, dlg.info());
        }
    }
}