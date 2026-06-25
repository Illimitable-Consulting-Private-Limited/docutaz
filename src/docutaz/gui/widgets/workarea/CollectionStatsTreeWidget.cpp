#include "docutaz/gui/widgets/workarea/CollectionStatsTreeWidget.h"

#include <QHeaderView>
#include <QPalette>

#include "docutaz/gui/widgets/workarea/CollectionStatsTreeItem.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/gui/Theme.h"

namespace Docutaz
{

    CollectionStatsTreeWidget::CollectionStatsTreeWidget(const std::vector<MongoDocumentPtr> &documents, QWidget *parent) 
        : QTreeWidget(parent)
    {
        QStringList colums;
        colums << "Name" << "Count" << "Size" << "Storage" << "Index" << "Average Object" << "Padding";
        setHeaderLabels(colums);

        auto applyThemeBorder = [this] {
            const QString line = Theme::current().mid.name();
            setStyleSheet(QString("QTreeWidget { border-left: 1px solid %1; border-top: 1px solid %1; }").arg(line));
        };
        applyThemeBorder();
        connect(Theme::Notifier::instance(), &Theme::Notifier::changed, this, applyThemeBorder);

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
