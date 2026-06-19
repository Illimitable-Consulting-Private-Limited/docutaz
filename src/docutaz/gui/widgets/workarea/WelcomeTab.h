#pragma once

#include <QPixmap>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QScrollArea;
class QResizeEvent;
class QShowEvent;
class QFrame;
QT_END_NAMESPACE

namespace Docutaz
{
    class MongoshSettingsChangedEvent;

    class WelcomeTab : public QWidget
    {
        Q_OBJECT
    public:
        explicit WelcomeTab(QScrollArea* parent = nullptr);
        QScrollArea* getParent() const { return _parent; }
        void resize();

    public Q_SLOTS:
        // Re-evaluate the card when the mongosh path is changed from anywhere
        // (e.g. the status-bar nudge), so it clears even while this tab is open.
        void handle(MongoshSettingsChangedEvent* event);

    protected:
        // Apply the logo pixmap as soon as the tab is shown, and keep it scaled
        // on every resize — relying on the parent tab widget to forward a resize
        // while the tab is visible is timing-dependent and can leave the logo
        // blank on first show (see WorkAreaTabWidget::resizeEvent).
        void showEvent(QShowEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        // Show/hide the "mongosh not detected" card based on current detection
        // (re-checked on show, so configuring a path elsewhere clears it).
        void refreshMongoshCard();

        QScrollArea* _parent;
        QLabel*      _logo;
        QPixmap      _logoPx;
        QFrame*      _mongoshCard = nullptr;
    };
}
