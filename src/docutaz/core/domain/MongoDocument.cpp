#include "docutaz/core/domain/MongoDocument.h"

#include <mongo/client/dbclient_base.h>
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/utils/BsonUtils.h"

namespace Docutaz
{
    MongoDocument::MongoDocument()
    {

    }

    MongoDocument::~MongoDocument()
    {
    }

    /*
    ** Create MongoDocument from BsonObj. It will take owned version of BSONObj
    */
    MongoDocument::MongoDocument(mongo::BSONObj bsonObj) :_bsonObj(bsonObj)
    {
    }

    /*
    ** Create MongoDocument from BsonObj. It will take owned version of BSONObj
    */ 
    MongoDocumentPtr MongoDocument::fromBsonObj(const mongo::BSONObj &bsonObj)
    {
        MongoDocument *doc = new MongoDocument(bsonObj);
        return MongoDocumentPtr(doc);
    }

    /*
    ** Create list of MongoDocuments from QList<BsonObj>. It will take owned version of BSONObj
    */ 
    std::vector<MongoDocumentPtr> MongoDocument::fromBsonObj(const std::vector<mongo::BSONObj> &bsonObjs)
    {
        std::vector<MongoDocumentPtr> list;
        for (std::vector<mongo::BSONObj>::const_iterator it = bsonObjs.begin(); it != bsonObjs.end(); ++it) {
            list.push_back(fromBsonObj(*it));
        }

        return list;
    }
}
