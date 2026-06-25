#include "docutaz/gui/widgets/workarea/OutputWidget.h"

#include <QHBoxLayout>
#include <QSplitter>
#include <QWidget>
#include <QMouseEvent>
#include <QPalette>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/domain/MongoShell.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/utils/QtUtils.h"

#include "docutaz/gui/Theme.h"
#include "docutaz/gui/widgets/workarea/OutputItemContentWidget.h"
#include "docutaz/gui/widgets/workarea/ProgressBarPopup.h"
#include "docutaz/gui/widgets/workarea/WorkAreaTabBar.h"

namespace Docutaz
{
    OutputWidget::OutputWidget(QWidget *parent) :
        QTabWidget(parent), _splitter(new QSplitter), _tabbedResults(false)
    {
        _splitter->setOrientation(Qt::Vertical);
        _splitter->setHandleWidth(1);
        _splitter->setContentsMargins(0, 0, 0, 0);

        QVBoxLayout *layout = new QVBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(_splitter);
        setLayout(layout);

        setTabsClosable(true);
        setElideMode(Qt::ElideRight);
        setMovable(true);
#ifdef __APPLE__      
        setDocumentMode(false);
#else        
        setDocumentMode(true);
#endif        
        setStyleSheet(buildStyleSheet());
        // Regenerate the themed stylesheet on a live colour-scheme change.
        connect(Theme::Notifier::instance(), &Theme::Notifier::changed,
                this, [this] { setStyleSheet(buildStyleSheet()); });
        VERIFY(connect(this, SIGNAL(tabCloseRequested(int)), SLOT(tabCloseRequested(int))));
        
        _progressBarPopup = new ProgressBarPopup(this);
    }

    void OutputWidget::present(MongoShell *shell, const std::vector<MongoShellResult> &results)
    {
        if (_prevResultsCount > 0)
            clearAllParts();
        
        int const RESULTS_SIZE = _prevResultsCount = results.size();
        bool const multipleResults = (RESULTS_SIZE > 1);
        _tabbedResults = (RESULTS_SIZE > 2);
        _splitter->setHidden(_tabbedResults ? true : false);
        _outputItemContentWidgets.clear();        

        while (count() > 0)
            removeTab(count()-1);

        for (int i = 0; i < RESULTS_SIZE; ++i) {
            MongoShellResult shellResult = results[i];
            double secs = shellResult.elapsedMs() / 1000.f;
            ViewMode viewMode = AppRegistry::instance().settingsManager()->viewMode();
            if (_prevViewModes.size()) {
                viewMode = _prevViewModes.back();
                _prevViewModes.pop_back();
            }

            // Collection stats has a dedicated custom renderer; open it there.
            if (shellResult.type() == "collectionStats")
                viewMode = Custom;

            bool const firstItem = (0 == i);
            bool const lastItem = (RESULTS_SIZE-1 == i);

            OutputItemContentWidget* item = nullptr;
            if (shellResult.documents().size() > 0) {
                item = new OutputItemContentWidget(viewMode, shell, QtUtils::toQString(shellResult.type()),
                                                   shellResult.documents(), shellResult.queryInfo(), secs, 
                                                   multipleResults, _tabbedResults, firstItem, lastItem,
                                                   shellResult.aggrInfo(), this);
            } else {
                item = new OutputItemContentWidget(viewMode, shell, QtUtils::toQString(shellResult.response()), 
                                                   secs, multipleResults, _tabbedResults, firstItem, lastItem,
                                                   shellResult.aggrInfo(), this);
            }
            VERIFY(connect(item, SIGNAL(maximizedPart()), this, SLOT(maximizePart())));
            VERIFY(connect(item, SIGNAL(restoredSize()), this, SLOT(restoreSize())));

            if (_tabbedResults) {
                addTab(item, QString::fromStdString(shellResult.statementShort()));
                setTabToolTip(i, QString::fromStdString(shellResult.statement()));
            }
            else
                _splitter->addWidget(item);
             
            _outputItemContentWidgets.push_back(item);
        }
        
        tryToMakeAllPartsEqualInSize();
    }

    void OutputWidget::updatePart(int partIndex, const MongoQueryInfo &queryInfo, 
                                  const std::vector<MongoDocumentPtr> &documents)
    {
        if (!_tabbedResults && partIndex >= _splitter->count())
            return;

        OutputItemContentWidget* outputItemContentWidget = nullptr;
        if(_tabbedResults)
            outputItemContentWidget = qobject_cast<OutputItemContentWidget*>(currentWidget());
        else
            outputItemContentWidget = qobject_cast<OutputItemContentWidget*>(_splitter->widget(partIndex));
        
        outputItemContentWidget->updateWithInfo(queryInfo, documents);
        outputItemContentWidget->refreshOutputItem();
    }

    void OutputWidget::updatePart(int partIndex, const AggrInfo &agrrInfo, 
                                  const std::vector<MongoDocumentPtr> &documents)
    {
        if (partIndex >= _splitter->count())
            return;

        auto outputItemContentWidget = qobject_cast<OutputItemContentWidget*>(_splitter->widget(partIndex));
        outputItemContentWidget->updateWithInfo(agrrInfo, documents);
        outputItemContentWidget->refreshOutputItem();
    }

    void OutputWidget::toggleOrientation()
    {
        bool const horizontal = _splitter->orientation() == Qt::Horizontal;
        _splitter->setOrientation(horizontal ? Qt::Vertical : Qt::Horizontal);
        int const COUNT = _splitter->count();
        if (COUNT > 1) {
            auto const* firstItem = qobject_cast<OutputItemContentWidget*>(_splitter->widget(0));
            auto const* lastItem = qobject_cast<OutputItemContentWidget*>(_splitter->widget(COUNT-1));
            firstItem->toggleOrientation(_splitter->orientation());
            lastItem->toggleOrientation(_splitter->orientation());
        }
    }

    void OutputWidget::switchMode(
        std::function<void(OutputItemContentWidget*)> modeFunc
    )
    {
        if (_tabbedResults) {
            QWidget* currentTab { widget(currentIndex()) };
            modeFunc(qobject_cast<OutputItemContentWidget*>(currentTab));
        }
        else {
            for (int i = 0; i < _splitter->count(); i++) {
                QWidget* widget { _splitter->widget(i) };
                modeFunc(qobject_cast<OutputItemContentWidget*>(widget));
            }
        }
    }

    void OutputWidget::enterTreeMode() 
    {
        switchMode(&OutputItemContentWidget::showTree);
    }

    void OutputWidget::enterTextMode() 
    {
        switchMode(&OutputItemContentWidget::showText);
    }

    void OutputWidget::enterTableMode()
    {
        switchMode(&OutputItemContentWidget::showTable);
    }

    void OutputWidget::enterCustomMode()
    {
        switchMode(&OutputItemContentWidget::showCustom);
    }

    void OutputWidget::maximizePart()
    {
        OutputItemContentWidget *result = qobject_cast<OutputItemContentWidget *>(sender());
        int count = _splitter->count();
        for (int i = 0; i < count; i++) {
            OutputItemContentWidget *widget = (OutputItemContentWidget *) _splitter->widget(i);

            if (widget != result)
                widget->hide();
        }
    }

    void OutputWidget::tabCloseRequested(int index)
    {
        removeTab(index);
    }

    void OutputWidget::restoreSize()
    {
        int count = _splitter->count();
        for (int i = 0; i < count; i++) {
            OutputItemContentWidget *widget = (OutputItemContentWidget *) _splitter->widget(i);
            widget->show();
        }
    }

    int OutputWidget::resultIndex(OutputItemContentWidget *result)
    {
        return _splitter->indexOf(result);
    }

    void OutputWidget::showProgress()
    {
        QSize siz = size();
        QPoint point(siz.width() / 2 - ProgressBarPopup::width/2, siz.height() / 2 - ProgressBarPopup::height/2);
        _progressBarPopup->move(point);
        _progressBarPopup->show();
    }

    void OutputWidget::hideProgress()
    {
        _progressBarPopup->hide();
    }

    bool OutputWidget::progressBarActive() const 
    {
        return _progressBarPopup->isVisible();
    }

    void OutputWidget::applyDockUndockSettings(bool isDocking) const
    {
        for (auto const& item : _outputItemContentWidgets) {
            item->applyDockUndockSettings(isDocking);
        }
    }

    Qt::Orientation OutputWidget::getOrientation() const
    {
        return _splitter->orientation();
    }

    void OutputWidget::mouseReleaseEvent(QMouseEvent * event)
    {
        if (event->button() != Qt::MiddleButton)
            return;

        int const tabIndex = tabBar()->tabAt(event->pos());
        removeTab(tabIndex);
        QTabWidget::mouseReleaseEvent(event);
    }

    void OutputWidget::clearAllParts()
    {
        _prevViewModes.clear();
        while (_splitter->count() > 0) {
            OutputItemContentWidget *widget =  (OutputItemContentWidget *)_splitter->widget(_splitter->count()-1);
            _prevViewModes.push_back(widget->viewMode());
            widget->hide();
            delete widget;
        }
    }

    QString OutputWidget::buildStyleSheet()
    {
        // Flat, theme-driven results tabs that mirror WorkAreaTabBar: a window
        // strip with the selected tab raised to the canvas and a brand-green top
        // marker. One path for light and dark.
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
            "QTabWidget::pane { background-color: %5; border: none; }"
            "QTabBar::close-button { image: url(%8); width: 12px; height: 12px; }"
            "QTabBar::close-button:hover { image: url(%9); }"
            "QTabBar::tab {"
                "color: %2;"
                "font-size: 11px;"
                "background: %1;"
                "border: none;"
                "border-top: 2px solid transparent;"
                "border-right: 1px solid %6;"
                "padding: 5px 8px 6px 10px;"
            "}"
            "QTabBar::tab:hover { background: %3; color: %4; }"
            "QTabBar::tab:selected {"
                "color: %4;"
                "background: %5;"
                "border-top: 2px solid %7;"
            "}"
        ).arg(window, muted, hover, text, base, border, accent, closeIcon, closeHover);
    }

    void OutputWidget::tryToMakeAllPartsEqualInSize()
    {
        int resultsCount = _splitter->count();

        if (resultsCount <= 1)
            return;

        int dimension = _splitter->orientation() == Qt::Vertical ? _splitter->height() : _splitter->width();
        int step = dimension / resultsCount;

        QList<int> partSizes;
        for (int i = 0; i < resultsCount; ++i) {
            partSizes << step;
        }

        _splitter->setSizes(partSizes);
    }
}
