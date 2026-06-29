#pragma once
// Compatibility stub: provides mongo::BSONObj, mongo::BSONElement, and related types
// backed by bsoncxx raw-byte access. Replaces the old embedded robomongo-shell driver.

#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/document/element.hpp>
#include <bsoncxx/array/view.hpp>
#include <bsoncxx/array/element.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/bson_value/view.hpp>
#include <bsoncxx/decimal128.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/oid.hpp>

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <memory>

namespace mongo {

// ─── StringData (alias for std::string) ────────────────────────────────────
using StringData = std::string;

// ─── StringBuilder ─────────────────────────────────────────────────────────
class StringBuilder {
    std::ostringstream _ss;
public:
    template<typename T>
    StringBuilder& operator<<(const T& v) { _ss << v; return *this; }
    std::string str() const { return _ss.str(); }
    void width(int w) { _ss.width(w); }
    void fill(char c) { _ss.fill(c); }
    void reset() { _ss.str(""); _ss.clear(); }
};

// ─── BSONType enum ─────────────────────────────────────────────────────────
// Values must match BSON wire-format type codes (same as bsoncxx::type)
enum BSONType {
    MinKey      = -1,
    EOO         = 0,
    NumberDouble = 1,
    String      = 2,
    Object      = 3,
    Array       = 4,
    BinData     = 5,
    Undefined   = 6,
    jstOID      = 7,
    Bool        = 8,
    Date        = 9,
    jstNULL     = 10,
    RegEx       = 11,
    DBRef       = 12,
    Code        = 13,
    Symbol      = 14,
    CodeWScope  = 15,
    NumberInt   = 16,
    bsonTimestamp = 17,
    NumberLong  = 18,
    NumberDecimal = 19,
    MaxKey      = 127,
};

// ─── BinDataType enum ──────────────────────────────────────────────────────
enum BinDataType {
    BinDataGeneral      = 0,
    Function            = 1,
    ByteArrayDeprecated = 2,
    bdtUUID             = 3,   // Old UUID (subtype 3)
    newUUID             = 4,   // Standard UUID (subtype 4)
    MD5Type             = 5,
    Encrypt             = 6,
    bdtCustom           = 128,
};

// ─── JsonStringFormat enum ─────────────────────────────────────────────────
enum JsonStringFormat { Strict, TenGen, JS };

// Forward declarations
class BSONElement;
class BSONObjBuilder;
class BSONObjIterator;

// ─── Date_t ────────────────────────────────────────────────────────────────
struct Date_t {
    long long millis = 0;
    explicit Date_t(long long ms = 0) : millis(ms) {}
    long long toMillisSinceEpoch() const { return millis; }
    std::string toString() const { return std::to_string(millis); }
};

// ─── Timestamp_t ───────────────────────────────────────────────────────────
struct Timestamp_t {
    uint32_t i = 0;  // increment
    uint32_t t = 0;  // seconds
    uint32_t getSecs() const { return t; }
    uint32_t getInc()  const { return i; }
};

// ─── Decimal128 ────────────────────────────────────────────────────────────
// Holds the already-rendered decimal string. The value is decoded from the raw
// 16 BSON bytes by BSONElement::numberDecimal() (via bsoncxx::decimal128).
// (This used to be a stub whose toString() always returned "0.0", so every
// NumberDecimal field rendered and exported as 0.0 regardless of its value.)
struct Decimal128 {
    std::string _str = "0.0";
    Decimal128() = default;
    explicit Decimal128(std::string s) : _str(std::move(s)) {}
    std::string toString() const { return _str; }
};

// ─── OID ───────────────────────────────────────────────────────────────────
struct OID {
    uint8_t data[12] = {};
    std::string toString() const {
        char buf[25];
        for (int i = 0; i < 12; ++i)
            snprintf(buf + i * 2, 3, "%02x", data[i]);
        return std::string(buf, 24);
    }
    friend std::ostream& operator<<(std::ostream& os, const OID& oid) {
        return os << oid.toString();
    }
};

// ─── BSONElement ───────────────────────────────────────────────────────────
// Wraps a raw pointer into a BSON document's bytes. Lifetime is tied to the
// owning BSONObj. Access mirrors the old mongo::BSONElement API.
class BSONElement {
    const uint8_t* _raw;  // null → EOO (end-of-object sentinel)

    // Pointer to the value bytes (after type + null-terminated key)
    const char* rawValue() const {
        if (!_raw || *_raw == 0) return nullptr;
        const char* fn = reinterpret_cast<const char*>(_raw) + 1;
        return fn + strlen(fn) + 1;  // skip key + null terminator
    }

public:
    BSONElement() : _raw(nullptr) {}
    explicit BSONElement(const uint8_t* raw) : _raw(raw) {}

    bool eoo() const { return !_raw || *_raw == 0; }
    explicit operator bool() const { return !eoo(); }

    BSONType type() const {
        if (eoo()) return EOO;
        return static_cast<BSONType>(static_cast<int8_t>(*_raw));
    }

    const char* fieldName() const {
        if (eoo()) return "";
        return reinterpret_cast<const char*>(_raw) + 1;
    }

    // Raw element bytes: [type][key\0][value]. Valid only while the owning
    // BSONObj is alive. Used to copy an element verbatim into a builder.
    const uint8_t* rawData() const { return _raw; }

    // Raw value pointer (varies by type)
    const char* value() const { return rawValue(); }

    // ── String / Symbol access ──
    // BSON stores: int32_t size (incl. null) | string bytes | '\0'
    const char* valuestr() const {
        const char* v = rawValue();
        if (!v) return "";
        return v + 4;  // skip int32 size
    }
    int valuestrsize() const {
        const char* v = rawValue();
        if (!v) return 1;
        int32_t sz;
        memcpy(&sz, v, 4);
        return sz;
    }
    std::string String() const {
        if (type() != mongo::String && type() != Symbol) return "";
        const char* s = valuestr();
        int len = valuestrsize() - 1;  // exclude null terminator
        if (len < 0) len = 0;
        return std::string(s, static_cast<size_t>(len));
    }
    const char* valuestrsafe() const {
        if (type() != mongo::String && type() != Symbol) return "";
        return valuestr();
    }

    // ── Number access ──
    double number() const {
        switch (type()) {
        case NumberDouble: {
            double v; memcpy(&v, rawValue(), 8); return v;
        }
        case NumberInt: {
            int32_t v; memcpy(&v, rawValue(), 4); return static_cast<double>(v);
        }
        case NumberLong: {
            int64_t v; memcpy(&v, rawValue(), 8); return static_cast<double>(v);
        }
        default: return 0.0;
        }
    }
    double Double() const {
        if (type() != NumberDouble) return 0.0;
        double v; memcpy(&v, rawValue(), 8); return v;
    }
    double numberDouble() const { return number(); }
    int32_t Int() const {
        if (type() == NumberInt) { int32_t v; memcpy(&v, rawValue(), 4); return v; }
        if (type() == NumberLong) { int64_t v; memcpy(&v, rawValue(), 8); return static_cast<int32_t>(v); }
        return 0;
    }
    int64_t Long() const {
        if (type() == NumberLong) { int64_t v; memcpy(&v, rawValue(), 8); return v; }
        if (type() == NumberInt) { int32_t v; memcpy(&v, rawValue(), 4); return static_cast<int64_t>(v); }
        return 0;
    }
    int64_t safeNumberLong() const { return Long(); }
    int64_t _numberLong() const { return Long(); }
    int32_t _numberInt() const { return Int(); }

    // Decimal128 is stored as 16 little-endian bytes: low 64 bits then high 64.
    Decimal128 numberDecimal() const {
        if (type() != NumberDecimal) return {};
        const char* v = rawValue();
        if (!v) return {};
        uint64_t low, high;
        memcpy(&low, v, 8);
        memcpy(&high, v + 8, 8);
        return Decimal128{ bsoncxx::decimal128(high, low).to_string() };
    }
    Decimal128 _numberDecimal() const { return numberDecimal(); }

    // ── Bool ──
    bool Bool() const {
        if (type() != mongo::Bool) return false;
        return rawValue() && *rawValue() != 0;
    }
    bool boolean() const { return Bool(); }

    // ── Date ──
    Date_t date() const {
        if (type() != mongo::Date) return Date_t{0};
        int64_t ms; memcpy(&ms, rawValue(), 8);
        return Date_t{ms};
    }

    // ── Timestamp ──
    Timestamp_t timestamp() const {
        if (type() != bsonTimestamp) return {};
        uint32_t inc, sec;
        memcpy(&inc, rawValue(), 4);
        memcpy(&sec, rawValue() + 4, 4);
        return {inc, sec};
    }
    uint32_t timestampInc() const { return timestamp().i; }
    Date_t timestampTime() const {
        if (type() != bsonTimestamp) return Date_t{0};
        uint32_t sec; memcpy(&sec, rawValue() + 4, 4);
        return Date_t{static_cast<long long>(sec) * 1000LL};
    }

    // ── OID ──
    OID __oid() const {
        OID oid;
        if (type() == jstOID)
            memcpy(oid.data, rawValue(), 12);
        return oid;
    }

    // ── BinData ──
    BinDataType binDataType() const {
        if (type() != BinData) return BinDataGeneral;
        // Format: int32 length | uint8 subtype | bytes
        return static_cast<BinDataType>(static_cast<uint8_t>(*(rawValue() + 4)));
    }
    const char* binData(int& len) const {
        if (type() != BinData) { len = 0; return nullptr; }
        int32_t sz; memcpy(&sz, rawValue(), 4);
        len = sz;
        return rawValue() + 5;  // skip int32 + subtype byte
    }

    // ── Regex ──
    const char* regex() const {
        if (type() != RegEx) return "";
        return rawValue();  // null-terminated pattern
    }
    const char* regexFlags() const {
        if (type() != RegEx) return "";
        const char* pat = rawValue();
        return pat + strlen(pat) + 1;  // flags after pattern
    }

    // ── Code ──
    std::string _asCode() const {
        if (type() == Code) return String();
        if (type() == CodeWScope) {
            // Format: int32 total size | int32 string size | string | scope doc
            const char* v = rawValue() + 4;  // skip total size
            int32_t slen; memcpy(&slen, v, 4);
            return std::string(v + 4, static_cast<size_t>(slen - 1));
        }
        return "";
    }

    // ── Embedded document / array ──
    class BSONObj embeddedObject() const;
    class BSONObj Obj() const;  // alias for embeddedObject()

    // For array elements: iterate the embedded doc
    std::vector<BSONElement> Array() const;
    std::vector<BSONElement> arrayElements() const;

    // ── Code+scope ──
    class BSONObj codeWScopeObject() const;

    // ── Type predicates ──
    bool isNull() const { return type() == jstNULL; }
    bool isABSONObj() const { return type() == Object || type() == BSONType::Array; }
    bool isSimpleType() const {
        switch (type()) {
        case NumberLong: case NumberDouble: case NumberInt: case mongo::String:
        case mongo::Bool: case jstNULL: return true;
        default: return false;
        }
    }

    // ── String representation ──
    std::string toString(bool /*includeFieldName*/ = false) const {
        switch (type()) {
        case mongo::String: case Symbol: return String();
        case NumberInt: return std::to_string(Int());
        case NumberLong: return std::to_string(Long());
        case NumberDouble: return std::to_string(Double());
        case mongo::Bool: return Bool() ? "true" : "false";
        case jstOID: return __oid().toString();
        case jstNULL: return "null";
        default: return "<element>";
        }
    }

    // ── Total element byte size (including type byte, key, value) ──
    int size() const;
};

// ─── BSONObj ───────────────────────────────────────────────────────────────
class BSONObj {
    // Shared ownership of the raw BSON bytes.
    std::shared_ptr<std::vector<uint8_t>> _owned;
    const uint8_t* _data;  // always valid; points into _owned or static empty doc
    bool _isArray = false;  // true when this doc semantically represents an array

    static const uint8_t kEmptyDoc[5];  // {5,0,0,0,0}

public:
    BSONObj() : _data(kEmptyDoc) {}

    // Construct from raw BSON bytes; copies and takes ownership.
    explicit BSONObj(const char* raw) {
        if (!raw) { _data = kEmptyDoc; return; }
        int32_t sz; memcpy(&sz, raw, 4);
        if (sz < 5) { _data = kEmptyDoc; return; }
        _owned = std::make_shared<std::vector<uint8_t>>(
            reinterpret_cast<const uint8_t*>(raw),
            reinterpret_cast<const uint8_t*>(raw) + sz);
        _data = _owned->data();
    }

    // Construct from bsoncxx view (copies bytes)
    explicit BSONObj(bsoncxx::document::view v) {
        if (v.empty()) { _data = kEmptyDoc; return; }
        _owned = std::make_shared<std::vector<uint8_t>>(v.data(), v.data() + v.length());
        _data = _owned->data();
    }

    // Copy / move
    BSONObj(const BSONObj&) = default;
    BSONObj& operator=(const BSONObj&) = default;
    BSONObj(BSONObj&&) = default;
    BSONObj& operator=(BSONObj&&) = default;

    BSONObj copy() const { return *this; }  // shared_ptr = O(1) "copy"

    const char* objdata() const { return reinterpret_cast<const char*>(_data); }
    int objsize() const {
        int32_t sz; memcpy(&sz, _data, 4); return sz;
    }

    bool isEmpty() const {
        int32_t sz; memcpy(&sz, _data, 4);
        return sz <= 5;
    }
    bool isValid() const { return !isEmpty(); }
    // Robomongo extension: a top-level BSON document whose numeric keys
    // ("0","1",…) should be presented as an array. Set via markAsArray().
    bool isArray() const { return _isArray; }
    BSONObj& markAsArray() { _isArray = true; return *this; }

    // Field access
    BSONElement getField(const char* name) const {
        const uint8_t* p = _data + 4;  // skip document size
        while (*p != 0) {
            const char* key = reinterpret_cast<const char*>(p) + 1;
            BSONElement e(p);
            if (strcmp(key, name) == 0) return e;
            p += e.size();
        }
        return {};
    }
    BSONElement operator[](const char* name) const { return getField(name); }
    BSONElement operator[](const std::string& name) const { return getField(name.c_str()); }

    std::string getStringField(const char* name) const {
        auto e = getField(name);
        return e ? e.String() : std::string{};
    }
    BSONObj getObjectField(const char* name) const;  // defined below
    BSONObj getObjectField(const std::string& name) const { return getObjectField(name.c_str()); }
    bool getBoolField(const char* name) const {
        auto e = getField(name);
        if (!e) return false;
        if (e.type() == mongo::Bool) return e.Bool();
        if (e.type() == NumberInt) return e.Int() != 0;
        return false;
    }
    int getIntField(const char* name) const {
        auto e = getField(name);
        return e ? e.Int() : 0;
    }

    std::string jsonString(JsonStringFormat fmt = TenGen, int pretty = 0) const;

    std::string toString(bool isArray = false) const {
        return jsonString(TenGen, 0);
    }

    bsoncxx::document::view view() const {
        return bsoncxx::document::view(_data, static_cast<std::size_t>(objsize()));
    }
};

// Static empty BSON document (size=5, EOO terminator)
inline const uint8_t BSONObj::kEmptyDoc[5] = {5, 0, 0, 0, 0};

inline std::string BSONObj::jsonString(JsonStringFormat /*fmt*/, int /*pretty*/) const {
    try { return bsoncxx::to_json(view()); } catch (...) { return "{}"; }
}

// ─── Deferred BSONElement methods needing BSONObj ──────────────────────────
inline BSONObj BSONElement::embeddedObject() const {
    if (type() != Object && type() != BSONType::Array) return {};
    return BSONObj(rawValue());
}
inline BSONObj BSONElement::Obj() const { return embeddedObject(); }

inline BSONObj BSONElement::codeWScopeObject() const {
    if (type() != CodeWScope) return {};
    // Format: int32 total | int32 code_len | code | scope_doc
    const char* v = rawValue();
    int32_t total; memcpy(&total, v, 4);
    int32_t code_len; memcpy(&code_len, v + 4, 4);
    const char* scope = v + 4 + 4 + code_len;
    return BSONObj(scope);
}

inline std::vector<BSONElement> BSONElement::Array() const {
    std::vector<BSONElement> result;
    BSONObj arr = embeddedObject();
    const uint8_t* p = reinterpret_cast<const uint8_t*>(arr.objdata()) + 4;
    while (*p != 0) {
        result.emplace_back(p);
        BSONElement tmp(p);
        p += tmp.size();
    }
    return result;
}

inline std::vector<BSONElement> BSONElement::arrayElements() const { return Array(); }

// Total size of a BSON element (type + key + null + value)
inline int BSONElement::size() const {
    if (!_raw || *_raw == 0) return 1;  // EOO is 1 byte
    const char* fn = reinterpret_cast<const char*>(_raw) + 1;
    int keyLen = static_cast<int>(strlen(fn)) + 1;  // include null
    int headerLen = 1 + keyLen;                      // type + key
    const char* v = fn + keyLen;                     // start of value

    switch (type()) {
    case NumberDouble:  return headerLen + 8;
    case mongo::String:
    case Code:
    case Symbol: {
        int32_t sz; memcpy(&sz, v, 4);
        return headerLen + 4 + sz;
    }
    case Object:
    case BSONType::Array: {
        int32_t sz; memcpy(&sz, v, 4);
        return headerLen + sz;
    }
    case BinData: {
        int32_t sz; memcpy(&sz, v, 4);
        return headerLen + 4 + 1 + sz;
    }
    case Undefined:    return headerLen + 0;
    case jstOID:       return headerLen + 12;
    case mongo::Bool:  return headerLen + 1;
    case mongo::Date:  return headerLen + 8;
    case jstNULL:      return headerLen + 0;
    case RegEx: {
        int patLen = static_cast<int>(strlen(v)) + 1;
        int flagLen = static_cast<int>(strlen(v + patLen)) + 1;
        return headerLen + patLen + flagLen;
    }
    case DBRef: {
        int32_t sz; memcpy(&sz, v, 4);
        return headerLen + 4 + sz + 12;
    }
    case CodeWScope: {
        int32_t sz; memcpy(&sz, v, 4);
        return headerLen + sz;
    }
    case NumberInt:    return headerLen + 4;
    case bsonTimestamp: return headerLen + 8;
    case NumberLong:   return headerLen + 8;
    case NumberDecimal: return headerLen + 16;
    case MinKey:
    case MaxKey:       return headerLen + 0;
    default:           return headerLen + 0;
    }
}

inline BSONObj BSONObj::getObjectField(const char* name) const {
    auto e = getField(name);
    if (!e) return {};
    if (e.type() != Object && e.type() != Array) return {};
    return e.embeddedObject();
}

// ─── BSONObjIterator ────────────────────────────────────────────────────────
class BSONObjIterator {
    const uint8_t* _pos;
    const uint8_t* _end;
public:
    explicit BSONObjIterator(const BSONObj& obj) {
        const uint8_t* d = reinterpret_cast<const uint8_t*>(obj.objdata());
        int32_t sz; memcpy(&sz, d, 4);
        _pos = d + 4;
        _end = d + sz - 1;  // exclude terminal EOO
    }
    bool more() const { return _pos < _end && *_pos != 0; }
    BSONElement next() {
        if (!more()) return {};
        BSONElement e(_pos);
        _pos += e.size();
        return e;
    }
};

// ─── BSONCode ───────────────────────────────────────────────────────────────
struct BSONCode {
    std::string code;
    explicit BSONCode(const std::string& c) : code(c) {}
    explicit BSONCode(std::string&& c) : code(std::move(c)) {}
};

// ─── BSONObjBuilder ─────────────────────────────────────────────────────────
// bsoncxx 4.x kvp() requires std::string_view keys, not const char*.
// We use std::string(name) to avoid the mismatch.
class BSONObjBuilder {
    bsoncxx::builder::basic::document _b;

    static std::string _k(const char* s) { return s ? std::string(s) : std::string(); }
    static std::string _k(const std::string& s) { return s; }
public:
    BSONObjBuilder() = default;

    BSONObjBuilder& append(const char* name, const std::string& val) {
        _b.append(bsoncxx::builder::basic::kvp(_k(name), val));
        return *this;
    }
    BSONObjBuilder& append(const char* name, const char* val) {
        _b.append(bsoncxx::builder::basic::kvp(_k(name), std::string(val ? val : "")));
        return *this;
    }
    BSONObjBuilder& append(const char* name, bool val) {
        _b.append(bsoncxx::builder::basic::kvp(_k(name), val));
        return *this;
    }
    BSONObjBuilder& appendBool(const char* name, bool val) {
        return append(name, val);
    }
    BSONObjBuilder& append(const char* name, int val) {
        _b.append(bsoncxx::builder::basic::kvp(_k(name), val));
        return *this;
    }
    BSONObjBuilder& append(const char* name, long long val) {
        _b.append(bsoncxx::builder::basic::kvp(_k(name), static_cast<int64_t>(val)));
        return *this;
    }
    BSONObjBuilder& append(const char* name, double val) {
        _b.append(bsoncxx::builder::basic::kvp(_k(name), val));
        return *this;
    }
    BSONObjBuilder& append(const char* name, const BSONCode& code) {
        _b.append(bsoncxx::builder::basic::kvp(
            _k(name), bsoncxx::types::b_code{code.code}));
        return *this;
    }
    BSONObjBuilder& appendCode(const char* name, const std::string& code) {
        _b.append(bsoncxx::builder::basic::kvp(
            _k(name), bsoncxx::types::b_code{code}));
        return *this;
    }
    BSONObjBuilder& append(const char* name, const BSONObj& obj);  // defined after BSONObj
    BSONObjBuilder& appendElements(const BSONObj& /*obj*/) {
        // skip for now — Phase 1 stub
        return *this;
    }
    BSONObjBuilder& append(const BSONElement& e);  // defined below BSONObjIterator
    BSONObj obj() {
        auto doc = _b.extract();
        return BSONObj(doc.view());
    }
    BSONObj done() { return obj(); }
};

// Defined here so BSONObjBuilder and BSONObj are both complete
inline BSONObjBuilder& BSONObjBuilder::append(const BSONElement& e) {
    if (e.eoo()) return *this;
    // The element's raw bytes ([type][key\0][value]) are a fragment of its
    // parent document. Wrap them in a minimal standalone BSON document so
    // bsoncxx can parse the value generically (ObjectId, string, int, binary
    // UUID, …), then copy that value into this builder under the same key.
    // Previously this was a no-op stub, which silently dropped the element and
    // produced an empty {} document. For the delete path that meant the filter
    // built from a document's _id was empty, so delete_many matched and wiped
    // the *entire* collection instead of the selected rows.
    const int elemLen = e.size();
    const int32_t total = 4 + elemLen + 1;
    std::vector<uint8_t> doc(static_cast<size_t>(total), 0);
    std::memcpy(doc.data(), &total, 4);
    std::memcpy(doc.data() + 4, e.rawData(), static_cast<size_t>(elemLen));
    // doc.back() stays 0: the document's terminating EOO byte.
    bsoncxx::document::view v(doc.data(), static_cast<std::size_t>(total));
    auto parsed = v[e.fieldName()];
    if (parsed)
        _b.append(bsoncxx::builder::basic::kvp(_k(e.fieldName()), parsed.get_value()));
    return *this;
}
inline BSONObjBuilder& BSONObjBuilder::append(const char* name, const BSONObj& obj) {
    _b.append(bsoncxx::builder::basic::kvp(_k(name), obj.view()));
    return *this;
}

// ─── mongo::tojson / mongo::fromjson ─────────────────────────────────────
inline std::string tojson(const BSONObj& obj, JsonStringFormat /*fmt*/ = TenGen, bool /*pretty*/ = false) {
    try { return bsoncxx::to_json(obj.view()); } catch (...) { return "{}"; }
}

// BSON builder macro — very simplified, handles common streaming usage
// e.g.: BSON("key" << 1)
// We implement this as a small builder helper.
struct _BSONBuilder {
    bsoncxx::builder::basic::document _doc;
    std::string _pendingKey;
    bool _haveKey = false;

    // bsoncxx 4.x requires string keys — use std::string for pending key
    _BSONBuilder& operator<<(const char* s) {
        if (!_haveKey) { _pendingKey = s; _haveKey = true; }
        else { _doc.append(bsoncxx::builder::basic::kvp(_pendingKey, std::string(s))); _haveKey = false; }
        return *this;
    }
    _BSONBuilder& operator<<(const std::string& s) {
        if (!_haveKey) { _pendingKey = s; _haveKey = true; }
        else { _doc.append(bsoncxx::builder::basic::kvp(_pendingKey, s)); _haveKey = false; }
        return *this;
    }
    _BSONBuilder& operator<<(int32_t v) {
        if (_haveKey) { _doc.append(bsoncxx::builder::basic::kvp(_pendingKey, v)); _haveKey = false; }
        return *this;
    }
    _BSONBuilder& operator<<(int64_t v) {
        if (_haveKey) { _doc.append(bsoncxx::builder::basic::kvp(_pendingKey, v)); _haveKey = false; }
        return *this;
    }
    _BSONBuilder& operator<<(double v) {
        if (_haveKey) { _doc.append(bsoncxx::builder::basic::kvp(_pendingKey, v)); _haveKey = false; }
        return *this;
    }
    _BSONBuilder& operator<<(bool v) {
        if (_haveKey) { _doc.append(bsoncxx::builder::basic::kvp(_pendingKey, v)); _haveKey = false; }
        return *this;
    }

    BSONObj obj() {
        return BSONObj(_doc.view());
    }
};

#define BSON(x) ([&]() -> ::mongo::BSONObj { ::mongo::_BSONBuilder _b; _b << x; return _b.obj(); }())

// ─── StatusWith<T> (stub for ConnectionBasicTab.cpp) ─────────────────────
template<typename T>
class StatusWith {
    T _val;
    bool _ok;
    std::string _errMsg;
public:
    StatusWith() : _ok(false) {}
    explicit StatusWith(T val) : _val(std::move(val)), _ok(true) {}
    StatusWith(bool /*ignored*/, std::string msg) : _ok(false), _errMsg(std::move(msg)) {}
    bool isOK() const { return _ok; }
    T& getValue() { return _val; }
    const T& getValue() const { return _val; }
    struct Status {
        std::string msg;
        std::string toString() const { return msg; }
    };
    Status getStatus() const { return {_errMsg}; }
};

// ─── ConnectionString (stub) ─────────────────────────────────────────────
// Forward-declare HostAndPort so we can use it here
struct ConnectionString {
    enum ConnectionType { MASTER, SET, SYNC, CUSTOM, INVALID };
    ConnectionType _type = MASTER;
    std::string _str;

    ConnectionString() = default;
    // Constructed from HostAndPort (needed by ConnectionAdvancedTab)
    template<typename HP>
    explicit ConnectionString(const HP& hp) : _str(hp.toString()) {}

    ConnectionType type() const { return _type; }
    std::string toString() const { return _str; }
    void append(const std::string& s) { _str += s; }
};

// ─── BSONArray ──────────────────────────────────────────────────────────────
// BSONArray is a BSONObj that is semantically an array.
using BSONArray = BSONObj;

}  // namespace mongo
