#pragma once

#include "docutaz/core/events/MongoEventsInfo.h"
#include "docutaz/gui/widgets/explorer/ExplorerTreeItem.h"

namespace Docutaz
{
    class ExplorerCollectionIndexesDir;

    class ExplorerCollectionIndexItem : public ExplorerTreeItem
    {
        Q_OBJECT

    public:
        using BaseClass = ExplorerTreeItem ;
        explicit ExplorerCollectionIndexItem(
            ExplorerCollectionIndexesDir *parent, const IndexInfo &info);

    private Q_SLOTS:
        void ui_dropIndex();
        void ui_edit();

    private:
        IndexInfo _info;
    };
}