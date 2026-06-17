#include "docutaz/gui/editors/JSLexer.h"

#include <Qsci/qscilexerjavascript.h>

namespace Docutaz
{
    JSLexer::JSLexer(QObject *parent) : QsciLexerJavaScript(parent)
    {
    }

    QColor JSLexer::defaultPaper(int style) const
    {
        return QColor(73, 76, 78);
        //return QColor(48, 10, 36); // Ubuntu-style background
    }

    QColor JSLexer::defaultColor(int style) const
    {
        switch (style)
        {
        case Default:
            return QColor("#FFFFFF");

        case Comment:
        case CommentLine:
            return QColor("#999999");

        case CommentDoc:
        case CommentLineDoc:
            return QColor("#999999");

        case Number:
            //return QColor("#DBF76C");
            return QColor("#FFA09E");

        case Keyword:
            //return QColor("#FDE15D");
            return QColor("#BEE5FF");

        case KeywordSet2:
            // mongo-shell globals, BSON type constructors and the iconic
            // collection/db methods — a warm orange that stands apart from the
            // light-blue JavaScript keywords.
            return QColor("#FFB86C");

        case DoubleQuotedString:
        case SingleQuotedString:
        case RawString:
            //return QColor("#5ED363");
            return QColor("#C6F079");

        case PreProcessor:
            return QColor("#00FF00");

        case Operator:
        case UnclosedString:
            //return QColor("#FF7729");
            //return QColor("#AFBED4");
            return QColor("#FFD14D");


        case Regex:
            return QColor("#FFFFFF");

        case CommentDocKeyword:
            return QColor("#FFFFFF");

        case CommentDocKeywordError:
            return QColor("#FFFFFF");

        case InactiveDefault:
        case InactiveUUID:
        case InactiveCommentLineDoc:
        case InactiveKeywordSet2:
        case InactiveCommentDocKeyword:
        case InactiveCommentDocKeywordError:
            return QColor("#FFFFFF");

        case InactiveComment:
        case InactiveCommentLine:
        case InactiveNumber:
            return QColor("#FFFFFF");

        case InactiveCommentDoc:
            return QColor("#FFFFFF");

        case InactiveKeyword:
            return QColor("#FFFFFF");

        case InactiveDoubleQuotedString:
        case InactiveSingleQuotedString:
        case InactiveRawString:
            return QColor("#FFFFFF");

        case InactivePreProcessor:
            return QColor("#FFFFFF");

        case InactiveOperator:
        case InactiveIdentifier:
        case InactiveGlobalClass:
            return QColor("#FFFFFF");

        case InactiveUnclosedString:
            return QColor("#FFFFFF");

        case InactiveVerbatimString:
            return QColor("#FFFFFF");

        case InactiveRegex:
            return QColor("#FFFFFF");
        }

        return QColor("#FFFFFF");
        //    return QsciLexer::defaultColor(style);
    }

    const char *JSLexer::keywords(int set) const
    {
        // Set 1 (Keyword style): standard JavaScript keywords and literals.
        if (set == 1)
            return
                "abstract async await boolean break byte case catch char class "
                "const continue debugger default delete do double else enum export "
                "extends false final finally float for function goto if implements "
                "import in instanceof int interface let long native new null of "
                "package private protected public return short static super switch "
                "synchronized this throw throws transient true try typeof var void "
                "volatile while with yield ";

        // Set 2 (KeywordSet2 style): mongo-shell globals, BSON type constructors
        // and the iconic collection / database / cursor methods.
        if (set == 2)
            return
                "db rs sh "
                "ObjectId ISODate Date Timestamp BinData UUID LUUID PYUUID CSUUID "
                "JUUID NUUID NumberInt NumberLong NumberDecimal Number DBRef MinKey "
                "MaxKey Mongo _id "
                "find findOne findOneAndUpdate findOneAndReplace findOneAndDelete "
                "aggregate insertOne insertMany updateOne updateMany deleteOne "
                "deleteMany replaceOne countDocuments estimatedDocumentCount distinct "
                "bulkWrite createIndex createIndexes dropIndex dropIndexes getIndexes "
                "explain hint pretty sort limit skip getSiblingDB getCollection "
                "getCollectionNames createCollection runCommand drop renameCollection "
                "stats ";

        return 0;
    }
}
