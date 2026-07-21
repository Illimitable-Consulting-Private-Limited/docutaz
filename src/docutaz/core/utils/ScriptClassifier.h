#pragma once

#include <QString>

namespace Docutaz
{
    namespace ScriptClassifier
    {
        // How many documents a write can touch — the axis the production-safety
        // gate uses to decide whether to prompt. `Multi` also covers structural
        // and mass operations (drop, dropDatabase, $out/$merge, bulkWrite), which
        // always warrant a confirmation.
        enum class WriteScope
        {
            None,   // not a write at all — never prompt
            Single, // touches at most one document (updateOne/deleteOne/save/...)
            Multi   // may touch many, or is mass/structural (updateMany/remove/drop/...)
        };

        // Heuristic: what is the write scope of this shell script?
        //
        // Used only as a production-safety gate — a `Multi` result always prompts
        // on a guarded connection, a `Single` result prompts only when the user
        // opts into single-document warnings. It is deliberately conservative: it
        // does not parse JS (a token inside a string or comment can match), and
        // anything ambiguous is treated as `Multi`, because under-warning on a
        // mass write is the worse failure. So a false `Multi` costs one extra
        // confirmation; we never silently downgrade a real mass write to `Single`.
        //
        // `Single`: updateOne/deleteOne/replaceOne/insertOne/save and the
        //   findOneAndUpdate|Delete|Replace family.
        // `Multi` : updateMany/deleteMany/insertMany/bulkWrite/mapReduce, the
        //   legacy multi-capable update(...)/remove(...), every drop*/create*/
        //   reIndex/renameCollection structural op, and $out/$merge stages.
        WriteScope classify(const QString &script);

        // Convenience wrapper for callers that only care whether a script writes
        // at all (classify(script) != WriteScope::None).
        bool mayModifyData(const QString &script);
    }
}
