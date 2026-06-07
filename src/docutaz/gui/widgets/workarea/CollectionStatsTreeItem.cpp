#include "docutaz/gui/widgets/workarea/CollectionStatsTreeItem.h"

#include <mongo/db/jsobj.h>
#include <mongo/bson/bsonobj.h>

#include "docutaz/core/domain/MongoUtils.h"
#include "docutaz/core/domain/MongoDocument.h"
#include "docutaz/core/domain/MongoNamespace.h"
#include "docutaz/core/utils/BsonUtils.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/core/utils/QtUtils.h"

namespace 
{
    QString prepareValue(const QString &data)
    {
        return data + "     "; // ugly yet simple way to extend size of columns
    }
}

namespace Docutaz
{

    CollectionStatsTreeItem::CollectionStatsTreeItem(MongoDocumentPtr document)
    {
        mongo::BSONObj _obj = document->bsonObj();

        MongoNamespace ns(BsonUtils::getField<mongo::String>(_obj, "ns"));

        setText(0, prepareValue(QtUtils::toQString(ns.collectionName())));
        setIcon(0, GuiRegistry::instance().collectionIcon());
        setText(1, prepareValue(QString::number(BsonUtils::getField<mongo::NumberLong>(_obj, "count"))));
        setText(2, prepareValue(MongoUtils::buildNiceSizeString(BsonUtils::getField<mongo::NumberDouble>(_obj, "size"))));
        setText(3, prepareValue(MongoUtils::buildNiceSizeString(BsonUtils::getField<mongo::NumberDouble>(_obj, "storageSize"))));
        setText(4, prepareValue(MongoUtils::buildNiceSizeString(BsonUtils::getField<mongo::NumberDouble>(_obj, "totalIndexSize"))));
        setText(5, prepareValue(MongoUtils::buildNiceSizeString(BsonUtils::getField<mongo::NumberDouble>(_obj, "avgObjSize"))));
        setText(6, prepareValue(QString::number(BsonUtils::getField<mongo::NumberDouble>(_obj, "paddingFactor"))));
    }
}
