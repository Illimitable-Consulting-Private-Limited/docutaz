#pragma once
// BsonBridge: conversion between EJSON strings (from mongosh) and mongo::BSONObj stubs.
// Both types share identical BSON binary wire encoding, so conversion is a byte copy.

#include <bsoncxx/json.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/exception/exception.hpp>

#include "mongo/bson/bsonobj.h"

#include <string>
#include <vector>
#include <stdexcept>

namespace Robomongo {
namespace BsonBridge {

// EJSON string → mongo::BSONObj (copies BSON bytes)
inline mongo::BSONObj ejsonToBson(const std::string& ejson) {
    if (ejson.empty() || ejson == "{}") return {};
    try {
        auto val = bsoncxx::from_json(ejson);
        return mongo::BSONObj(val.view());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ejsonToBson: ") + e.what());
    }
}

// mongo::BSONObj → canonical EJSON string
inline std::string bsonToEjson(const mongo::BSONObj& obj) {
    if (obj.isEmpty()) return "{}";
    try {
        bsoncxx::document::view view(
            reinterpret_cast<const uint8_t*>(obj.objdata()),
            static_cast<std::size_t>(obj.objsize()));
        return bsoncxx::to_json(view, bsoncxx::ExtendedJsonMode::k_canonical);
    } catch (...) {
        return "{}";
    }
}

// bsoncxx::document::view → mongo::BSONObj (copies BSON bytes)
inline mongo::BSONObj fromBsoncxx(const bsoncxx::document::view& view) {
    return mongo::BSONObj(reinterpret_cast<const char*>(view.data()));
}

// Vector of EJSON strings → vector of mongo::BSONObj
inline std::vector<mongo::BSONObj> ejsonArrayToBsonVec(
    const std::vector<std::string>& ejsonArray)
{
    std::vector<mongo::BSONObj> result;
    result.reserve(ejsonArray.size());
    for (const auto& ej : ejsonArray)
        result.push_back(ejsonToBson(ej));
    return result;
}

} // BsonBridge
} // Robomongo
