#pragma once
#include <QTreeWidgetItem>
#include "robomongo/core/Core.h"

namespace Docutaz
{
    class CollectionStatsTreeItem : public QTreeWidgetItem
    {
    public:
        CollectionStatsTreeItem(MongoDocumentPtr document);
    };
}
