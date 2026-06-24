#include "docutaz/gui/editors/JSLexer.h"

#include <Qsci/qscilexerjavascript.h>

#include "docutaz/gui/Theme.h"

namespace Docutaz
{
    JSLexer::JSLexer(QObject *parent) : QsciLexerJavaScript(parent)
    {
    }

    QColor JSLexer::defaultPaper(int /*style*/) const
    {
        // The editor canvas follows the active light/dark scheme. QScintilla
        // renders independently of QPalette/QSS, so the colour is read here and
        // re-applied when the theme changes (see PlainJavaScriptEditor).
        return Theme::current().editorCanvas;
    }

    QColor JSLexer::defaultColor(int style) const
    {
        const Theme::Tokens &t = Theme::current();

        switch (style)
        {
        case Comment:
        case CommentLine:
        case CommentDoc:
        case CommentLineDoc:
            return t.synComment;

        case Number:
            return t.synNumber;

        case Keyword:
        case KeywordSet2:   // mongo-shell globals / BSON ctors / collection methods
        case PreProcessor:
            return t.synKeyword;

        case DoubleQuotedString:
        case SingleQuotedString:
        case RawString:
        case UnclosedString:
            return t.synString;

        case Operator:
            return t.synOperator;

        // Inactive (greyed-out) regions read as muted in either scheme.
        case InactiveDefault:
        case InactiveUUID:
        case InactiveComment:
        case InactiveCommentLine:
        case InactiveCommentDoc:
        case InactiveCommentLineDoc:
        case InactiveNumber:
        case InactiveKeyword:
        case InactiveKeywordSet2:
        case InactiveDoubleQuotedString:
        case InactiveSingleQuotedString:
        case InactiveRawString:
        case InactiveUnclosedString:
        case InactiveVerbatimString:
        case InactivePreProcessor:
        case InactiveOperator:
        case InactiveIdentifier:
        case InactiveGlobalClass:
        case InactiveRegex:
        case InactiveCommentDocKeyword:
        case InactiveCommentDocKeywordError:
            return t.synComment;

        case Default:
        case Regex:
        case CommentDocKeyword:
        case CommentDocKeywordError:
        default:
            return t.text;
        }
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
