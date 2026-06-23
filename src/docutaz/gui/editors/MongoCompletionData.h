#pragma once

#include <QString>
#include <QStringList>

namespace Docutaz
{
    namespace MongoCompletion
    {
        /**
         * @brief Where, syntactically, the token being completed sits.
         *
         * Classified purely from the text to the left of the cursor — no I/O,
         * no server round-trip. Drives which static word list is offered.
         */
        enum class Context
        {
            Unknown,          // could not classify — offer nothing static
            Global,           // bare token, not after a '.'  (globals / $operators)
            DbMember,         // after "db."                  (db-level methods)
            CollectionMember  // after "db.<collection>."     (collection methods)
        };

        /**
         * @brief Static (Tier 1) completion candidates for @p context, filtered
         * to those that start with @p partial (case-insensitive).
         *
         * The returned values are bare names (e.g. "find", "ObjectId", "$match"),
         * not full dotted paths — the caller prepends the qualifier for member
         * contexts. `$`-prefixed operators are only returned in Global context and
         * only when @p partial itself starts with '$' (a strong, low-noise signal,
         * and one that never fires inside a string literal). Global context returns
         * nothing for an empty @p partial, to avoid popping a list on a bare cursor.
         */
        QStringList staticCandidates(Context context, const QString &partial);
    }
}
