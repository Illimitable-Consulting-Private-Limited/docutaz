#include "docutaz/gui/editors/JsBeautifier.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <vector>

namespace Docutaz
{
namespace
{
    // ----------------------------- Tokenizer -----------------------------
    enum class TT { Ident, Num, Str, Punct, LineComment, BlockComment, Regex, End };
    struct Tok { TT t; std::string s; bool nl = false; };

    bool identStart(char c) { return std::isalpha((unsigned char)c) || c == '_' || c == '$'; }
    bool identCh(char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '$'; }

    // Whether a '/' starting here begins a regex literal (vs. a division
    // operator), inferred from the previous significant token.
    bool regexAllowed(const Tok* prev)
    {
        if (!prev) return true;
        if (prev->t == TT::Ident) {
            static const char* kw[] = { "return", "typeof", "instanceof", "in",
                                        "of", "new", "delete", "void", "do",
                                        "else", "case", "yield", "await" };
            for (auto k : kw) if (prev->s == k) return true;
            return false;
        }
        if (prev->t == TT::Num || prev->t == TT::Str || prev->t == TT::Regex) return false;
        if (prev->t == TT::Punct)
            return !(prev->s == ")" || prev->s == "]" || prev->s == "}");
        return true;
    }

    std::vector<Tok> lex(const std::string& src)
    {
        std::vector<Tok> out;
        size_t i = 0, n = src.size();
        bool nl = false;
        auto last = [&]() -> const Tok* { return out.empty() ? nullptr : &out.back(); };
        while (i < n) {
            char c = src[i];
            if (c == '\n') { nl = true; i++; continue; }
            if (std::isspace((unsigned char)c)) { i++; continue; }
            // comments
            if (c == '/' && i + 1 < n && src[i + 1] == '/') {
                size_t j = i; while (j < n && src[j] != '\n') j++;
                out.push_back({ TT::LineComment, src.substr(i, j - i), nl }); nl = false; i = j; continue;
            }
            if (c == '/' && i + 1 < n && src[i + 1] == '*') {
                size_t j = i + 2; while (j + 1 < n && !(src[j] == '*' && src[j + 1] == '/')) j++;
                j = std::min(n, j + 2);
                out.push_back({ TT::BlockComment, src.substr(i, j - i), nl }); nl = false; i = j; continue;
            }
            // regex vs. divide
            if (c == '/' && regexAllowed(last())) {
                size_t j = i + 1; bool inClass = false;
                while (j < n) {
                    char d = src[j];
                    if (d == '\\') { j += 2; continue; }
                    if (d == '[') inClass = true;
                    else if (d == ']') inClass = false;
                    else if (d == '/' && !inClass) { j++; break; }
                    else if (d == '\n') break;
                    j++;
                }
                while (j < n && std::isalpha((unsigned char)src[j])) j++; // flags
                out.push_back({ TT::Regex, src.substr(i, j - i), nl }); nl = false; i = j; continue;
            }
            // strings (double, single, template)
            if (c == '"' || c == '\'' || c == '`') {
                char q = c; size_t j = i + 1;
                while (j < n) { if (src[j] == '\\') { j += 2; continue; } if (src[j] == q) { j++; break; } j++; }
                out.push_back({ TT::Str, src.substr(i, j - i), nl }); nl = false; i = j; continue;
            }
            // numbers
            if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < n && std::isdigit((unsigned char)src[i + 1]))) {
                size_t j = i;
                if (src[j] == '0' && j + 1 < n && (src[j + 1] == 'x' || src[j + 1] == 'X')) {
                    j += 2; while (j < n && std::isxdigit((unsigned char)src[j])) j++;
                } else {
                    while (j < n && (std::isdigit((unsigned char)src[j]) || src[j] == '.' ||
                           src[j] == 'e' || src[j] == 'E' ||
                           ((src[j] == '+' || src[j] == '-') && (src[j - 1] == 'e' || src[j - 1] == 'E')))) j++;
                }
                out.push_back({ TT::Num, src.substr(i, j - i), nl }); nl = false; i = j; continue;
            }
            // identifiers / keywords
            if (identStart(c)) {
                size_t j = i; while (j < n && identCh(src[j])) j++;
                out.push_back({ TT::Ident, src.substr(i, j - i), nl }); nl = false; i = j; continue;
            }
            // punctuators (longest match first)
            static const char* ops[] = { "===", "!==", "...", "=>", "==", "!=",
                                         "<=", ">=", "&&", "||", "++", "--",
                                         "+=", "-=", "*=", "/=", "??" };
            std::string m;
            for (auto o : ops) {
                size_t L = std::char_traits<char>::length(o);
                if (i + L <= n && src.compare(i, L, o) == 0) { m = o; break; }
            }
            if (m.empty()) m = std::string(1, c);
            out.push_back({ TT::Punct, m, nl }); nl = false; i += m.size();
        }
        out.push_back({ TT::End, "", nl });
        return out;
    }

    // ----------------------- Parse: Text / Bracket tree -----------------------
    struct Bracket;
    struct Node { bool brack = false; std::string text; std::shared_ptr<Bracket> b; };
    struct Element { std::vector<Node> nodes; bool forceBreak = false; };
    struct Bracket { char open = 0, close = 0; std::vector<Element> elements; bool hasComment = false; };

    bool isCloser(const std::string& s) { return s == ")" || s == "]" || s == "}"; }
    bool isOperatorLike(const Tok& t) { return t.t == TT::Punct && !(isCloser(t.s) || t.s == "." || t.s == ";"); }

    bool isComment(const Tok& t) { return t.t == TT::LineComment || t.t == TT::BlockComment; }

    // Render a run of simple (non-bracket) tokens applying inline spacing rules.
    std::string renderText(const std::vector<Tok>& ts)
    {
        std::string out;
        bool noSpaceNext = false;
        for (size_t i = 0; i < ts.size(); ++i) {
            const Tok& cur = ts[i];
            bool space = i > 0;
            if (i > 0) {
                if (cur.s == "." || ts[i - 1].s == ".") space = false;
                else if (cur.s == ":" || cur.s == ";" || cur.s == ",") space = false;
                else if (ts[i - 1].s == "(") space = false;
                if (noSpaceNext) space = false;
            }
            noSpaceNext = false;
            if (cur.s == "-" || cur.s == "+" || cur.s == "!") {
                bool unary = (i == 0) || isOperatorLike(ts[i - 1]) ||
                             ts[i - 1].s == ":" || ts[i - 1].s == "," || ts[i - 1].s == "(";
                if (unary) noSpaceNext = true;
            }
            if (space) out += ' ';
            out += cur.s;
        }
        return out;
    }

    struct Parser {
        const std::vector<Tok>& ts;
        size_t i = 0;
        explicit Parser(const std::vector<Tok>& t) : ts(t) {}

        std::vector<Element> parseElements(char close)
        {
            std::vector<Element> els;
            Element cur;
            std::vector<Tok> pend;
            auto flush = [&] { if (!pend.empty()) { Node n; n.text = renderText(pend); cur.nodes.push_back(n); pend.clear(); } };
            while (ts[i].t != TT::End) {
                const Tok& t = ts[i];
                if (t.t == TT::Punct && !t.s.empty() && t.s[0] == close) { i++; flush(); break; }
                if (t.t == TT::Punct && (t.s == "(" || t.s == "[" || t.s == "{")) {
                    flush();
                    char o = t.s[0]; char cl = o == '(' ? ')' : o == '[' ? ']' : '}';
                    i++;
                    auto b = std::make_shared<Bracket>(); b->open = o; b->close = cl; b->elements = parseElements(cl);
                    for (auto& e : b->elements) if (e.forceBreak) b->hasComment = true;
                    Node n; n.brack = true; n.b = b; cur.nodes.push_back(n);
                    continue;
                }
                if (t.t == TT::Punct && t.s == ",") { flush(); els.push_back(cur); cur = Element(); i++; continue; }
                if (isComment(t)) { flush(); Node n; n.text = t.s; cur.nodes.push_back(n); cur.forceBreak = true; i++; continue; }
                // a stray closer that is not ours: skip to stay tolerant (the
                // token-equality guard will reject the result if this matters).
                if (t.t == TT::Punct && isCloser(t.s) && close != 0 && t.s[0] != close) { i++; continue; }
                pend.push_back(t); i++;
            }
            if (!cur.nodes.empty()) els.push_back(cur);
            return els;
        }

        // Top level: split into statements on ';' and on non-continuation newlines.
        std::vector<Element> parseTop()
        {
            std::vector<Element> stmts;
            Element cur;
            std::vector<Tok> pend;
            auto flush = [&] { if (!pend.empty()) { Node n; n.text = renderText(pend); cur.nodes.push_back(n); pend.clear(); } };
            auto endStmt = [&] { flush(); if (!cur.nodes.empty()) stmts.push_back(cur); cur = Element(); };
            while (ts[i].t != TT::End) {
                const Tok& t = ts[i];
                if (t.nl && (!pend.empty() || !cur.nodes.empty())) {
                    bool cont = (t.t == TT::Punct && (t.s == "." || isCloser(t.s)));
                    const Tok* pv = pend.empty() ? nullptr : &pend.back();
                    if (pv && (isOperatorLike(*pv) || pv->s == "," || pv->s == "(")) cont = true;
                    if (!cont) endStmt();
                }
                if (t.t == TT::Punct && t.s == ";") { endStmt(); i++; continue; }
                if (t.t == TT::Punct && (t.s == "(" || t.s == "[" || t.s == "{")) {
                    flush();
                    char o = t.s[0]; char cl = o == '(' ? ')' : o == '[' ? ']' : '}';
                    i++;
                    auto b = std::make_shared<Bracket>(); b->open = o; b->close = cl; b->elements = parseElements(cl);
                    for (auto& e : b->elements) if (e.forceBreak) b->hasComment = true;
                    Node n; n.brack = true; n.b = b; cur.nodes.push_back(n);
                    continue;
                }
                if (isComment(t)) { flush(); Node n; n.text = t.s; cur.nodes.push_back(n); i++; if (t.t == TT::LineComment) endStmt(); continue; }
                pend.push_back(t); i++;
            }
            endStmt();
            return stmts;
        }
    };

    // ------------------------- Printer (fit or break) -------------------------
    char trailCh(const Node& n) { return n.brack ? n.b->close : (n.text.empty() ? ' ' : n.text.back()); }
    char leadCh(const Node& n) { return n.brack ? n.b->open : (n.text.empty() ? ' ' : n.text.front()); }

    bool noSpaceBetween(const Node& a, const Node& b)
    {
        char tr = trailCh(a), ld = leadCh(b);
        if (ld == '.' || tr == '.') return true;                       // member access
        if (!b.brack && (ld == ',' || ld == ';' || ld == ':' ||
                         ld == ')' || ld == ']' || ld == '}')) return true;
        if (b.brack && (b.b->open == '(' || b.b->open == '[')) {       // call / index
            if (identCh(tr) || tr == ')' || tr == ']') return true;
        }
        return false;
    }

    struct Printer {
        int indent;
        int width;

        std::string ind(int lvl) const { return std::string((size_t)lvl * indent, ' '); }

        std::string inlineEl(const Element& e) const
        {
            std::string out;
            for (size_t k = 0; k < e.nodes.size(); ++k) {
                if (k && !noSpaceBetween(e.nodes[k - 1], e.nodes[k])) out += ' ';
                out += inlineNode(e.nodes[k]);
            }
            return out;
        }
        std::string inlineBr(const Bracket& b) const
        {
            if (b.elements.empty()) return std::string(1, b.open) + std::string(1, b.close);
            std::string inner;
            for (size_t k = 0; k < b.elements.size(); ++k) { if (k) inner += ", "; inner += inlineEl(b.elements[k]); }
            if (b.open == '{') return "{ " + inner + " }";
            return std::string(1, b.open) + inner + std::string(1, b.close);
        }
        std::string inlineNode(const Node& n) const { return n.brack ? inlineBr(*n.b) : n.text; }

        bool elHasComment(const Element& e) const
        {
            for (auto& n : e.nodes)
                if (!n.brack && (n.text.rfind("//", 0) == 0 || n.text.rfind("/*", 0) == 0)) return true;
            return false;
        }
        int callCount(const Element& e) const
        {
            int c = 0; for (auto& n : e.nodes) if (n.brack && n.b->open == '(') c++; return c;
        }
        bool isChain(const Element& e) const
        {
            if (callCount(e) < 2) return false;
            bool saw = false;
            for (const auto& n : e.nodes) {
                if (n.brack) { if (n.b->open == '(') saw = true; }
                else if (saw && !n.text.empty() && n.text[0] == '.') return true;
            }
            return false;
        }

        void printBr(const Bracket& b, int lvl, int& col, std::string& out) const
        {
            std::string fl = inlineBr(b);
            if (!b.hasComment && col + (int)fl.size() <= width) { out += fl; col += (int)fl.size(); return; }
            // Hug a call whose sole argument is one object/array: keep ([ / ({
            // on the opening line instead of adding a redundant indent level.
            if (b.open == '(' && b.elements.size() == 1 &&
                b.elements[0].nodes.size() == 1 && b.elements[0].nodes[0].brack) {
                out += '('; col++; printBr(*b.elements[0].nodes[0].b, lvl, col, out); out += ')'; col++; return;
            }
            out += b.open; out += '\n';
            for (size_t k = 0; k < b.elements.size(); ++k) {
                out += ind(lvl + 1); col = (lvl + 1) * indent;
                printEl(b.elements[k], lvl + 1, col, out);
                if (k + 1 < b.elements.size()) out += ',';
                out += '\n';
            }
            out += ind(lvl); out += b.close; col = lvl * indent + 1;
        }

        // Break a method chain so each .call(...) after the first is its own line.
        void printChain(const Element& e, int lvl, int& col, std::string& out) const
        {
            bool saw = false; int useLvl = lvl;
            for (size_t k = 0; k < e.nodes.size(); ++k) {
                const Node& n = e.nodes[k];
                bool brk = (!n.brack && saw && !n.text.empty() && n.text[0] == '.');
                if (brk) { out += '\n'; out += ind(lvl + 1); col = (lvl + 1) * indent; useLvl = lvl + 1; }
                else if (k && !noSpaceBetween(e.nodes[k - 1], n)) { out += ' '; col++; }
                if (n.brack) { printBr(*n.b, useLvl, col, out); if (n.b->open == '(') saw = true; }
                else { out += n.text; col += (int)n.text.size(); }
            }
        }

        void printEl(const Element& e, int lvl, int& col, std::string& out) const
        {
            std::string fl = inlineEl(e);
            bool fits = col + (int)fl.size() <= width && !elHasComment(e);
            if (!fits && isChain(e)) { printChain(e, lvl, col, out); return; }
            for (size_t k = 0; k < e.nodes.size(); ++k) {
                if (k && !noSpaceBetween(e.nodes[k - 1], e.nodes[k])) { out += ' '; col++; }
                const Node& n = e.nodes[k];
                if (n.brack) printBr(*n.b, lvl, col, out);
                else { out += n.text; col += (int)n.text.size(); }
            }
        }
    };

    // Significant-token signature used to verify the transform is loss-free.
    std::vector<std::pair<TT, std::string>> signature(const std::string& s)
    {
        std::vector<std::pair<TT, std::string>> sig;
        for (const auto& t : lex(s)) { if (t.t == TT::End) break; sig.emplace_back(t.t, t.s); }
        return sig;
    }
}

std::string JsBeautifier::format(const std::string& source, int indentWidth, int printWidth)
{
    if (source.empty()) return source;
    if (indentWidth < 1) indentWidth = 1;
    if (printWidth < 20) printWidth = 20;

    std::string out;
    auto tokens = lex(source);
    Parser parser(tokens);
    auto stmts = parser.parseTop();
    Printer printer{ indentWidth, printWidth };
    for (size_t s = 0; s < stmts.size(); ++s) {
        int col = 0;
        printer.printEl(stmts[s], 0, col, out);
        out += '\n';
    }
    if (!out.empty() && out.back() == '\n') out.pop_back();

    // Hard safety guard: the only legal change is whitespace. If the output's
    // token stream differs from the input's at all, abandon the reformat and
    // return the original text untouched.
    if (signature(out) != signature(source)) return source;
    return out;
}

}
