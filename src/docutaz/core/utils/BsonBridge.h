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

namespace Docutaz {
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

// EJSON element strings → a single mongo::BSONObj representing a top-level array.
// Builds a document with ordered numeric keys ("0","1",…) — the BSON wire form
// of an array — then marks it so the GUI renders it as an Array. Elements are
// inserted verbatim and may be any EJSON value (object, number, string, …).
inline mongo::BSONObj ejsonElementsToBsonArray(
    const std::vector<std::string>& elements)
{
    std::string json = "{";
    for (std::size_t i = 0; i < elements.size(); ++i) {
        if (i) json += ',';
        json += '"';
        json += std::to_string(i);
        json += "\":";
        json += elements[i];
    }
    json += '}';
    return ejsonToBson(json).markAsArray();
}

} // BsonBridge
} // Robomongo
