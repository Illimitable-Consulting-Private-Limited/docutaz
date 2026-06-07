#pragma once
#include <QTreeWidgetItem>
#include "docutaz/core/Core.h"

namespace Docutaz
{
    class CollectionStatsTreeItem : public QTreeWidgetItem
    {
    public:
        CollectionStatsTreeItem(MongoDocumentPtr document);
    };
}
