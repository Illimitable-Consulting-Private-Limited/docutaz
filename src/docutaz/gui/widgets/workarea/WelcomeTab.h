#pragma once

#include <QPixmap>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QScrollArea;
QT_END_NAMESPACE

namespace Docutaz
{
    class WelcomeTab : public QWidget
    {
        Q_OBJECT
    public:
        explicit WelcomeTab(QScrollArea* parent = nullptr);
        QScrollArea* getParent() const { return _parent; }
        void resize();

    private:
        QScrollArea* _parent;
        QLabel*      _logo;
        QPixmap      _logoPx;
    };
}
