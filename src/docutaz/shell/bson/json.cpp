// Replacement json.cpp — parses JSON using bsoncxx::from_json.
// This replaces the 1490-line embedded-shell JSON parser.
//
// bsoncxx::from_json is a strict (RFC-4627) parser: it requires double-quoted
// field names and only understands MongoDB Extended JSON in its $-prefixed form
// ({"$oid": "..."} etc.). The original shell parser was deliberately relaxed —
// it accepted unquoted field names, single-quoted strings and shell type
// constructors (ObjectId("..."), ISODate("..."), NumberLong(...), ...). Users
// type that relaxed form into the Insert/Edit Document dialogs, so before
// handing text to bsoncxx we normalize it back to strict Extended JSON.
//
// The normalizer is string-aware (it never rewrites inside string literals) and
// conservative: anything it doesn't recognise is passed through untouched so
// bsoncxx reports the error rather than us silently producing a different
// document — important for a database tool.

#include "docutaz/shell/bson/json.h"

#include <bsoncxx/json.hpp>
#include <bsoncxx/exception/exception.hpp>

#include <cctype>
#include <cstring>
#include <string>

namespace mongo {
namespace Docutaz {

namespace {

bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// Copies the string literal beginning at s[i] (a quote char) into out, advancing
// i past the closing quote. Single-quoted literals are converted to double
// quotes; embedded double quotes are escaped and \' is unescaped to '.
void copyString(const std::string& s, std::size_t& i, std::string& out) {
    const char quote = s[i];
    const std::size_t n = s.size();
    out.push_back('"');
    ++i;
    while (i < n) {
        const char c = s[i];
        if (c == '\\' && i + 1 < n) {
            const char nx = s[i + 1];
            if (quote == '\'' && nx == '\'') { out.push_back('\''); i += 2; continue; }
            out.push_back('\\');
            out.push_back(nx);
            i += 2;
            continue;
        }
        if (c == quote) { ++i; break; }
        if (c == '"' && quote == '\'') { out += "\\\""; ++i; continue; }
        out.push_back(c);
        ++i;
    }
    out.push_back('"');
}

// Captures the contents between balanced parentheses. On entry i points at '(';
// on return i is just past the matching ')'. Parentheses inside string literals
// are ignored. The returned text is the raw inner argument list.
std::string captureParens(const std::string& s, std::size_t& i) {
    const std::size_t n = s.size();
    std::string inner;
    int depth = 0;
    while (i < n) {
        const char c = s[i];
        if (c == '"' || c == '\'') {
            const char q = c;
            inner.push_back(c);
            ++i;
            while (i < n) {
                if (s[i] == '\\' && i + 1 < n) { inner.push_back(s[i]); inner.push_back(s[i + 1]); i += 2; continue; }
                inner.push_back(s[i]);
                if (s[i] == q) { ++i; break; }
                ++i;
            }
            continue;
        }
        if (c == '(') { ++depth; ++i; if (depth == 1) continue; inner.push_back(c); continue; }
        if (c == ')') { --depth; ++i; if (depth == 0) break; inner.push_back(c); continue; }
        inner.push_back(c);
        ++i;
    }
    return inner;
}

std::string normalize(const std::string& s);

// Converts a recognised shell type constructor into its Extended JSON form.
// Returns false (leaving `out` untouched) for anything unrecognised so the
// caller emits the original text verbatim.
bool tryConstructor(const std::string& name, const std::string& rawArg, std::string& out) {
    const std::string arg = trim(rawArg);

    auto wrap = [&](const char* key, bool forceString) -> std::string {
        std::string value = trim(normalize(arg));
        if (forceString && (value.empty() || value.front() != '"'))
            value = "\"" + value + "\"";
        return std::string("{\"") + key + "\": " + value + "}";
    };

    if (name == "ObjectId")      { out = wrap("$oid", true);            return true; }
    if (name == "ISODate")       { out = wrap("$date", false);          return true; }
    if (name == "Date")          { if (arg.empty()) return false; out = wrap("$date", false); return true; }
    if (name == "NumberLong")    { out = wrap("$numberLong", true);     return true; }
    if (name == "NumberInt")     { out = wrap("$numberInt", true);      return true; }
    if (name == "NumberDecimal") { out = wrap("$numberDecimal", true);  return true; }
    return false;
}

// Rewrites relaxed shell/JS object syntax to strict Extended JSON.
std::string normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    const std::size_t n = s.size();
    std::size_t i = 0;
    while (i < n) {
        const char c = s[i];
        if (c == '"' || c == '\'') { copyString(s, i, out); continue; }

        if (isIdentStart(c)) {
            const std::size_t start = i;
            while (i < n && isIdentChar(s[i])) ++i;
            const std::string ident = s.substr(start, i - start);

            std::size_t j = i;
            while (j < n && std::isspace(static_cast<unsigned char>(s[j]))) ++j;
            const char next = (j < n) ? s[j] : '\0';

            if (next == ':') {           // unquoted object key
                out.push_back('"');
                out += ident;
                out.push_back('"');
                out += s.substr(i, j - i);  // preserve whitespace before ':'
                i = j;
                continue;
            }
            if (ident == "new") {        // drop the JS `new` keyword (e.g. new Date(...))
                continue;
            }
            if (next == '(') {           // type constructor call
                std::size_t k = j;
                const std::string inner = captureParens(s, k);
                std::string repl;
                if (tryConstructor(ident, inner, repl)) {
                    out += repl;
                } else {
                    out += ident;
                    out += s.substr(i, k - i);  // leave unrecognised call verbatim
                }
                i = k;
                continue;
            }
            out += ident;                // keyword/bareword (true/false/null/...)
            continue;
        }

        out.push_back(c);
        ++i;
    }
    return out;
}

}  // namespace

static BSONObj parseWithBsoncxx(const char* str, int* outLen) {
    if (!str || *str == '\0') {
        if (outLen) *outLen = 0;
        return {};
    }

    try {
        auto val = bsoncxx::from_json(normalize(std::string(str)));
        if (outLen) *outLen = static_cast<int>(strlen(str));
        return BSONObj(val.view());
    } catch (const bsoncxx::exception& e) {
        throw ParseMsgAssertionException(
            0, e.what(), 0, std::string(e.what()));
    } catch (const std::exception& e) {
        throw ParseMsgAssertionException(
            0, e.what(), 0, std::string(e.what()));
    }
}

BSONObj fromjson(const std::string& str) {
    return parseWithBsoncxx(str.c_str(), nullptr);
}

BSONObj fromjson(const char* str, int* len) {
    return parseWithBsoncxx(str, len);
}

}  // namespace Docutaz
}  // namespace mongo
