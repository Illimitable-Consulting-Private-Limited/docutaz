#include "docutaz/gui/dialogs/ToolRunnerDialog.h"

#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QProgressBar>
#include <QFontDatabase>
#include <QMessageBox>
#include <QTextCursor>
#include <QPalette>

#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{
    ToolRunnerDialog::ToolRunnerDialog(QWidget* parent)
        : QDialog(parent)
    {
        auto* outer = new QVBoxLayout(this);

        // Subclass form goes at the top.
        _formLayout = new QVBoxLayout;
        outer->addLayout(_formLayout);

        // Live output log — monospaced, read-only, grows to fill the dialog.
        _log = new QPlainTextEdit(this);
        _log->setReadOnly(true);
        _log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        _log->setPlaceholderText("Output will appear here while the operation runs.");
        _log->setMinimumHeight(200);
        outer->addWidget(_log, 1);

        // Prominent busy indicator — an indeterminate ("marching") bar shown only
        // while a tool is running, so a long dump/restore clearly looks alive.
        _progress = new QProgressBar(this);
        _progress->setTextVisible(false);
        _progress->setMinimumHeight(10);
        _progress->setVisible(false);
        outer->addWidget(_progress);

        _status = new QLabel(this);
        _status->setWordWrap(true);
        _status->setTextFormat(Qt::PlainText);
        outer->addWidget(_status);

        _buttonBox = new QDialogButtonBox(this);
        _runButton = _buttonBox->addButton("Run", QDialogButtonBox::AcceptRole);
        _cancelButton = _buttonBox->addButton("Stop", QDialogButtonBox::ActionRole);
        _buttonBox->addButton(QDialogButtonBox::Close);
        _cancelButton->setEnabled(false);
        outer->addWidget(_buttonBox);

        VERIFY(connect(_runButton, &QPushButton::clicked, this, &ToolRunnerDialog::start));
        VERIFY(connect(_cancelButton, &QPushButton::clicked, this, &ToolRunnerDialog::cancelRun));
        VERIFY(connect(_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject));

        resize(720, 640);
    }

    void ToolRunnerDialog::setRunButtonText(const QString& text)
    {
        _runButton->setText(text);
    }

    void ToolRunnerDialog::appendLog(const QString& text)
    {
        _log->appendPlainText(text);
    }

    void ToolRunnerDialog::start()
    {
        QString error;
        if (!validate(error)) {
            QMessageBox::warning(this, "Cannot start", error);
            return;
        }

        const QString path = toolPath();
        if (path.isEmpty()) {
            QMessageBox::warning(this, toolName() + " not found", notFoundMessage());
            return;
        }

        const QList<QStringList> sets = argumentSets();
        if (sets.isEmpty()) {
            QMessageBox::warning(this, "Nothing to do", "There is nothing selected to run.");
            return;
        }

        if (!confirm())
            return;

        _log->clear();
        _userCancelled = false;
        _pending = sets;
        _stepIndex = 0;
        _stepTotal = sets.size();
        setRunning(true);
        runNextStep();
    }

    void ToolRunnerDialog::runNextStep()
    {
        if (_userCancelled)
            return;
        if (_stepIndex >= _pending.size()) {
            // All steps completed successfully.
            setRunning(false);
            setStatus(_stepTotal > 1
                ? QString("Completed successfully (%1 operations).").arg(_stepTotal)
                : "Completed successfully.", Status::Success);
            onSucceeded();
            // Prominent confirmation so success is obvious even if the log scrolled.
            QMessageBox::information(this, successTitle(), successMessage());
            return;
        }

        const QString path = toolPath();
        const QStringList args = _pending[_stepIndex];
        if (_stepTotal > 1)
            appendLog(QString("\n=== Step %1 of %2 ===").arg(_stepIndex + 1).arg(_stepTotal));
        appendLog("$ " + path + " " + args.join(' ') + "\n");

        _proc = new QProcess(this);
        // mongodump/restore/export/import write progress to stderr; merge so the
        // log shows everything in order.
        _proc->setProcessChannelMode(QProcess::MergedChannels);
        VERIFY(connect(_proc, &QProcess::readyRead, this, &ToolRunnerDialog::onReadyRead));
        VERIFY(connect(_proc, &QProcess::errorOccurred, this, &ToolRunnerDialog::onErrorOccurred));
        VERIFY(connect(_proc,
                       QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                       this, &ToolRunnerDialog::onFinished));

        setStatus(_stepTotal > 1
            ? QString("Running %1 (%2/%3)…").arg(toolName()).arg(_stepIndex + 1).arg(_stepTotal)
            : "Running " + toolName() + "…", Status::Running);
        _proc->start(path, args);
    }

    void ToolRunnerDialog::cancelRun()
    {
        if (_proc && _proc->state() != QProcess::NotRunning) {
            _userCancelled = true;
            setStatus("Stopping…", Status::Running);
            _proc->kill();
        }
    }

    void ToolRunnerDialog::onReadyRead()
    {
        if (!_proc) return;
        const QString chunk = QString::fromUtf8(_proc->readAll());
        // Preserve the tool's own line breaks; trim only the trailing newline so
        // appendPlainText doesn't add a blank line each read.
        _log->moveCursor(QTextCursor::End);
        _log->insertPlainText(chunk);
        _log->moveCursor(QTextCursor::End);
    }

    void ToolRunnerDialog::onErrorOccurred(QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart) {
            setStatus("Failed to start " + toolName() + ". Check the tool path in Preferences.",
                      Status::Error);
            setRunning(false);
        }
    }

    void ToolRunnerDialog::onFinished(int exitCode, QProcess::ExitStatus status)
    {
        onReadyRead();   // drain any tail output
        if (_proc) { _proc->deleteLater(); _proc = nullptr; }

        if (_userCancelled) {
            setRunning(false);
            setStatus("Stopped.", Status::Idle);
            return;
        }
        if (status == QProcess::CrashExit) {
            setRunning(false);
            setStatus(toolName() + " terminated unexpectedly.", Status::Error);
            QMessageBox::warning(this, toolName() + " failed",
                toolName() + " terminated unexpectedly. See the output for details.");
            return;
        }
        if (exitCode != 0) {
            setRunning(false);
            setStatus(toolName() + " failed (exit code " +
                      QString::number(exitCode) + "). See the output above.", Status::Error);
            QMessageBox::warning(this, toolName() + " failed",
                toolName() + " failed (exit code " + QString::number(exitCode) +
                "). See the output for details.");
            return;
        }
        // This step succeeded — advance to the next (or finish).
        ++_stepIndex;
        runNextStep();
    }

    void ToolRunnerDialog::setStatus(const QString& text, Status status)
    {
        const bool running = (status == Status::Running);
        _progress->setVisible(running);
        // range (0,0) makes the bar an indeterminate busy animation.
        _progress->setRange(0, running ? 0 : 1);

        QString prefix, color;
        switch (status) {
        case Status::Success: prefix = "✓  "; color = "#119E66"; break;  // brand green
        case Status::Error:   prefix = "✕  "; color = "#C0392B"; break;  // production red
        case Status::Running:
        case Status::Idle:
            color = palette().color(QPalette::WindowText).name();
            break;
        }
        const bool bold = (status == Status::Success || status == Status::Error);
        _status->setStyleSheet(
            QString("QLabel { font-size: 14px; font-weight: %1; color: %2; }")
                .arg(bold ? "700" : "500", color));
        _status->setText(prefix + text);
    }

    void ToolRunnerDialog::setRunning(bool running)
    {
        _runButton->setEnabled(!running);
        _cancelButton->setEnabled(running);
        // Disable the form inputs while running so the spec can't change mid-run.
        for (int i = 0; i < _formLayout->count(); ++i) {
            if (QWidget* w = _formLayout->itemAt(i)->widget())
                w->setEnabled(!running);
        }
    }
}
