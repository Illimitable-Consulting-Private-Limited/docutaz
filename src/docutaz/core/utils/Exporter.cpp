#include "docutaz/core/utils/Exporter.h"

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <set>
#include <sstream>
#include <string>

#include <QDateTime>

#include <bsoncxx/document/view.hpp>
#include <bsoncxx/json.hpp>

#include "docutaz/core/utils/BsonUtils.h"

namespace Docutaz
{
    namespace
    {
        // ---- JSON ----------------------------------------------------------
        // Relaxed Extended JSON, single line per document (the mongoexport /
        // mongoimport default). BsonUtils::jsonString can't emit single-line
        // output, so go through bsoncxx, which the docs share a wire format with.
        std::string toRelaxedJson(const mongo::BSONObj &obj)
        {
            const bsoncxx::document::view view(
                reinterpret_cast<const uint8_t *>(obj.objdata()),
                static_cast<std::size_t>(obj.objsize()));
            return bsoncxx::to_json(view, bsoncxx::ExtendedJsonMode::k_relaxed);
        }

        size_t writeJson(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts,
                         std::ostream &out)
        {
            if (opts.jsonArray)
                out << "[\n";
            for (size_t i = 0; i < docs.size(); ++i) {
                out << toRelaxedJson(docs[i]);
                if (opts.jsonArray && i + 1 < docs.size())
                    out << ',';
                out << '\n';
            }
            if (opts.jsonArray)
                out << "]\n";
            return docs.size();
        }

        // ---- CSV -----------------------------------------------------------
        std::string isoDate(long long ms, SupportedTimes tz)
        {
            const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms, Qt::UTC);
            const QDateTime shown = (tz == Utc) ? dt : dt.toLocalTime();
            return shown.toString(Qt::ISODateWithMs).toStdString();
        }

        // A single scalar value as a plain (un-quoted) cell. Nested objects/arrays
        // fall back to a compact JSON string so the cell stays self-contained.
        std::string elementToCell(const mongo::BSONElement &e, const ExportOptions &opts)
        {
            switch (e.type()) {
                case mongo::EOO:
                case mongo::jstNULL:
                case mongo::Undefined:
                    return std::string();
                case mongo::String:
                    return std::string(e.valuestr(), e.valuestrsize() - 1);
                case mongo::NumberInt:
                    return std::to_string(e._numberInt());
                case mongo::NumberLong:
                    return std::to_string(e._numberLong());
                case mongo::NumberDouble: {
                    std::ostringstream ss;
                    ss << std::setprecision(15) << e.numberDouble();
                    return ss.str();
                }
                case mongo::NumberDecimal:
                    return e.numberDecimal().toString();
                case mongo::Bool:
                    return e.boolean() ? "true" : "false";
                case mongo::jstOID:
                    return e.__oid().toString();
                case mongo::Date:
                    return isoDate(e.date().toMillisSinceEpoch(), opts.timeZone);
                default:
                    // Object, Array, BinData, RegEx, Timestamp, … — compact JSON.
                    return BsonUtils::jsonString(e, mongo::Strict, false, 0, opts.uuidEncoding,
                                                 opts.timeZone, BsonUtils::isArray(e));
            }
        }

        void collectColumns(const mongo::BSONObj &obj, bool flatten, const std::string &prefix,
                            std::vector<std::string> &cols, std::set<std::string> &seen)
        {
            for (mongo::BSONObjIterator it(obj); it.more();) {
                const mongo::BSONElement e = it.next();
                const std::string key =
                    prefix.empty() ? std::string(e.fieldName()) : prefix + "." + e.fieldName();
                if (flatten && e.type() == mongo::Object) {
                    const mongo::BSONObj sub = e.embeddedObject();   // bind: avoid temp UAF
                    if (sub.isEmpty()) {
                        if (seen.insert(key).second) cols.push_back(key);
                    } else {
                        collectColumns(sub, flatten, key, cols, seen);
                    }
                } else if (seen.insert(key).second) {
                    cols.push_back(key);
                }
            }
        }

        // Resolve a (possibly dotted, when flattening) column to its cell value.
        std::string cellForColumn(const mongo::BSONObj &doc, const std::string &col,
                                  bool flatten, const ExportOptions &opts)
        {
            if (!flatten)
                return elementToCell(doc.getField(col.c_str()), opts);

            mongo::BSONObj cur = doc;          // views doc's buffer; doc outlives us
            size_t start = 0;
            while (true) {
                const size_t dot = col.find('.', start);
                const std::string seg =
                    col.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
                const mongo::BSONElement e = cur.getField(seg.c_str());
                if (dot == std::string::npos)
                    return elementToCell(e, opts);
                if (e.type() != mongo::Object)
                    return std::string();      // intermediate path missing/!object
                cur = e.embeddedObject();
                start = dot + 1;
            }
        }

        std::string csvEscape(const std::string &s)
        {
            if (s.find_first_of(",\"\r\n") == std::string::npos)
                return s;
            std::string out = "\"";
            for (char c : s) {
                if (c == '"') out += "\"\"";
                else out += c;
            }
            out += '"';
            return out;
        }

        size_t writeCsv(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts,
                        std::ostream &out)
        {
            std::vector<std::string> cols;
            std::set<std::string> seen;
            for (const auto &d : docs)
                collectColumns(d, opts.flattenNested, std::string(), cols, seen);

            // _id is the natural primary column — pin it first.
            for (size_t i = 1; i < cols.size(); ++i) {
                if (cols[i] == "_id") {
                    cols.erase(cols.begin() + i);
                    cols.insert(cols.begin(), "_id");
                    break;
                }
            }

            out << "\xEF\xBB\xBF";   // UTF-8 BOM so Excel detects the encoding
            for (size_t i = 0; i < cols.size(); ++i) {
                if (i) out << ',';
                out << csvEscape(cols[i]);
            }
            out << "\r\n";

            for (const auto &d : docs) {
                for (size_t i = 0; i < cols.size(); ++i) {
                    if (i) out << ',';
                    out << csvEscape(cellForColumn(d, cols[i], opts.flattenNested, opts));
                }
                out << "\r\n";
            }
            return docs.size();
        }
    }

    size_t Exporter::write(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts,
                           std::ostream &out)
    {
        switch (opts.format) {
            case ExportFormat::Csv:  return writeCsv(docs, opts, out);
            case ExportFormat::Json:
            default:                 return writeJson(docs, opts, out);
        }
    }
}
