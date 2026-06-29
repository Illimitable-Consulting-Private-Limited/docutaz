#pragma once

#include <QString>

namespace Docutaz
{
    namespace ScriptClassifier
    {
        // Heuristic: does this shell script look like it modifies data?
        //
        // Used only as a production-safety gate — when it returns true on a
        // guarded connection we show an extra confirmation before running. It is
        // deliberately conservative-to-noisy: a false positive costs one extra
        // confirmation, a false negative lets a write through unprompted, so we
        // err toward matching. It does not parse JS (a token inside a string or
        // comment can match); that is acceptable for a confirmation gate.
        //
        // Matches write collection methods invoked as `.method(` (insert/update/
        // delete/remove/save/replace/drop/findAndModify/bulkWrite/createIndex/…)
        // and the `$out`/`$merge` aggregation output stages.
        bool mayModifyData(const QString &script);
    }
}
