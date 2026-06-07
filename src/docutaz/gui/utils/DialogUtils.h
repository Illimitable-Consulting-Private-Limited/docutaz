#pragma once

#include <QMessageBox>

namespace Docutaz
{
    namespace utils
    {
        int questionDialog(QWidget *parent, const QString &actionText, const QString &itemText, const QString &valueText);

        int questionDialog(QWidget *parent, const QString &actionText, const QString &itemText, const QString &templateText, const QString &valueText);
    }
}

