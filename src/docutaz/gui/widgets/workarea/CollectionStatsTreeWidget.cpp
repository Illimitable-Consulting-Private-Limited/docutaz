#include "docutaz/gui/widgets/workarea/CollectionStatsTreeWidget.h"

#include <QHeaderView>
#include <QPalette>

#include "docutaz/gui/widgets/workarea/CollectionStatsTreeItem.h"
#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{

    CollectionStatsTreeWidget::CollectionStatsTreeWidget(const std::vector<MongoDocumentPtr> &documents, QWidget *parent) 
        : QTreeWidget(parent)
    {
        QStringList colums;
        colums << "Name" << "Count" << "Size" << "Storage" << "Index" << "Average Object" << "Padding";
        setHeaderLabels(colums);

        {
            const QString line = QtUtils::isDarkPalette(this)
                ? palette().window().color().lighter(140).name()
                : QStringLiteral("#c7c5c4");
            setStyleSheet(QString("QTreeWidget { border-left: 1px solid %1; border-top: 1px solid %1; }").arg(line));
        }

        QList<QTreeWidgetItem *> items;
        size_t documentsCount = documents.size();
        for (int i = 0; i < documentsCount; i++) {
            MongoDocumentPtr document = documents[i];
            CollectionStatsTreeItem *item = new CollectionStatsTreeItem(document);
            items.append(item);
        }

        addTopLevelItems(items);

        header()->resizeSections(QHeaderView::ResizeToContents);
    }
}
