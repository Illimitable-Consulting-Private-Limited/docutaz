#include "docutaz/core/utils/ScriptClassifier.h"

#include <QRegularExpression>

namespace Docutaz
{
    namespace ScriptClassifier
    {
        bool mayModifyData(const QString &script)
        {
            if (script.trimmed().isEmpty())
                return false;

            // Collection write methods, matched as `.method(` so a field or
            // variable that merely contains one of these words doesn't trip it.
            static const QRegularExpression writeMethod(
                QStringLiteral(
                    "\\.\\s*(insert|insertOne|insertMany|save|update|updateOne|"
                    "updateMany|replaceOne|remove|delete|deleteOne|deleteMany|"
                    "drop|dropDatabase|dropIndex|dropIndexes|renameCollection|"
                    "findAndModify|findOneAndUpdate|findOneAndDelete|"
                    "findOneAndReplace|bulkWrite|createCollection|createIndex|"
                    "createIndexes|createView|mapReduce|reIndex)\\s*\\("),
                QRegularExpression::CaseInsensitiveOption);

            if (writeMethod.match(script).hasMatch())
                return true;

            // Aggregation pipelines that write their results out.
            static const QRegularExpression outStage(
                QStringLiteral("\\$(out|merge)\\b"),
                QRegularExpression::CaseInsensitiveOption);

            return outStage.match(script).hasMatch();
        }
    }
}
