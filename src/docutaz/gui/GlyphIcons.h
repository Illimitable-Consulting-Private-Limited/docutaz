#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace Docutaz
{
    // Monochrome glyph icons: crisp, theme-tinted SVG paths rendered at any size
    // and DPR. The glyph set is the locked UI-design language (24x24 viewBox,
    // 1.6px strokes, currentColor) — see the look-&-feel study. Widgets get an
    // icon by name; the colour comes from the theme so every icon follows the
    // active light/dark scheme. When an item is rendered selected (e.g. a chosen
    // explorer/tree row over the brand highlight) the engine swaps to the
    // selected colour automatically.
    namespace GlyphIcons
    {
        // Semantic tints resolved from Theme::current() at PAINT time, so an icon
        // built once (and cached, e.g. in GuiRegistry) follows live light/dark
        // colour-scheme changes without being rebuilt.
        enum class Tint
        {
            Text,       // primary foreground
            Muted,      // secondary / de-emphasised
            Link,       // info accent
            Danger,     // destructive accent
            Highlight,  // brand green
        };

        // Build a theme-following icon for the named glyph: its colour is the
        // resolved `tint` at paint time, and it swaps to the highlighted-text
        // colour when painted in the Selected mode. Returns a null icon if the
        // name is unknown.
        QIcon icon(const QString &name, Tint tint);

        // Build a fixed-colour icon for the named glyph (used where the colour is
        // intentionally not theme-driven, e.g. environment dots). `selected` is
        // used when Qt paints the icon in the Selected mode (defaults to white).
        QIcon icon(const QString &name, const QColor &color,
                   const QColor &selected = QColor(Qt::white));

        // True if a glyph with this name exists.
        bool has(const QString &name);
    }
}
