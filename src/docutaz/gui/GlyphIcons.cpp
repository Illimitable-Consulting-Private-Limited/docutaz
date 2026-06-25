#include "docutaz/gui/GlyphIcons.h"

#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QGuiApplication>

#include "docutaz/gui/Theme.h"

namespace Docutaz
{
    namespace GlyphIcons
    {
        namespace
        {
            // Inner SVG body for each glyph (wrapped in a 24x24 <svg> at build
            // time). 21 are the approved mockup glyphs; the rest are authored in
            // the same flat style (currentColor, ~1.6px strokes). currentColor is
            // substituted with the tint when the icon is rendered.
            const QHash<QString, QByteArray> &glyphTable()
            {
                static const QHash<QString, QByteArray> table = {
                    // ---- toolbar / actions (from mockup) ----
                    {"connect", R"SVG(<path d="M4 7a1 1 0 0 1 1-1h14a1 1 0 0 1 1 1v3H4V7Z" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M4 12h16v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1v-5Z" fill="none" stroke="currentColor" stroke-width="1.6"/><circle cx="7.5" cy="8" r=".9" fill="currentColor"/><circle cx="7.5" cy="14.5" r=".9" fill="currentColor"/>)SVG"},
                    {"open", R"SVG(<path d="M3 7.5A1.5 1.5 0 0 1 4.5 6h4l2 2H19a1.5 1.5 0 0 1 1.5 1.5v8A1.5 1.5 0 0 1 19 19H4.5A1.5 1.5 0 0 1 3 17.5v-10Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/>)SVG"},
                    {"save", R"SVG(<path d="M5 4h11l3 3v13H5V4Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/><path d="M8 4v5h7V4" fill="none" stroke="currentColor" stroke-width="1.6"/><rect x="8" y="13" width="8" height="5" fill="none" stroke="currentColor" stroke-width="1.6"/>)SVG"},
                    {"run", R"SVG(<path d="M7 5.5v13l11-6.5L7 5.5Z" fill="currentColor"/>)SVG"},
                    {"stop", R"SVG(<rect x="6.5" y="6.5" width="11" height="11" rx="1.5" fill="currentColor"/>)SVG"},
                    {"refresh", R"SVG(<path d="M19 12a7 7 0 1 1-2.05-4.95M19 4.5V8h-3.5" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"caret-d", R"SVG(<path d="M5 8l5 5 5-5" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" transform="translate(2,2)"/>)SVG"},
                    {"chev-r", R"SVG(<path d="M9 6l6 6-6 6" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"chev-d", R"SVG(<path d="M6 9l6 6 6-6" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"chev-l", R"SVG(<path d="M15 6l-6 6 6 6" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"server", R"SVG(<rect x="4" y="5" width="16" height="6" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.6"/><rect x="4" y="13" width="16" height="6" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.6"/><circle cx="7.5" cy="8" r="1" fill="currentColor"/><circle cx="7.5" cy="16" r="1" fill="currentColor"/>)SVG"},
                    {"db", R"SVG(<ellipse cx="12" cy="6" rx="7" ry="2.6" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M5 6v12c0 1.4 3.1 2.6 7 2.6s7-1.2 7-2.6V6" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M5 12c0 1.4 3.1 2.6 7 2.6s7-1.2 7-2.6" fill="none" stroke="currentColor" stroke-width="1.6"/>)SVG"},
                    {"coll", R"SVG(<rect x="4" y="4" width="7" height="7" rx="1.3" fill="none" stroke="currentColor" stroke-width="1.6"/><rect x="13" y="4" width="7" height="7" rx="1.3" fill="none" stroke="currentColor" stroke-width="1.6"/><rect x="4" y="13" width="7" height="7" rx="1.3" fill="none" stroke="currentColor" stroke-width="1.6"/><rect x="13" y="13" width="7" height="7" rx="1.3" fill="none" stroke="currentColor" stroke-width="1.6"/>)SVG"},
                    {"folder", R"SVG(<path d="M3 7.5A1.5 1.5 0 0 1 4.5 6h4l2 2H19a1.5 1.5 0 0 1 1.5 1.5v8A1.5 1.5 0 0 1 19 19H4.5A1.5 1.5 0 0 1 3 17.5v-10Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/>)SVG"},
                    {"doc", R"SVG(<path d="M6 3h8l4 4v14H6V3Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/><path d="M14 3v4h4" fill="none" stroke="currentColor" stroke-width="1.6"/>)SVG"},
                    {"x", R"SVG(<path d="M6 6l12 12M18 6L6 18" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/>)SVG"},
                    {"warn", R"SVG(<path d="M12 4 2.5 20h19L12 4Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/><path d="M12 10v4.5" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/><circle cx="12" cy="17.4" r="1.05" fill="currentColor"/>)SVG"},
                    {"copy", R"SVG(<rect x="8" y="8" width="11" height="12" rx="1.6" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M5 16V5.6A1.6 1.6 0 0 1 6.6 4H15" fill="none" stroke="currentColor" stroke-width="1.6"/>)SVG"},
                    {"export", R"SVG(<path d="M12 3v11M12 3 8 7M12 3l4 4" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/><path d="M5 14v5h14v-5" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"import", R"SVG(<path d="M12 14V3M12 14l-4-4M12 14l4-4" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/><path d="M5 15v4h14v-4" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"undock", R"SVG(<path d="M18 13v5a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h5" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/><path d="M14 4h6v6M20 4l-9 9" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"dock", R"SVG(<rect x="4" y="6" width="16" height="13" rx="1.6" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M4 10h16" fill="none" stroke="currentColor" stroke-width="1.6"/>)SVG"},
                    {"search", R"SVG(<circle cx="11" cy="11" r="6" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M16 16l4 4" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/>)SVG"},
                    {"history", R"SVG(<path d="M12 7v5l3.5 2" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/><path d="M4.5 12a7.5 7.5 0 1 1 2.2 5.3M4.5 12H8M4.5 12V8.5" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},

                    // ---- explorer extras ----
                    {"index", R"SVG(<path d="M4 6h16M4 12h16M4 18h10" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/>)SVG"},
                    {"user", R"SVG(<circle cx="12" cy="8" r="3.5" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M5 19a7 7 0 0 1 14 0" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/>)SVG"},
                    {"function", R"SVG(<path d="M14 5c-2 0-3 1.2-3.3 3L8.5 19" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/><path d="M7 9h6" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/><path d="M13.5 11l4.5 6M18 11l-4.5 6" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/>)SVG"},
                    {"key", R"SVG(<circle cx="8" cy="12" r="4" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M11.5 12H20M17 12v3M20 12v4" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},

                    // ---- result view modes ----
                    {"view-text", R"SVG(<path d="M5 7h14M5 11h14M5 15h10M5 19h6" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/>)SVG"},
                    {"view-tree", R"SVG(<circle cx="6" cy="6" r="2" fill="none" stroke="currentColor" stroke-width="1.5"/><path d="M6 8v8M6 12h5M6 16h5" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/><circle cx="13.5" cy="12" r="2" fill="none" stroke="currentColor" stroke-width="1.5"/><circle cx="13.5" cy="16" r="2" fill="none" stroke="currentColor" stroke-width="1.5"/>)SVG"},
                    {"view-table", R"SVG(<rect x="4" y="5" width="16" height="14" rx="1.4" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M4 10h16M4 14.5h16M9.5 5v14M14.5 5v14" fill="none" stroke="currentColor" stroke-width="1.3"/>)SVG"},
                    {"view-custom", R"SVG(<path d="M5 19V11M10 19V5M15 19v-6M20 19v-9" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/>)SVG"},

                    // ---- bson types ----
                    {"bson-object", R"SVG(<path d="M9 4c-2 0-3 1-3 3v2c0 1.2-.6 2-2 2 1.4 0 2 .8 2 2v3c0 2 1 3 3 3" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/><path d="M15 4c2 0 3 1 3 3v2c0 1.2.6 2 2 2-1.4 0-2 .8-2 2v3c0 2-1 3-3 3" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"bson-array", R"SVG(<path d="M9 4H6v16h3M15 4h3v16h-3" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"bson-string", R"SVG(<path d="M7.5 7c-1.4 0-2.5 1.1-2.5 2.5 0 1.2.9 2.2 2 2.4-.2 1.6-1.2 2.6-2.6 3.1M16.5 7c-1.4 0-2.5 1.1-2.5 2.5 0 1.2.9 2.2 2 2.4-.2 1.6-1.2 2.6-2.6 3.1" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"bson-number", R"SVG(<path d="M9 4 7 20M17 4l-2 16M5 9h14M4 15h14" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/>)SVG"},
                    {"bson-datetime", R"SVG(<circle cx="12" cy="12" r="7.5" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M12 8v4.5l3 2" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"bson-binary", R"SVG(<rect x="6" y="6" width="4.5" height="12" rx="2.2" fill="none" stroke="currentColor" stroke-width="1.5"/><path d="M16 6v12M13.5 6H16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"bson-null", R"SVG(<circle cx="12" cy="12" r="7.5" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M7 7l10 10" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/>)SVG"},
                    {"bson-bool", R"SVG(<rect x="3.5" y="8" width="17" height="8" rx="4" fill="none" stroke="currentColor" stroke-width="1.6"/><circle cx="15.5" cy="12" r="2.4" fill="currentColor"/>)SVG"},

                    // ---- marks / misc ----
                    {"check", R"SVG(<path d="M5 12.5l4.5 4.5L19 7" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"dash", R"SVG(<path d="M6 12h12" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>)SVG"},
                    {"question", R"SVG(<path d="M9 9a3 3 0 1 1 4.5 2.6c-1 .6-1.5 1.2-1.5 2.4M12 18h.01" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"info", R"SVG(<circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M12 11v5.5" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/><circle cx="12" cy="7.7" r="1.15" fill="currentColor"/>)SVG"},
                    {"trash", R"SVG(<path d="M5 7h14M10 7V5h4v2M6 7l1 13h10l1-13" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/><path d="M10 11v6M14 11v6" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"/>)SVG"},
                    {"edit", R"SVG(<path d="M5 19h14" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/><path d="M15 5l4 4-9.5 9.5H5.5V14.5L15 5Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/>)SVG"},
                    {"maximize", R"SVG(<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"minimize", R"SVG(<path d="M10 4v6H4M14 4v6h6M10 20v-6H4M14 20v-6h6" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                    {"circle", R"SVG(<circle cx="12" cy="12" r="6" fill="none" stroke="currentColor" stroke-width="1.6"/>)SVG"},
                    {"plus", R"SVG(<path d="M12 5v14M5 12h14" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/>)SVG"},
                    {"minus", R"SVG(<path d="M5 12h14" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/>)SVG"},
                    {"eye", R"SVG(<path d="M2.5 12S6 5.5 12 5.5 21.5 12 21.5 12 18 18.5 12 18.5 2.5 12 2.5 12Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/><circle cx="12" cy="12" r="2.6" fill="none" stroke="currentColor" stroke-width="1.6"/>)SVG"},
                    {"eye-off", R"SVG(<path d="M4 4l16 16" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/><path d="M9.5 5.8C10.3 5.6 11.1 5.5 12 5.5c6 0 9.5 6.5 9.5 6.5a16 16 0 0 1-3.2 3.8M6.2 7.6A16 16 0 0 0 2.5 12S6 18.5 12 18.5c1 0 1.9-.1 2.7-.4" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>)SVG"},
                };
                return table;
            }

            QByteArray buildSvg(const QByteArray &body, const QColor &color)
            {
                QByteArray svg =
                    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\">";
                svg += body;
                svg += "</svg>";
                svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
                return svg;
            }

            QColor resolveTint(Tint tint)
            {
                const Theme::Tokens &t = Theme::current();
                switch (tint)
                {
                case Tint::Muted:     return t.muted;
                case Tint::Link:      return t.link;
                case Tint::Danger:    return t.danger;
                case Tint::Highlight: return t.highlight;
                case Tint::Text:
                default:              return t.text;
                }
            }

            // QIconEngine that renders the glyph SVG fresh at the requested size,
            // so it stays crisp at any DPI. The normal colour is either a fixed
            // tint or a semantic role resolved from the live theme at paint time;
            // Selected mode uses the highlighted-text colour (or a fixed
            // override) and Disabled mode is drawn at reduced opacity.
            class GlyphIconEngine : public QIconEngine
            {
            public:
                // Theme-following: colour follows `tint` at paint time.
                GlyphIconEngine(QByteArray body, Tint tint)
                    : _body(std::move(body)), _useTint(true), _tint(tint)
                {
                }

                // Fixed colours: do not follow the theme.
                GlyphIconEngine(QByteArray body, QColor normal, QColor selected)
                    : _body(std::move(body)), _normal(normal), _selected(selected)
                {
                }

                void paint(QPainter *painter, const QRect &rect,
                           QIcon::Mode mode, QIcon::State /*state*/) override
                {
                    QColor normal = _useTint ? resolveTint(_tint) : _normal;
                    QColor selected = _useTint ? Theme::current().highlightedText : _selected;
                    const QColor c = (mode == QIcon::Selected) ? selected : normal;
                    QSvgRenderer renderer(buildSvg(_body, c));
                    painter->save();
                    if (mode == QIcon::Disabled)
                        painter->setOpacity(0.4);
                    painter->setRenderHint(QPainter::Antialiasing, true);
                    renderer.render(painter, QRectF(rect));
                    painter->restore();
                }

                QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                               QIcon::State state) override
                {
                    const qreal dpr = qApp->devicePixelRatio();
                    QPixmap pm(size * dpr);
                    pm.setDevicePixelRatio(dpr);
                    pm.fill(Qt::transparent);
                    QPainter p(&pm);
                    paint(&p, QRect(QPoint(0, 0), size), mode, state);
                    p.end();
                    return pm;
                }

                QIconEngine *clone() const override
                {
                    return _useTint ? new GlyphIconEngine(_body, _tint)
                                    : new GlyphIconEngine(_body, _normal, _selected);
                }

            private:
                QByteArray _body;
                bool _useTint = false;
                Tint _tint = Tint::Text;
                QColor _normal;
                QColor _selected;
            };
        }

        bool has(const QString &name)
        {
            return glyphTable().contains(name);
        }

        QIcon icon(const QString &name, Tint tint)
        {
            const auto it = glyphTable().constFind(name);
            if (it == glyphTable().constEnd())
                return QIcon();
            return QIcon(new GlyphIconEngine(it.value(), tint));
        }

        QIcon icon(const QString &name, const QColor &color, const QColor &selected)
        {
            const auto it = glyphTable().constFind(name);
            if (it == glyphTable().constEnd())
                return QIcon();
            return QIcon(new GlyphIconEngine(it.value(), color, selected));
        }
    }
}
