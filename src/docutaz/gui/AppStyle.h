#pragma once

#include <QStyle>
#include <QProxyStyle>

namespace Docutaz
{
    namespace AppStyleUtils
    {
        void initStyle();
        void applyStyle(const QString &styleName);
        QStringList getSupportedStyles();
        // Tint the application palette with the Docutaz brand accent (selection
        // highlight + links) on top of whatever native light/dark palette the OS
        // provides. Call once after the style is initialised.
        void applyBrandAccent();
    }

    class AppStyle : public QProxyStyle
    {
        Q_OBJECT

    public:
        static const QString StyleName;
        virtual void drawControl(ControlElement element, const QStyleOption * option, QPainter * painter, const QWidget * widget) const;
        virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
        virtual QRect subElementRect( SubElement element, const QStyleOption * option, const QWidget * widget = 0 ) const;
    };
}
