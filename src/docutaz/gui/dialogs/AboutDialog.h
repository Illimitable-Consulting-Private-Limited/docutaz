#pragma  once

#include <QDialog>

namespace Docutaz
{
    class AboutDialog : public QDialog
    {
        Q_OBJECT

    public:
        explicit AboutDialog(QWidget *parent);
    };
}
