#include "docutaz/gui/editors/PlainJavaScriptEditor.h"

#include <QPainter>
#include <QApplication>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/editors/JsBeautifier.h"
#include "docutaz/core/utils/QtUtils.h"

namespace
{
    /**
    * @brief Returns the number of digits in an 32-bit integer
    * http://stackoverflow.com/questions/1489830/efficient-way-to-determine-number-of-digits-in-an-integer
    */
    int getNumberOfDigits(int x)
    {
        if (x < 0) return getNumberOfDigits(-x) + 1;

        if (x >= 10000) {
            if (x >= 10000000) {
                if (x >= 100000000) {
                    if (x >= 1000000000)
                        return 10;
                    return 9;
                }
                return 8;
            }
            if (x >= 100000) {
                if (x >= 1000000)
                    return 7;
                return 6;
            }
            return 5;
        }
        if (x >= 100) {
            if (x >= 1000)
                return 4;
            return 3;
        }
        if (x >= 10)
            return 2;
        return 1;
    }
}

namespace Docutaz
{
    const QColor DocutazScintilla::marginsBackgroundColor = QColor(73, 76, 78);
    const QColor DocutazScintilla::caretForegroundColor = QColor("#FFFFFF");
    const QColor DocutazScintilla::matchedBraceForegroundColor = QColor("#FF8861");

    DocutazScintilla::DocutazScintilla(QWidget *parent) : QsciScintilla(parent),
        _ignoreEnterKey(false),
        _ignoreTabKey(false),
        _lineNumberDigitWidth(0),
        _lineNumberMarginWidth(0)
    {
        setAutoIndent(true);
        setIndentationsUseTabs(false);
        setIndentationWidth(indentationWidth);
        setUtf8(true);
        setMarginWidth(1, 0);
        setCaretForegroundColor(caretForegroundColor);
        setMatchedBraceForegroundColor(matchedBraceForegroundColor); //1AB0A6
        setMatchedBraceBackgroundColor(marginsBackgroundColor);
        setContentsMargins(0, 0, 0, 0);
        setViewportMargins(3, 3, 3, 3);
        QFont ourFont = GuiRegistry::instance().font();
        setMarginsFont(ourFont);
        setMarginLineNumbers(0, true);
        setMarginsBackgroundColor(QColor(53, 56, 58));
        setMarginsForegroundColor(QColor(173, 176, 178));

        SendScintilla(QsciScintilla::SCI_STYLESETFONT, 1, ourFont.family().data());
        SendScintilla(QsciScintilla::SCI_SETHSCROLLBAR, 0);

        setWrapMode((QsciScintilla::WrapMode)QsciScintilla::SC_WRAP_NONE);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); 
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); 

        // Cache width of one digit
#ifdef Q_OS_WIN
        _lineNumberDigitWidth = rowNumberWidth;
#else
        _lineNumberDigitWidth = textWidth(STYLE_LINENUMBER, "0");
#endif
        updateLineNumbersMarginWidth();

        setLineNumbers(AppRegistry::instance().settingsManager()->lineNumbers());
        setUtf8(true);
        VERIFY(connect(this, SIGNAL(linesChanged()), this, SLOT(updateLineNumbersMarginWidth())));
    }

    int DocutazScintilla::lineNumberMarginWidth() const
    {
        return marginWidth(0);
    }

    int DocutazScintilla::textWidth(int style, const QString &text)
    {
        const char *byteArray = (text.toUtf8()).constData();
        return SendScintilla(SCI_TEXTWIDTH, style, byteArray);
    }

    void DocutazScintilla::wheelEvent(QWheelEvent *e)
    {
        if (this->isActiveWindow()) {
            QsciScintilla::wheelEvent(e);
        }
        else {
            qApp->sendEvent(parentWidget(), e);
            e->accept();
        }
    }

    void DocutazScintilla::setLineNumbers(bool displayNumbers)
    {
        if (displayNumbers) {
            setMarginWidth(0, _lineNumberMarginWidth);
        }
        else {
            setMarginWidth(0, 0);
        }
    }

    void DocutazScintilla::toggleLineNumbers()
    {
        setLineNumbers(!lineNumberMarginWidth());
    }

    void DocutazScintilla::keyPressEvent(QKeyEvent *keyEvent)
    {
        if (_ignoreEnterKey) {
            if (keyEvent->key() == Qt::Key_Return) {
                keyEvent->ignore();
                _ignoreEnterKey = false;
                return;
            }
        }

        if (_ignoreTabKey) {
            if (keyEvent->key() == Qt::Key_Tab) {
                keyEvent->ignore();
                _ignoreTabKey = false;
                return;
            }
        }

        if (keyEvent->key() == Qt::Key_F11) {
            keyEvent->ignore();
            toggleLineNumbers();
            return;
        }

        if ((keyEvent->modifiers() & Qt::ControlModifier) &&
            (keyEvent->modifiers() & Qt::ShiftModifier) &&
            keyEvent->key() == Qt::Key_F) {
            keyEvent->accept();
            formatCode();
            return;
        }

        if (((keyEvent->modifiers() & Qt::ControlModifier) &&
            (keyEvent->key() == Qt::Key_F4 || keyEvent->key() == Qt::Key_W ||
             keyEvent->key() == Qt::Key_T || keyEvent->key() == Qt::Key_Space ||
             keyEvent->key() == Qt::Key_F || keyEvent->key() == Qt::Key_Slash))
            || keyEvent->key() == Qt::Key_Escape /*|| keyEvent->key() == Qt::Key_Return*/
            || ((keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->modifiers() & Qt::AltModifier) && keyEvent->key() == Qt::Key_Left)
            || ((keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->modifiers() & Qt::AltModifier) && keyEvent->key() == Qt::Key_Right)
            || ((keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->modifiers() & Qt::ShiftModifier) && keyEvent->key() == Qt::Key_C)
           )
        {
            keyEvent->ignore();
        }
        else
        {
            BaseClass::keyPressEvent(keyEvent);
        }
    }

    void DocutazScintilla::updateLineNumbersMarginWidth()
    {
        int numberOfDigits = getNumberOfDigits(lines());
        _lineNumberMarginWidth = numberOfDigits * _lineNumberDigitWidth + rowNumberWidth;

        // If line numbers margin already displayed, update its width
        if (lineNumberMarginWidth()) {
            setMarginWidth(0, _lineNumberMarginWidth);
        }
    }

    void DocutazScintilla::contextMenuEvent(QContextMenuEvent *e)
    {
        // QsciScintilla's built-in context menu cannot be extended, so build a
        // standard edit menu and append the Format action.
        QMenu menu(this);

        QAction *undoAct = menu.addAction(tr("&Undo"));
        undoAct->setShortcut(QKeySequence::Undo);
        undoAct->setEnabled(isUndoAvailable());
        VERIFY(connect(undoAct, SIGNAL(triggered()), this, SLOT(undo())));

        QAction *redoAct = menu.addAction(tr("&Redo"));
        redoAct->setShortcut(QKeySequence::Redo);
        redoAct->setEnabled(isRedoAvailable());
        VERIFY(connect(redoAct, SIGNAL(triggered()), this, SLOT(redo())));

        menu.addSeparator();

        QAction *cutAct = menu.addAction(tr("Cu&t"));
        cutAct->setShortcut(QKeySequence::Cut);
        cutAct->setEnabled(hasSelectedText());
        VERIFY(connect(cutAct, SIGNAL(triggered()), this, SLOT(cut())));

        QAction *copyAct = menu.addAction(tr("&Copy"));
        copyAct->setShortcut(QKeySequence::Copy);
        copyAct->setEnabled(hasSelectedText());
        VERIFY(connect(copyAct, SIGNAL(triggered()), this, SLOT(copy())));

        QAction *pasteAct = menu.addAction(tr("&Paste"));
        pasteAct->setShortcut(QKeySequence::Paste);
        VERIFY(connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste())));

        menu.addSeparator();

        QAction *formatAct = menu.addAction(tr("&Format Code"));
        formatAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
        formatAct->setEnabled(!text().isEmpty());
        VERIFY(connect(formatAct, SIGNAL(triggered()), this, SLOT(formatCode())));

        menu.addSeparator();

        QAction *selectAllAct = menu.addAction(tr("Select &All"));
        selectAllAct->setShortcut(QKeySequence::SelectAll);
        VERIFY(connect(selectAllAct, SIGNAL(triggered()), this, SLOT(selectAll())));

        menu.exec(e->globalPos());
    }

    void DocutazScintilla::formatCode()
    {
        const bool hasSel = hasSelectedText();
        const std::string src =
            QtUtils::toStdString(hasSel ? selectedText() : text());
        if (src.empty())
            return;

        const std::string out = JsBeautifier::format(src, indentationWidth);
        if (out == src)   // nothing to do, or the beautifier safely bailed out
            return;

        const QString formatted = QtUtils::toQString(out);
        if (hasSel) {
            replaceSelectedText(formatted);
        } else {
            int line = 0, index = 0;
            getCursorPosition(&line, &index);
            setText(formatted);
            // Best-effort: keep the caret on roughly the same line.
            setCursorPosition(std::min(line, lines() - 1), 0);
        }
    }

    void DocutazScintilla::setAppropriateBraceMatching() {
#ifdef Q_OS_MAC
        // On Mac OS when brace matching is enabled, text
        // will blink when you move cursor to some brace or
        // when inside braces. This behaviour is not fully fixed
        // in QScintilla 2.9.1 and 2.8.4
        setBraceMatching(QsciScintilla::NoBraceMatch);
#else
        setBraceMatching(QsciScintilla::StrictBraceMatch);
#endif
    }


}
