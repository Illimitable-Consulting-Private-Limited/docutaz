#pragma once

#include <string>

namespace Docutaz
{
    // Width-aware ("fit-or-break", Prettier-style) reformatter for the
    // JavaScript / mongo-shell snippets typed in the query editor.
    //
    // It is a pure text transform: it never executes anything and never touches
    // a server, so the worst it can do is reflow whitespace. As a hard safety
    // guard the formatter compares the significant-token stream of its output
    // against the input; if they differ for any reason (an unbalanced bracket,
    // an unterminated string, an internal slip) it returns the input verbatim
    // rather than risk corrupting the user's text.
    class JsBeautifier
    {
    public:
        // Reformat `source`. `indentWidth` is the number of spaces per level
        // (the editor uses 4); `printWidth` is the soft column at which groups
        // break onto multiple lines. The result has no trailing newline.
        static std::string format(const std::string& source,
                                   int indentWidth = 4,
                                   int printWidth = 80);
    };
}
