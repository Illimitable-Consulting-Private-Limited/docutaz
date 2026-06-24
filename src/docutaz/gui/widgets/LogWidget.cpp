#include "docutaz/gui/widgets/LogWidget.h"

#include <QHBoxLayout>
#include <QScrollBar>
#include <QMenu>
#include <QTime>
#include <QAction>
#include <QPlainTextEdit>
#include <QPalette>
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/gui/Theme.h"

namespace Docutaz
{
    LogWidget::LogWidget(QWidget* parent) 
        : BaseClass(parent), _logTextEdit(new QTextEdit(this))
    {
        _logTextEdit->setReadOnly(true);
        _logTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
        VERIFY(connect(_logTextEdit, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint &))));
        QHBoxLayout *hlayout = new QHBoxLayout;
        hlayout->setContentsMargins(0, 0, 0, 0);
        hlayout->addWidget(_logTextEdit);
        _clear = new QAction("Clear All", this);
        VERIFY(connect(_clear, SIGNAL(triggered()), _logTextEdit, SLOT(clear())));
        setLayout(hlayout);      
    }

    void LogWidget::showContextMenu(const QPoint &pt)
    {
        QMenu *menu = _logTextEdit->createStandardContextMenu();
        menu->addAction(_clear);
        _clear->setEnabled(!_logTextEdit->toPlainText().isEmpty());

        menu->exec(_logTextEdit->mapToGlobal(pt));
        delete menu;
    }

    void LogWidget::addMessage(const QString &message, mongo::logger::LogSeverity level)
    {
        // Print time
        const Theme::Tokens &th = Theme::current();

        QTime time = QTime::currentTime();
        _logTextEdit->moveCursor (QTextCursor::End);
        _logTextEdit->setTextColor(th.muted);
        _logTextEdit->insertPlainText(time.toString("h:mm:ss AP") + "\t");

        // Print message
        _logTextEdit->moveCursor (QTextCursor::End);

        // Severity colours follow the theme so they stay legible in both schemes:
        // errors use the danger token, warnings the amber number token, ordinary
        // log lines the muted token, and the rest the plain text colour.
        QColor textColor = th.text;

        if (level == mongo::logger::LogSeverity::Error())
            textColor = th.danger;
        else if (level == mongo::logger::LogSeverity::Log())
            textColor = th.muted;
        else if (level == mongo::logger::LogSeverity::Warning())
            textColor = th.synNumber;

        _logTextEdit->setTextColor(textColor);

        const int maxLength = 500;
        if (message.length() <= maxLength) {
            _logTextEdit->insertPlainText(message.trimmed() + "\n");
        } else {
            _logTextEdit->insertPlainText(QString("(truncated) ") + message.left(maxLength).trimmed() + "...\n");
        }

        // Scroll to the bottom
        QScrollBar *sb = _logTextEdit->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}
