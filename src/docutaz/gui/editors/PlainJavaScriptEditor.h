#pragma once

#include <Qsci/qsciscintilla.h>

class QContextMenuEvent;

namespace Docutaz
{
    class DocutazScintilla : public QsciScintilla
    {
        Q_OBJECT
    public:
        typedef QsciScintilla BaseClass;
        enum { rowNumberWidth = 6, indentationWidth = 4 };
        static const QColor marginsBackgroundColor;
        static const QColor caretForegroundColor;
        static const QColor matchedBraceForegroundColor;

        DocutazScintilla(QWidget *parent = NULL);
        void setIgnoreEnterKey(bool ignore) { _ignoreEnterKey = ignore; }
        void setIgnoreTabKey(bool ignore) { _ignoreTabKey = ignore; }
        int lineNumberMarginWidth() const;
        int textWidth(int style, const QString &text);
        void setAppropriateBraceMatching();

    public Q_SLOTS:
        // Reflow the current selection (or the whole buffer when nothing is
        // selected) with the Prettier-style JS beautifier. Pure text transform;
        // leaves the text untouched if it cannot be reformatted safely.
        void formatCode();

    protected:
        void wheelEvent(QWheelEvent *e);
        void keyPressEvent(QKeyEvent *e);
        void contextMenuEvent(QContextMenuEvent *e) override;

    private Q_SLOTS:
        void updateLineNumbersMarginWidth();

    private:
        void setLineNumbers(bool displayNumbers);
        void toggleLineNumbers();
        bool _ignoreEnterKey;
        bool _ignoreTabKey;
        int _lineNumberMarginWidth;
        int _lineNumberDigitWidth;
    };
}
