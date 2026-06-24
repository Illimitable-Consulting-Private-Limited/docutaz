#pragma once

#include <Qsci/qsciscintilla.h>

#include <QString>
#include <QVector>

class QContextMenuEvent;

namespace Docutaz
{
    class DocutazScintilla : public QsciScintilla
    {
        Q_OBJECT
    public:
        typedef QsciScintilla BaseClass;
        enum { rowNumberWidth = 6, indentationWidth = 4 };

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
        // Show the syntax-error message as a tooltip when the cursor hovers a
        // squiggled range.
        bool event(QEvent *e) override;

    private Q_SLOTS:
        void updateLineNumbersMarginWidth();
        // One text-change scan that drives the inline indicators: bracket-pair
        // colorization (each bracket tinted by nesting depth) and highlighting of
        // mongo $-operators / field paths ($match, $group, "$amount", ...).
        // Strings/comments/regex are skipped for the bracket depth count.
        void updateSyntaxIndicators();

    private:
        void setLineNumbers(bool displayNumbers);
        void toggleLineNumbers();
        // Build the amber warning-triangle marker shown in the error margin
        // (margin 1) on lines that have a syntax error. Drawn programmatically
        // as an RGBA image so it stays crisp on HiDPI displays.
        void defineErrorMarker();
        // Auto-close brackets and quotes. Returns true (and accepts the event)
        // when it fully handled the key: inserting a matching closer, typing
        // over an existing one, wrapping a selection, or removing an empty pair
        // on Backspace. Returns false to let normal editing proceed.
        bool handleAutoClose(QKeyEvent *e);
        // The closing character for an opener/quote, or a null QChar if the
        // character does not open a pair.
        static QChar autoCloseChar(QChar open);
        bool _ignoreEnterKey;
        bool _ignoreTabKey;
        int _lineNumberMarginWidth;
        int _lineNumberDigitWidth;

        // A structural syntax problem found by updateSyntaxIndicators(): a byte
        // range in the UTF-8 buffer (Scintilla positions) plus the message shown
        // on hover. Recomputed on every text change.
        struct SyntaxError
        {
            int pos;
            int len;
            QString message;
        };
        QVector<SyntaxError> _syntaxErrors;
    };
}
