#pragma once

#include <iosfwd>
#include <vector>

#include <mongo/bson/bsonobj.h>

#include "docutaz/core/Enums.h"

namespace Docutaz
{
    enum class ExportFormat
    {
        Json,   // Extended JSON (relaxed)
        Csv,    // RFC 4180, UTF-8 with BOM (opens in Excel)
        Xlsx    // Native Excel .xlsx via libxlsxwriter
    };

    struct ExportOptions
    {
        ExportFormat format = ExportFormat::Json;
        // Json: true => a single "[ ... ]" array, false => one document per line
        // (JSONL, mongoimport-friendly).
        bool jsonArray = true;
        // Csv: false => a nested object/array is written as a compact JSON string
        // in one cell; true => nested objects are flattened into dot-notation
        // columns (address.city, …) and arrays stay JSON strings.
        bool flattenNested = false;
        UUIDEncoding  uuidEncoding = DefaultEncoding;
        SupportedTimes timeZone   = Utc;
    };

    namespace Exporter
    {
        // Serialise docs to out in the requested format. Returns the number of
        // documents written. Pure logic (no I/O beyond the stream) so it can be
        // unit-tested with a std::ostringstream and reused by the worker with a
        // std::ofstream.
        size_t write(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts,
                     std::ostream &out);

        // XLSX is written straight to a file (libxlsxwriter owns the file I/O),
        // not a stream. Returns the number of rows written (capped at Excel's
        // ~1,048,575-row data limit). On failure returns 0 and sets *error.
        size_t writeXlsxFile(const std::vector<mongo::BSONObj> &docs, const ExportOptions &opts,
                             const std::string &filePath, std::string *error = nullptr);
    }
}
