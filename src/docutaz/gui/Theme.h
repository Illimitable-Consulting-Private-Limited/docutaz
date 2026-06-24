#pragma once

#include <QColor>

QT_BEGIN_NAMESPACE
class QPalette;
class QWidget;
QT_END_NAMESPACE

namespace Docutaz
{
    // Centralised design tokens + palette for the flat light/dark look. These
    // values are the agreed design language (see the UI design study); widgets
    // should read semantic tokens from here instead of hardcoding hex, and the
    // app palette is built from them so standard Qt widgets follow the theme.
    namespace Theme
    {
        struct Tokens
        {
            // surfaces / chrome
            QColor window, base, alternateBase, mid, hover;
            QColor text, muted;
            // editor (QScintilla canvas; rendered independently of QPalette)
            QColor editorCanvas, lineNumber;
            // editor syntax
            QColor synKeyword, synString, synNumber, synOperator, synComment, synPunct;
            // brand / accent
            QColor highlight, highlightedText, link;
            // destructive
            QColor danger, dangerPress, dangerSoft;
        };

        // Locked light/dark token tables.
        const Tokens &tokens(bool dark);

        // True when the UI should render dark (follows QtUtils::isDarkMode,
        // i.e. the OS colour scheme on Qt 6.5+, palette heuristic below).
        bool isDark();

        // Tokens for the currently active scheme.
        const Tokens &current();

        // A curated QPalette built from the tokens for the given scheme.
        QPalette buildPalette(bool dark);

        // Apply the base style (Fusion) and the scheme-selected palette to the
        // application. Safe to call again on a colour-scheme change.
        void apply();
    }
}
