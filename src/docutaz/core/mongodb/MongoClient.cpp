#include "docutaz/core/mongodb/MongoClient.h"

#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/index_view.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/index.hpp>
#include <memory>
#include <mongocxx/options/replace.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/bson_value/view.hpp>

#include "docutaz/core/domain/MongoDocument.h"
#include "docutaz/core/utils/BsonUtils.h"
#include "docutaz/shell/bson/json.h"

namespace {
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::make_array;

static bsoncxx::document::view toView(const mongo::BSONObj& obj) {
    return bsoncxx::document::view(
        reinterpret_cast<const uint8_t*>(obj.objdata()),
        static_cast<std::size_t>(obj.objsize()));
}

static mongo::BSONObj fromView(bsoncxx::document::view v) {
    return mongo::BSONObj(reinterpret_cast<const char*>(v.data()));
}

static std::string getErrStr(bsoncxx::document::view result) {
    if (auto e = result["errmsg"]) return std::string(e.get_string().value);
    if (auto e = result["err"])    return std::string(e.get_string().value);
    return "Failed to get error message.";
}

static bool cmdOk(bsoncxx::document::view result) {
    if (auto e = result["ok"]) {
        switch (e.type()) {
        case bsoncxx::type::k_double: return e.get_double().value  != 0.0;
        case bsoncxx::type::k_int32:  return e.get_int32().value   != 0;
        case bsoncxx::type::k_int64:  return e.get_int64().value   != 0;
        case bsoncxx::type::k_bool:   return e.get_bool().value;
        default: break;
        }
    }
    return false;
}

Docutaz::IndexInfo makeIndexInfoFromBsonObj(
    const Docutaz::MongoCollectionInfo &collection,
    const mongo::BSONObj &obj)
{
    using namespace Docutaz::BsonUtils;
    Docutaz::IndexInfo info(collection);
    info._name = obj.getStringField("name");
    mongo::BSONObj keyObj = obj.getObjectField("key");
    if (keyObj.isValid())
        info._keys = jsonString(keyObj, mongo::TenGen, 1, Docutaz::DefaultEncoding, Docutaz::Utc);
    info._unique   = obj.getBoolField("unique");
    info._backGround = obj.getBoolField("background");
    info._sparse   = obj.getBoolField("sparse");
    info._ttl      = obj.getIntField("expireAfterSeconds");
    info._defaultLanguage  = obj.getStringField("default_language");
    info._languageOverride = obj.getStringField("language_override");
    mongo::BSONObj weightsObj = obj.getObjectField("weights");
    if (weightsObj.isValid())
        info._textWeights = jsonString(weightsObj, mongo::TenGen, 1, Docutaz::DefaultEncoding,
                                       Docutaz::Utc);
    return info;
}
} // namespace

namespace Docutaz {

MongoClient::MongoClient(mongocxx::client& client) : _client(client) {}

std::vector<std::string> MongoClient::getCollectionNamesWithDbname(const std::string &dbname) const
{
    std::vector<std::string> collNames;
    auto cursor = _client[dbname].list_collections();
    for (auto& doc : cursor) {
        auto e = doc["name"];
        if (e && e.type() == bsoncxx::type::k_string)
            collNames.push_back(dbname + '.' + std::string(e.get_string().value));
    }
    std::sort(collNames.begin(), collNames.end());
    return collNames;
}

float MongoClient::getVersion() const
{
    try {
        auto res = _client["admin"].run_command(make_document(kvp("buildInfo", 1)).view());
        auto v = res.view()["version"];
        if (v && v.type() == bsoncxx::type::k_string)
            return static_cast<float>(std::atof(std::string(v.get_string().value).c_str()));
    } catch (...) {}
    return 0.0f;
}

std::string MongoClient::dbVersionStr() const
{
    try {
        auto res = _client["admin"].run_command(make_document(kvp("buildInfo", 1)).view());
        auto v = res.view()["version"];
        if (v && v.type() == bsoncxx::type::k_string)
            return std::string(v.get_string().value);
    } catch (...) {}
    return "";
}

std::string MongoClient::getStorageEngineType() const
{
    try {
        auto res = _client["admin"].run_command(make_document(kvp("serverStatus", 1)).view());
        auto se = res.view()["storageEngine"]["name"];
        if (se && se.type() == bsoncxx::type::k_string)
            return std::string(se.get_string().value);
    } catch (...) {}
    return "";
}

std::vector<std::string> MongoClient::getDatabaseNames() const
{
    auto names = _client.list_database_names();
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<MongoUser> MongoClient::getUsers(const std::string &dbName)
{
    auto res = _client[dbName].run_command(make_document(kvp("usersInfo", 1)).view());
    if (!cmdOk(res.view()))
        throw std::runtime_error(getErrStr(res.view()));

    std::vector<MongoUser> users;
    auto usersElem = res.view()["users"];
    if (usersElem && usersElem.type() == bsoncxx::type::k_array) {
        float ver = getVersion();
        for (auto& u : usersElem.get_array().value)
            if (u.type() == bsoncxx::type::k_document)
                users.push_back(MongoUser(ver, fromView(u.get_document().view())));
    }
    return users;
}

void MongoClient::createUser(const std::string &dbName, const MongoUser &user)
{
    bsoncxx::builder::basic::document cmd;
    cmd.append(kvp("createUser", user.name()));
    cmd.append(kvp("pwd", user.password()));

    bsoncxx::builder::basic::array roles;
    for (auto& r : user.roles())
        roles.append(make_document(kvp("role", r), kvp("db", user.userSource())));
    cmd.append(kvp("roles", roles));

    auto res = _client[dbName].run_command(cmd.view());
    if (!cmdOk(res.view()))
        throw std::runtime_error(getErrStr(res.view()));
}

void MongoClient::dropUser(const std::string &dbName, const std::string &user)
{
    auto res = _client[dbName].run_command(make_document(kvp("dropUser", user)).view());
    if (!cmdOk(res.view()))
        throw std::runtime_error(getErrStr(res.view()));
}

std::vector<MongoFunction> MongoClient::getFunctions(const std::string &dbName) const
{
    std::vector<MongoFunction> functions;
    mongocxx::options::find opts;
    opts.sort(make_document(kvp("_id", 1)));
    auto cursor = _client[dbName]["system.js"].find({}, opts);
    for (auto& doc : cursor) {
        try {
            functions.push_back(MongoFunction(fromView(doc)));
        } catch (...) {}
    }
    return functions;
}

std::vector<IndexInfo> MongoClient::getIndexes(const MongoCollectionInfo &collection) const
{
    std::vector<IndexInfo> result;
    auto cursor = _client[collection.ns().databaseName()][collection.ns().collectionName()].list_indexes();
    for (auto& doc : cursor)
        result.push_back(makeIndexInfoFromBsonObj(collection, fromView(doc)));
    return result;
}

void MongoClient::dropIndexFromCollection(const MongoCollectionInfo &collection,
                                           const std::string &indexName) const
{
    _client[collection.ns().databaseName()][collection.ns().collectionName()].indexes().drop_one(indexName);
}

void MongoClient::addEditIndex(const IndexInfo &oldInfo, const IndexInfo &newInfo) const
{
    bool editIndex = !oldInfo._name.empty();
    const std::string db   = newInfo._collection.ns().databaseName();
    const std::string coll = newInfo._collection.ns().collectionName();

    if (editIndex)
        _client[db][coll].indexes().drop_one(oldInfo._name);

    try {
        bsoncxx::document::value keysDoc =
            bsoncxx::from_json(newInfo._keys.empty() ? "{}" : newInfo._keys);

        mongocxx::options::index opts;
        opts.name(newInfo._name);
        if (newInfo._unique)    opts.unique(true);
        if (newInfo._backGround) opts.background(true);
        if (newInfo._sparse)    opts.sparse(true);
        if (!newInfo._defaultLanguage.empty())
            opts.default_language(newInfo._defaultLanguage);
        if (!newInfo._languageOverride.empty())
            opts.language_override(newInfo._languageOverride);
        if (newInfo._ttl > 0)
            opts.expire_after(std::chrono::seconds(newInfo._ttl));
        if (!newInfo._textWeights.empty()) {
            try { opts.weights(bsoncxx::from_json(newInfo._textWeights)); } catch (...) {}
        }

        _client[db][coll].create_index(keysDoc.view(), opts);
    } catch (...) {
        if (editIndex) {
            try {
                auto keysDoc = bsoncxx::from_json(oldInfo._keys.empty() ? "{}" : oldInfo._keys);
                mongocxx::options::index opts;
                opts.name(oldInfo._name);
                _client[db][coll].create_index(keysDoc.view(), opts);
            } catch (...) {}
        }
        throw;
    }
}

void MongoClient::renameIndexFromCollection(const MongoCollectionInfo &collection,
                                             const std::string &oldIndexName,
                                             const std::string &/*newIndexName*/) const
{
    _client[collection.ns().databaseName()][collection.ns().collectionName()].indexes().drop_one(oldIndexName);
}

void MongoClient::createFunction(const std::string &dbName, const MongoFunction &fun,
                                  const std::string &existingFunctionName)
{
    auto obj = fun.toBson();
    auto docView = toView(obj);
    std::string name = fun.name();

    if (existingFunctionName.empty()) {
        _client[dbName]["system.js"].insert_one(docView);
    } else {
        if (existingFunctionName == name) {
            mongocxx::options::replace opts;
            opts.upsert(true);
            _client[dbName]["system.js"].replace_one(
                make_document(kvp("_id", name)).view(), docView, opts);
        } else {
            _client[dbName]["system.js"].insert_one(docView);
            _client[dbName]["system.js"].delete_one(
                make_document(kvp("_id", existingFunctionName)).view());
        }
    }
}

void MongoClient::dropFunction(const std::string &dbName, const std::string &name)
{
    _client[dbName]["system.js"].delete_one(make_document(kvp("_id", name)).view());
}

void MongoClient::createDatabase(const std::string &dbName)
{
    if (_client[dbName].has_collection("temp"))
        throw std::runtime_error(dbName + ".temp already exists.");
    _client[dbName]["temp"].insert_one(make_document(kvp("_id", "temp")).view());
    _client[dbName]["temp"].drop();
}

void MongoClient::dropDatabase(const std::string &dbName)
{
    _client[dbName].drop();
}

void MongoClient::createCollection(const std::string &ns, long long size, bool capped, int max,
                                    const mongo::BSONObj &/*extraOptions*/)
{
    size_t dot = ns.find('.');
    std::string db   = (dot != std::string::npos) ? ns.substr(0, dot) : ns;
    std::string coll = (dot != std::string::npos) ? ns.substr(dot + 1) : ns;

    if (_client[db].has_collection(coll))
        throw std::runtime_error("Collection with same name already exists.");

    if (capped) {
        auto b = bsoncxx::builder::basic::document{};
        b.append(kvp("capped", true));
        if (size) b.append(kvp("size", static_cast<int64_t>(size)));
        if (max)  b.append(kvp("max",  static_cast<int32_t>(max)));
        _client[db].create_collection(coll, b.view());
    } else {
        _client[db].create_collection(coll);
    }
}

void MongoClient::renameCollection(const MongoNamespace &ns, const std::string &newCollectionName)
{
    MongoNamespace to(ns.databaseName(), newCollectionName);
    auto res = _client["admin"].run_command(make_document(
        kvp("renameCollection", ns.toString()),
        kvp("to", to.toString()),
        kvp("dropTarget", false)).view());
    if (!cmdOk(res.view()))
        throw std::runtime_error(getErrStr(res.view()));
}

void MongoClient::duplicateCollection(const MongoNamespace &ns, const std::string &newCollectionName)
{
    if (_client[ns.databaseName()].has_collection(newCollectionName))
        throw std::runtime_error("Collection with same name already exists.");
    _client[ns.databaseName()].create_collection(newCollectionName);
    auto cursor = _client[ns.databaseName()][ns.collectionName()].find({});
    for (auto& doc : cursor)
        _client[ns.databaseName()][newCollectionName].insert_one(doc);
}

void MongoClient::dropCollection(const MongoNamespace &ns)
{
    if (!_client[ns.databaseName()].has_collection(ns.collectionName()))
        throw std::runtime_error("Collection does not exist.");
    _client[ns.databaseName()][ns.collectionName()].drop();
}

void MongoClient::copyCollectionToDiffServer(mongocxx::client &fromClient,
                                              const MongoNamespace &from,
                                              const MongoNamespace &to)
{
    if (!_client[to.databaseName()].has_collection(to.collectionName()))
        _client[to.databaseName()].create_collection(to.collectionName());
    auto cursor = fromClient[from.databaseName()][from.collectionName()].find({});
    for (auto& doc : cursor)
        _client[to.databaseName()][to.collectionName()].insert_one(doc);
}

void MongoClient::insertDocument(const mongo::BSONObj &obj, const MongoNamespace &ns)
{
    _client[ns.databaseName()][ns.collectionName()].insert_one(toView(obj));
}

void MongoClient::saveDocument(const mongo::BSONObj &obj, const MongoNamespace &ns)
{
    auto docView = toView(obj);
    auto idElem  = docView["_id"];
    if (idElem) {
        bsoncxx::builder::basic::document filter;
        filter.append(kvp("_id", idElem.get_value()));
        mongocxx::options::replace opts;
        opts.upsert(true);
        _client[ns.databaseName()][ns.collectionName()].replace_one(filter.view(), docView, opts);
    } else {
        _client[ns.databaseName()][ns.collectionName()].insert_one(docView);
    }
}

void MongoClient::removeDocuments(const MongoNamespace &ns, mongo::Query query, bool justOne)
{
    auto filterView = toView(query._filter);
    if (justOne)
        _client[ns.databaseName()][ns.collectionName()].delete_one(filterView);
    else
        _client[ns.databaseName()][ns.collectionName()].delete_many(filterView);
}

std::vector<MongoDocumentPtr> MongoClient::query(const MongoQueryInfo &info)
{
    MongoNamespace ns(info._info._ns);
    std::vector<MongoDocumentPtr> docs;

    if (info._limit == -1)
        return docs;

    if (ns.collectionName().empty() || ns.databaseName().empty())
        return docs;

    mongocxx::options::find findOpts;
    if (info._batchSize > 0) findOpts.batch_size(info._batchSize);
    if (info._limit > 0)     findOpts.limit(info._limit);
    if (info._skip > 0)      findOpts.skip(info._skip);
    if (!info._fields.isEmpty()) findOpts.projection(toView(info._fields));
    if (!info._sort.isEmpty())   findOpts.sort(toView(info._sort));

    auto cursor = _client[ns.databaseName()][ns.collectionName()].find(toView(info._query), findOpts);
    for (auto& doc : cursor)
        docs.push_back(std::make_shared<MongoDocument>(fromView(doc)));
    return docs;
}

MongoCollectionInfo MongoClient::runCollStatsCommand(const std::string &ns)
{
    return MongoCollectionInfo(ns);
}

std::vector<MongoCollectionInfo> MongoClient::runCollStatsCommand(const std::vector<std::string> &namespaces)
{
    std::vector<MongoCollectionInfo> infos;
    for (auto& ns : namespaces) {
        MongoCollectionInfo info = runCollStatsCommand(ns);
        if (info.ns().isValid())
            infos.push_back(info);
    }
    return infos;
}

void MongoClient::done() {}

} // Docutaz
