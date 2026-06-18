#include "docutaz/gui/AppStyle.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"

namespace Docutaz
{
    const QString AppStyle::StyleName = "Native";

    namespace AppStyleUtils
    {
        void applyStyle(const QString &styleName)
        {
            if (styleName == "Native") {
                QApplication::setStyle(new AppStyle);
                return;
            }

            QApplication::setStyle(QStyleFactory::create(styleName));
        }

        QStringList getSupportedStyles()
        {
            static QStringList result = QStringList() << AppStyle::StyleName << QStyleFactory::keys();
            return result;
        }

        void initStyle()
        {
            AppRegistry::instance().settingsManager()->save();
            QString style = AppRegistry::instance().settingsManager()->currentStyle();
            applyStyle(style);
        }

        void applyBrandAccent()
        {
            // Docutaz accent green for the selection highlight; a slightly darker
            // green (matching the website's link colour) for hyperlinks so they
            // stay legible on a light background. Everything else is left to the
            // native OS palette, so light/dark themes keep working.
            const QColor highlight(17, 158, 102);   // #119E66
            const QColor link(12, 122, 80);         // #0C7A50

            QPalette pal = QApplication::palette();
            pal.setColor(QPalette::Highlight, highlight);
            pal.setColor(QPalette::HighlightedText, Qt::white);
            pal.setColor(QPalette::Link, link);
            pal.setColor(QPalette::LinkVisited, link.darker(115));
            QApplication::setPalette(pal);
        }
    }

    void AppStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
        return QProxyStyle::drawControl(element, option, painter, widget);
    }

    void AppStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
#ifdef Q_OS_WIN
        if (element == QStyle::PE_FrameFocusRect)
            return;
#endif

        return QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    QRect AppStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget /*= 0 */) const
    {
        return QProxyStyle::subElementRect(element, option, widget);
    }
}
