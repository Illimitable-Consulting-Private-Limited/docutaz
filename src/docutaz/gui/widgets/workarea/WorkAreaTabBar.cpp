#include "docutaz/gui/widgets/workarea/WorkAreaTabBar.h"

#include <QMouseEvent>
#include <QTabWidget>
#include <QScrollArea>
#include <QPalette>

#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/gui/Theme.h"

namespace Docutaz
{
    /**
     * @brief Creates WorkAreaTabBar, without parent widget. We are
     * assuming, that tab bar will be installed to (and owned by)
     * WorkAreaTabWidget, using QTabWidget::setTabBar().
     */
    WorkAreaTabBar::WorkAreaTabBar(QWidget *parent) 
        : QTabBar(parent)
    {
        setDrawBase(false);
        setStyleSheet(buildStyleSheet());
        // Rebuild the tab styling on a live colour-scheme change (the stylesheet
        // bakes in theme colours, so it must be regenerated).
        connect(Theme::Notifier::instance(), &Theme::Notifier::changed,
                this, [this] { setStyleSheet(buildStyleSheet()); });

        _menu = new QMenu(this);
        _newShellAction = new QAction("&New Shell", _menu);
        _newShellAction->setShortcut(QKeySequence(QKeySequence::AddTab));
        _reloadShellAction = new QAction("&Re-execute Query", _menu);
        _reloadShellAction->setShortcut(Qt::CTRL | Qt::Key_R);
        _duplicateShellAction = new QAction("&Duplicate Query In New Tab", _menu);
        _duplicateShellAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_T);
        _pinShellAction = new QAction("&Pin Shell", _menu);
        _closeShellAction = new QAction("&Close Shell", _menu);
        _closeShellAction->setShortcut(Qt::CTRL | Qt::Key_W);
        _closeOtherShellsAction = new QAction("Close &Other Shells", _menu);
        _closeShellsToTheRightAction = new QAction("Close Shells to the R&ight", _menu);

        _menu->addAction(_newShellAction);
        _menu->addSeparator();
        _menu->addAction(_reloadShellAction);
        _menu->addAction(_duplicateShellAction);
        _menu->addSeparator();
        _menu->addAction(_closeShellAction);
        _menu->addAction(_closeOtherShellsAction);
        _menu->addAction(_closeShellsToTheRightAction);
    }

    /**
     * @brief Overrides QTabBar::mouseReleaseEvent() in order to support
     * middle-mouse tab close and to implement tab context menu.
     */
    void WorkAreaTabBar::mouseReleaseEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::MiddleButton)
            middleMouseReleaseEvent(event);
        else if (event->button() == Qt::RightButton)
            rightMouseReleaseEvent(event);

        // always calling base event handler, even if we
        // were interested by this event
        QTabBar::mouseReleaseEvent(event);
    }

    void WorkAreaTabBar::mouseDoubleClickEvent(QMouseEvent *event)
    {
        int tabIndex = tabAt(event->pos());

        // if tab was double-clicked, ignore this action
        if (tabIndex >= 0)
            return;

        int currentTab = currentIndex();
        if (currentTab < 0)
            return;

        emit newTabRequested(currentTab);
        QTabBar::mouseDoubleClickEvent(event);
    }

    /**
     * @brief Handles middle-mouse release event in order to close tab.
     */
    void WorkAreaTabBar::middleMouseReleaseEvent(QMouseEvent *event)
    {
        int tabIndex = tabAt(event->pos());
        if (tabIndex < 0)
            return;

        emit tabCloseRequested(tabIndex);
    }

    /**
     * @brief Handles right-mouse release event to show tab context menu.
     */
    void WorkAreaTabBar::rightMouseReleaseEvent(QMouseEvent *event)
    {
        int tabIndex = tabAt(event->pos());
        if (tabIndex < 0)
            return;

        // If this is a Welcome tab, do not show right click menu. 
        // Note: Scroll area represents a WelcomeTab.
        auto tabWidget = qobject_cast<QTabWidget*>(parentWidget());
        if (qobject_cast<QScrollArea*>(tabWidget->widget(tabIndex)))
            return;

        QAction *selected = _menu->exec(QCursor::pos());
        if (!selected)
            return;

        emitSignalForContextMenuAction(tabIndex, selected);
    }

    /**
     * @brief Emits signal, based on specified action. Only actions
     * specified in this class are supported. If we don't know specified
     * action - no signal will be emited.
     * @param tabIndex: index of tab, for which signal will be emited.
     * @param action: context menu action.
     */
    void WorkAreaTabBar::emitSignalForContextMenuAction(int tabIndex, QAction *action)
    {
        if (action == _newShellAction)
            emit newTabRequested(tabIndex);
        else if (action == _reloadShellAction)
            emit reloadTabRequested(tabIndex);
        else if (action == _duplicateShellAction)
            emit duplicateTabRequested(tabIndex);
        else if (action == _pinShellAction)
            emit pinTabRequested(tabIndex);
        else if (action == _closeShellAction)
            emit tabCloseRequested(tabIndex);
        else if (action == _closeOtherShellsAction)
            emit closeOtherTabsRequested(tabIndex);
        else if (action == _closeShellsToTheRightAction)
            emit closeTabsToTheRightRequested(tabIndex);
    }

    /**
     * @brief Builds stylesheet for this WorkAreaTabBar widget.
     *
     * Flat, theme-driven tabs: a solid window-coloured strip, with the selected
     * tab raised to the editor canvas and marked by a 2px brand-green top edge.
     * No gradients or rounded corners; one path for both light and dark.
     */
    QString WorkAreaTabBar::buildStyleSheet()
    {
        const Theme::Tokens &t = Theme::current();
        const QString window = t.window.name();
        const QString base   = t.base.name();
        const QString hover  = t.hover.name();
        const QString text   = t.text.name();
        const QString muted  = t.muted.name();
        const QString border = t.mid.name();
        const QString accent = t.highlight.name();
        const bool dark = Theme::isDark();
        const QString closeIcon  = dark ? QStringLiteral(":/docutaz/icons/close_glyph_dark.svg")
                                        : QStringLiteral(":/docutaz/icons/close_glyph.svg");
        const QString closeHover = dark ? QStringLiteral(":/docutaz/icons/close_glyph_hover_dark.svg")
                                        : QStringLiteral(":/docutaz/icons/close_glyph_hover.svg");

        return QString(
            #ifndef __APPLE__
            "QTabBar::tab:first { margin-left: 4px; }  "
            "QTabBar::tab:last { margin-right: 1px; }  "
            #endif
            "QTabBar::close-button { image: url(%8); width: 12px; height: 12px; }"
            "QTabBar::close-button:hover { image: url(%9); }"
            "QTabBar::tab {"
                "color: %2;"                          // muted text, unselected
                "background: %1;"                     // window strip
                "border: none;"
                "border-top: 2px solid transparent;"  // reserve room for the marker
                "border-right: 1px solid %6;"         // hairline separator
                "padding: 5px 10px 6px 10px;"
                #ifndef __APPLE__
                "max-width: 200px;"
                "margin: 0px; margin-left: 1px;"
                #endif
            "}"
            "QTabBar::tab:hover { background: %3; color: %4; }"
            "QTabBar::tab:selected {"
                "color: %4;"                          // full text colour
                "background: %5;"                     // raised to the canvas
                "border-top: 2px solid %7;"           // brand-green active marker
            "}"
        ).arg(window, muted, hover, text, base, border, accent, closeIcon, closeHover);
    }
}
