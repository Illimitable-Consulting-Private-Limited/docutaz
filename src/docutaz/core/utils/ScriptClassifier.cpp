#include "docutaz/core/utils/ScriptClassifier.h"

#include <QRegularExpression>

namespace Docutaz
{
    namespace ScriptClassifier
    {
        WriteScope classify(const QString &script)
        {
            if (script.trimmed().isEmpty())
                return WriteScope::None;

            // Methods that may touch many documents, plus every structural/mass
            // operation (drop*, create*, rename, mapReduce, bulkWrite). The legacy
            // `insert`/`update`/`remove` forms are multi-capable, so they land here
            // too — the `(?!One|Many)` lookaheads keep the explicit single/many
            // variants from being swallowed by the bare legacy name.
            static const QRegularExpression multiWrite(
                QStringLiteral(
                    "\\.\\s*("
                    "insertMany|updateMany|deleteMany|bulkWrite|"
                    "dropDatabase|dropIndexes|dropIndex|drop|"
                    "renameCollection|createCollection|createIndexes|createIndex|"
                    "createView|mapReduce|reIndex|"
                    "remove|"
                    "insert(?!One|Many)|"
                    "update(?!One|Many)"
                    ")\\s*\\("),
                QRegularExpression::CaseInsensitiveOption);

            if (multiWrite.match(script).hasMatch())
                return WriteScope::Multi;

            // Aggregation pipelines that write their results out rewrite a whole
            // collection — treat as a mass write.
            static const QRegularExpression outStage(
                QStringLiteral("\\$(out|merge)\\b"),
                QRegularExpression::CaseInsensitiveOption);

            if (outStage.match(script).hasMatch())
                return WriteScope::Multi;

            // Methods that touch at most a single document.
            static const QRegularExpression singleWrite(
                QStringLiteral(
                    "\\.\\s*("
                    "insertOne|updateOne|replaceOne|deleteOne|save|"
                    "findOneAndUpdate|findOneAndDelete|findOneAndReplace|findAndModify"
                    ")\\s*\\("),
                QRegularExpression::CaseInsensitiveOption);

            if (singleWrite.match(script).hasMatch())
                return WriteScope::Single;

            return WriteScope::None;
        }

        bool mayModifyData(const QString &script)
        {
            return classify(script) != WriteScope::None;
        }
    }
}
