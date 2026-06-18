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

#include <xlsxwriter.h>

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

        // Resolve a (possibly dotted, when flattening) column and invoke fn with
        // the matching element. fn runs while the intermediate embedded objects
        // are still alive — embeddedObject() here returns an owning copy, so the
        // element must be consumed in-scope (it can't be returned up).
        template <typename Fn>
        void withColumnElement(const mongo::BSONObj &doc, const std::string &col, bool flatten,
                               Fn &&fn)
        {
            if (!flatten) {
                fn(doc.getField(col.c_str()));
                return;
            }
            mongo::BSONObj cur = doc;
            size_t start = 0;
            while (true) {
                const size_t dot = col.find('.', start);
                const std::string seg =
                    col.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
                const mongo::BSONElement e = cur.getField(seg.c_str());
                if (dot == std::string::npos) {
                    fn(e);
                    return;
                }
                if (e.type() != mongo::Object) {
                    fn(mongo::BSONElement());      // intermediate path missing/!object
                    return;
                }
                cur = e.embeddedObject();
                start = dot + 1;
            }
        }

        std::string cellForColumn(const mongo::BSONObj &doc, const std::string &col,
                                  bool flatten, const ExportOptions &opts)
        {
            std::string out;
            withColumnElement(doc, col, flatten,
                              [&](const mongo::BSONElement &e) { out = elementToCell(e, opts); });
            return out;
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

        // Ordered union of column names across all docs, with _id pinned first.
        std::vector<std::string> buildColumns(const std::vector<mongo::BSONObj> &docs, bool flatten)
        {
            std::vector<std::string> cols;
            std::set<std::string> seen;
            for (const auto &d : docs)
                collectColumns(d, flatten, std::string(), cols, seen);
            for (size_t i = 1; i < cols.size(); ++i) {
                if (cols[i] == "_id") {
                    cols.erase(cols.begin() + i);
                    cols.insert(cols.begin(), "_id");
                    break;
                }
            }
            return cols;
        }

        size_t writeCsv(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts,
                        std::ostream &out)
        {
            const std::vector<std::string> cols = buildColumns(docs, opts.flattenNested);

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

        // ---- XLSX (libxlsxwriter) ------------------------------------------
        void writeXlsxCell(lxw_worksheet *ws, lxw_row_t row, lxw_col_t col,
                           const mongo::BSONElement &e, lxw_format *dateFmt,
                           const ExportOptions &opts)
        {
            switch (e.type()) {
                case mongo::EOO:
                case mongo::jstNULL:
                case mongo::Undefined:
                    return;   // leave the cell blank
                case mongo::String:
                    worksheet_write_string(ws, row, col,
                        std::string(e.valuestr(), e.valuestrsize() - 1).c_str(), nullptr);
                    return;
                case mongo::NumberInt:
                    worksheet_write_number(ws, row, col, e._numberInt(), nullptr); return;
                case mongo::NumberLong:
                    worksheet_write_number(ws, row, col,
                        static_cast<double>(e._numberLong()), nullptr); return;
                case mongo::NumberDouble:
                    worksheet_write_number(ws, row, col, e.numberDouble(), nullptr); return;
                case mongo::NumberDecimal:
                    // Excel has no 128-bit decimal; keep full precision as text.
                    worksheet_write_string(ws, row, col, e.numberDecimal().toString().c_str(),
                                           nullptr); return;
                case mongo::Bool:
                    worksheet_write_boolean(ws, row, col, e.boolean() ? 1 : 0, nullptr); return;
                case mongo::jstOID:
                    worksheet_write_string(ws, row, col, e.__oid().toString().c_str(), nullptr);
                    return;
                case mongo::Date: {
                    QDateTime dt = QDateTime::fromMSecsSinceEpoch(e.date().toMillisSinceEpoch(),
                                                                  Qt::UTC);
                    if (opts.timeZone != Utc)
                        dt = dt.toLocalTime();
                    lxw_datetime ld;
                    ld.year  = dt.date().year();
                    ld.month = dt.date().month();
                    ld.day   = dt.date().day();
                    ld.hour  = dt.time().hour();
                    ld.min   = dt.time().minute();
                    ld.sec   = dt.time().second() + dt.time().msec() / 1000.0;
                    worksheet_write_datetime(ws, row, col, &ld, dateFmt);
                    return;
                }
                default:
                    // Object, Array, BinData, … — compact JSON string.
                    worksheet_write_string(ws, row, col, elementToCell(e, opts).c_str(), nullptr);
            }
        }

        size_t writeXlsxImpl(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts,
                             const std::string &filePath, std::string *error)
        {
            const std::vector<std::string> cols = buildColumns(docs, opts.flattenNested);

            lxw_workbook *wb = workbook_new(filePath.c_str());
            if (!wb) {
                if (error) *error = "Could not create workbook: " + filePath;
                return 0;
            }
            lxw_worksheet *ws = workbook_add_worksheet(wb, nullptr);
            lxw_format *header = workbook_add_format(wb);
            format_set_bold(header);
            lxw_format *dateFmt = workbook_add_format(wb);
            format_set_num_format(dateFmt, "yyyy-mm-dd hh:mm:ss");

            for (size_t c = 0; c < cols.size(); ++c)
                worksheet_write_string(ws, 0, static_cast<lxw_col_t>(c), cols[c].c_str(), header);

            // Excel's hard limit is 1,048,576 rows including the header.
            const size_t kMaxDataRows = 1048575;
            size_t written = 0;
            for (const auto &d : docs) {
                if (written >= kMaxDataRows)
                    break;
                const lxw_row_t row = static_cast<lxw_row_t>(written + 1);
                for (size_t c = 0; c < cols.size(); ++c) {
                    const lxw_col_t col = static_cast<lxw_col_t>(c);
                    withColumnElement(d, cols[c], opts.flattenNested,
                        [&](const mongo::BSONElement &e) {
                            writeXlsxCell(ws, row, col, e, dateFmt, opts);
                        });
                }
                ++written;
            }

            const lxw_error err = workbook_close(wb);
            if (err != LXW_NO_ERROR) {
                if (error) *error = std::string("Failed to write .xlsx: ") + lxw_strerror(err);
                return 0;
            }
            return written;
        }
    }

    size_t Exporter::write(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts,
                           std::ostream &out)
    {
        switch (opts.format) {
            case ExportFormat::Csv:  return writeCsv(docs, opts, out);
            case ExportFormat::Json: return writeJson(docs, opts, out);
            default:                 return 0;   // Xlsx goes through writeXlsxFile()
        }
    }

    size_t Exporter::writeXlsxFile(const std::vector<mongo::BSONObj> &docs,
                                   const ExportOptions &opts, const std::string &filePath,
                                   std::string *error)
    {
        return writeXlsxImpl(docs, opts, filePath, error);
    }
}
