#pragma once

#include <QDialog>
#include <QProcess>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QDialogButtonBox;
class QProgressBar;
QT_END_NAMESPACE

namespace Docutaz
{
    // Base for the backup/restore dialogs. Owns the shared machinery: a form area
    // the subclass fills, a live output log, a status line, and Run/Cancel/Close
    // buttons that drive a MongoDB Database Tool as an asynchronous QProcess (the
    // GUI stays responsive; Cancel kills the process). Subclasses only supply the
    // tool to run, its arguments, input validation and an optional confirmation.
    class ToolRunnerDialog : public QDialog
    {
        Q_OBJECT

    public:
        explicit ToolRunnerDialog(QWidget* parent = nullptr);

    protected:
        // The subclass adds its input widgets here (above the log/buttons).
        QVBoxLayout* formLayout() const { return _formLayout; }
        void setRunButtonText(const QString& text);

        // --- Subclass contract ------------------------------------------------
        // Absolute path to the tool executable, or empty if it can't be located.
        virtual QString toolPath() const = 0;
        // Human name for messages, e.g. "mongodump".
        virtual QString toolName() const = 0;
        // Validate inputs; return false and set `error` to abort the run.
        virtual bool validate(QString& error) const = 0;
        // Extra confirmation before running (e.g. the production write-guard on
        // restore/import). Return false to abort. Default: proceed.
        virtual bool confirm() { return true; }
        // One or more argument vectors, each run with toolPath() in sequence
        // (mongodump selects one database per invocation, so a multi-database
        // backup is several runs). Stops on the first non-zero exit.
        virtual QList<QStringList> argumentSets() const = 0;
        // Called after all steps succeed — e.g. to report the output path.
        virtual void onSucceeded() {}
        // Title/body for the prominent success dialog shown when every step
        // finishes. Defaults suit a generic run; subclasses tailor them (e.g. the
        // backup output path or the number of documents restored).
        virtual QString successTitle() const { return "Done"; }
        virtual QString successMessage() const { return "The operation completed successfully."; }
        // Shown when the tool can't be located, instead of a spawn failure.
        virtual QString notFoundMessage() const = 0;

        void appendLog(const QString& text);

    protected Q_SLOTS:
        void start();
        void cancelRun();

    private Q_SLOTS:
        void onReadyRead();
        void onFinished(int exitCode, QProcess::ExitStatus status);
        void onErrorOccurred(QProcess::ProcessError error);

    private:
        // Visual state of the status banner (drives colour + the busy spinner).
        enum class Status { Idle, Running, Success, Error };
        void setStatus(const QString& text, Status status);
        void setRunning(bool running);
        void runNextStep();

        QList<QStringList> _pending;
        int               _stepIndex   = 0;
        int               _stepTotal   = 0;

        QVBoxLayout*      _formLayout   = nullptr;
        QPlainTextEdit*   _log          = nullptr;
        QProgressBar*     _progress     = nullptr;
        QLabel*           _status       = nullptr;
        QPushButton*      _runButton    = nullptr;
        QPushButton*      _cancelButton = nullptr;
        QDialogButtonBox* _buttonBox    = nullptr;
        QProcess*         _proc         = nullptr;
        bool              _userCancelled = false;
    };
}
