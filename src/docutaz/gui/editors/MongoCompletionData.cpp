#include "docutaz/gui/editors/MongoCompletionData.h"

namespace Docutaz
{
    namespace MongoCompletion
    {
        namespace
        {
            // Top-level globals and BSON helpers reachable as bare identifiers in
            // a mongosh script.
            const QStringList &globals()
            {
                static const QStringList list = {
                    "db", "print", "printjson", "tojson", "sleep",
                    "ObjectId", "ISODate", "Date", "UUID", "DBRef",
                    "NumberLong", "NumberInt", "NumberDecimal",
                    "Timestamp", "BinData", "MinKey", "MaxKey",
                    // common JS keywords people type in the editor
                    "function", "return", "var", "let", "const",
                    "for", "while", "if", "else", "true", "false", "null"
                };
                return list;
            }

            // Query / aggregation / update operators. Offered only when the token
            // already starts with '$', so the popup never appears unsolicited.
            const QStringList &operators()
            {
                static const QStringList list = {
                    // query / comparison
                    "$eq", "$ne", "$gt", "$gte", "$lt", "$lte", "$in", "$nin",
                    "$and", "$or", "$not", "$nor", "$exists", "$type",
                    "$regex", "$options", "$elemMatch", "$size", "$all", "$mod",
                    "$text", "$search", "$expr", "$where", "$jsonSchema",
                    // aggregation stages
                    "$match", "$group", "$project", "$sort", "$limit", "$skip",
                    "$unwind", "$lookup", "$addFields", "$set", "$unset",
                    "$count", "$facet", "$bucket", "$bucketAuto", "$replaceRoot",
                    "$replaceWith", "$sortByCount", "$sample", "$out", "$merge",
                    "$graphLookup", "$redact", "$collStats", "$indexStats",
                    "$setWindowFields", "$densify", "$fill", "$unionWith",
                    // aggregation expression / accumulators
                    "$sum", "$avg", "$min", "$max", "$first", "$last", "$push",
                    "$addToSet", "$concat", "$concatArrays", "$cond", "$ifNull",
                    "$switch", "$map", "$filter", "$reduce", "$arrayElemAt",
                    "$size", "$dateToString", "$dateFromString", "$toString",
                    "$toInt", "$toLong", "$toDouble", "$toDecimal", "$toObjectId",
                    "$toDate", "$toBool", "$convert", "$multiply", "$divide",
                    "$add", "$subtract", "$mergeObjects", "$objectToArray",
                    "$arrayToObject", "$regexMatch", "$function",
                    // update operators
                    "$inc", "$mul", "$rename", "$setOnInsert", "$currentDate",
                    "$pull", "$pullAll", "$pop", "$each", "$position", "$slice",
                    "$sort"
                };
                return list;
            }

            // Methods on the `db` object.
            const QStringList &dbMethods()
            {
                static const QStringList list = {
                    "getCollection", "getCollectionNames", "getCollectionInfos",
                    "getSiblingDB", "getName", "getMongo", "runCommand",
                    "adminCommand", "createCollection", "createView",
                    "dropDatabase", "stats", "listCommands", "currentOp",
                    "killOp", "getUsers", "createUser", "dropUser",
                    "serverStatus", "hostInfo", "version", "watch", "aggregate"
                };
                return list;
            }

            // Methods on a collection (db.<collection>.).
            const QStringList &collectionMethods()
            {
                static const QStringList list = {
                    "find", "findOne", "findOneAndUpdate", "findOneAndReplace",
                    "findOneAndDelete", "aggregate", "countDocuments",
                    "estimatedDocumentCount", "distinct", "insertOne",
                    "insertMany", "updateOne", "updateMany", "replaceOne",
                    "deleteOne", "deleteMany", "bulkWrite", "createIndex",
                    "createIndexes", "dropIndex", "dropIndexes", "getIndexes",
                    "reIndex", "drop", "renameCollection", "stats", "dataSize",
                    "storageSize", "totalIndexSize", "watch", "mapReduce",
                    "explain", "isCapped", "validate"
                };
                return list;
            }

            QStringList filterByPrefix(const QStringList &pool, const QString &partial)
            {
                QStringList out;
                for (const QString &word : pool)
                    if (word.startsWith(partial, Qt::CaseInsensitive))
                        out += word;
                return out;
            }
        }

        QStringList staticCandidates(Context context, const QString &partial)
        {
            switch (context) {
            case Context::Global:
                if (partial.startsWith('$'))
                    return filterByPrefix(operators(), partial);
                if (!partial.isEmpty())
                    return filterByPrefix(globals(), partial);
                return {};
            case Context::DbMember:
                return filterByPrefix(dbMethods(), partial);
            case Context::CollectionMember:
                return filterByPrefix(collectionMethods(), partial);
            case Context::Unknown:
            default:
                return {};
            }
        }
    }
}
