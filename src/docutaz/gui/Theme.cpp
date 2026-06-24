#include "docutaz/gui/Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{
    namespace Theme
    {
        namespace
        {
            const Tokens kLight = {
                /*window*/        QColor("#F4F5F7"),
                /*base*/          QColor("#FFFFFF"),
                /*alternateBase*/ QColor("#F3F5F7"),
                /*mid*/           QColor("#D9DDE2"),
                /*hover*/         QColor("#ECEFF2"),
                /*text*/          QColor("#1F2328"),
                /*muted*/         QColor("#6B7280"),
                /*editorCanvas*/  QColor("#FCFCFD"),
                /*lineNumber*/    QColor("#9AA2AC"),
                /*synKeyword*/    QColor("#0B66C2"),
                /*synString*/     QColor("#0A7D3F"),
                /*synNumber*/     QColor("#9A4A00"),
                /*synOperator*/   QColor("#A21561"),
                /*synComment*/    QColor("#8A93A0"),
                /*synPunct*/      QColor("#3A414B"),
                /*highlight*/     QColor("#119E66"),
                /*highlightedText*/ QColor("#FFFFFF"),
                /*link*/          QColor("#0C7A50"),
                /*danger*/        QColor("#D33A2C"),
                /*dangerPress*/   QColor("#B82E22"),
                /*dangerSoft*/    QColor("#FCEDEB"),
            };

            const Tokens kDark = {
                /*window*/        QColor("#1E2227"),
                /*base*/          QColor("#181B20"),
                /*alternateBase*/ QColor("#22272E"),
                /*mid*/           QColor("#30363D"),
                /*hover*/         QColor("#262C33"),
                /*text*/          QColor("#E6E8EB"),
                /*muted*/         QColor("#8B949E"),
                /*editorCanvas*/  QColor("#15181C"),
                /*lineNumber*/    QColor("#5A6470"),
                /*synKeyword*/    QColor("#C678DD"),
                /*synString*/     QColor("#98C379"),
                /*synNumber*/     QColor("#D19A66"),
                /*synOperator*/   QColor("#E5C07B"),
                /*synComment*/    QColor("#5C6370"),
                /*synPunct*/      QColor("#ABB2BF"),
                /*highlight*/     QColor("#119E66"),
                /*highlightedText*/ QColor("#FFFFFF"),
                /*link*/          QColor("#34C281"),  // lightened for legibility on dark
                /*danger*/        QColor("#E5534B"),
                /*dangerPress*/   QColor("#CF4339"),
                /*dangerSoft*/    QColor("#2A1A18"),
            };
        }

        const Tokens &tokens(bool dark)
        {
            return dark ? kDark : kLight;
        }

        bool isDark()
        {
            return QtUtils::isDarkMode();
        }

        const Tokens &current()
        {
            return tokens(isDark());
        }

        QPalette buildPalette(bool dark)
        {
            const Tokens &t = tokens(dark);
            QPalette p;

            p.setColor(QPalette::Window,          t.window);
            p.setColor(QPalette::WindowText,       t.text);
            p.setColor(QPalette::Base,             t.base);
            p.setColor(QPalette::AlternateBase,    t.alternateBase);
            p.setColor(QPalette::Text,             t.text);
            p.setColor(QPalette::Button,           t.window);
            p.setColor(QPalette::ButtonText,       t.text);
            p.setColor(QPalette::BrightText,       dark ? QColor("#FFFFFF") : QColor("#000000"));
            p.setColor(QPalette::ToolTipBase,      t.base);
            p.setColor(QPalette::ToolTipText,      t.text);
            p.setColor(QPalette::PlaceholderText,  t.muted);
            p.setColor(QPalette::Highlight,        t.highlight);
            p.setColor(QPalette::HighlightedText,  t.highlightedText);
            p.setColor(QPalette::Link,             t.link);
            p.setColor(QPalette::LinkVisited,      t.link.darker(115));

            // Frame/3D roles flattened toward the mid tone so Fusion draws thin,
            // even borders instead of bevels.
            p.setColor(QPalette::Mid,              t.mid);
            p.setColor(QPalette::Midlight,         t.mid.lighter(108));
            p.setColor(QPalette::Dark,             t.mid.darker(115));
            p.setColor(QPalette::Light,            t.base);
            p.setColor(QPalette::Shadow,           dark ? QColor("#000000") : t.mid.darker(130));

            // Disabled group: muted text, no selection contrast.
            p.setColor(QPalette::Disabled, QPalette::Text,       t.muted);
            p.setColor(QPalette::Disabled, QPalette::WindowText, t.muted);
            p.setColor(QPalette::Disabled, QPalette::ButtonText, t.muted);
            p.setColor(QPalette::Disabled, QPalette::Highlight,  t.mid);
            p.setColor(QPalette::Disabled, QPalette::HighlightedText, t.muted);

            return p;
        }

        void apply()
        {
            if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
                QApplication::setStyle(fusion);
            QApplication::setPalette(buildPalette(isDark()));
        }
    }
}
