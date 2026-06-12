#pragma once

#include <QPixmap>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QScrollArea;
class QResizeEvent;
class QShowEvent;
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

    protected:
        // Apply the logo pixmap as soon as the tab is shown, and keep it scaled
        // on every resize — relying on the parent tab widget to forward a resize
        // while the tab is visible is timing-dependent and can leave the logo
        // blank on first show (see WorkAreaTabWidget::resizeEvent).
        void showEvent(QShowEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        QScrollArea* _parent;
        QLabel*      _logo;
        QPixmap      _logoPx;
    };
}
