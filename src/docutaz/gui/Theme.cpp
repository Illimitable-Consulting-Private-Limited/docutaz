#include "docutaz/gui/Theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/gui/GlyphIcons.h"

namespace Docutaz
{
    namespace Theme
    {
        // The UI font family actually loaded (set in apply()); enforced via the
        // global QSS because an app stylesheet otherwise ignores setFont().
        static QString g_uiFontFamily = QStringLiteral("Inter");

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

        const Tokens &tokens(bool dark);
        const Tokens &current();

        // Thin proxy over Fusion that (1) drops the platform icons standard dialog
        // buttons carry (red-circle Cancel, blue-disk Save, …) and (2) maps the
        // standard message-box / reload pixmaps to our monochrome glyphs — the
        // system theme renders SP_MessageBoxInformation as a lightbulb, which
        // clashes badly. This catches every QMessageBox and any code that pulls a
        // standardIcon, in one place.
        class FlatProxyStyle : public QProxyStyle
        {
        public:
            explicit FlatProxyStyle(QStyle *base) : QProxyStyle(base) {}

            int styleHint(StyleHint hint, const QStyleOption *option,
                          const QWidget *widget, QStyleHintReturn *ret) const override
            {
                if (hint == SH_DialogButtonBox_ButtonsHaveIcons)
                    return 0;
                return QProxyStyle::styleHint(hint, option, widget, ret);
            }

            QIcon standardIcon(StandardPixmap sp, const QStyleOption *option,
                               const QWidget *widget) const override
            {
                const Tokens &t = current();
                switch (sp)
                {
                case SP_MessageBoxInformation:
                    return GlyphIcons::icon("info", t.link);
                case SP_MessageBoxWarning:
                    return GlyphIcons::icon("warn", t.synNumber);
                case SP_MessageBoxCritical:
                    return GlyphIcons::icon("warn", t.danger);
                case SP_MessageBoxQuestion:
                    return GlyphIcons::icon("question", t.muted);
                case SP_BrowserReload:
                    return GlyphIcons::icon("refresh", t.text);
                default:
                    return QProxyStyle::standardIcon(sp, option, widget);
                }
            }
        };

        // Application-wide flat chrome: slim scrollbars, flat menus/menubar/
        // toolbars, flat header views, roomier item rows, and flat buttons/inputs
        // — all keyed off the design tokens so the whole app reads as one flat,
        // themed surface in both light and dark. QScintilla draws its own canvas
        // and scrollbars, so editors are unaffected.
        QString buildGlobalStyleSheet(const Tokens &t)
        {
            const QString window  = t.window.name();
            const QString base    = t.base.name();
            const QString altBase = t.alternateBase.name();
            const QString mid     = t.mid.name();
            const QString hover   = t.hover.name();
            const QString text    = t.text.name();
            const QString muted   = t.muted.name();
            const QString accent  = t.highlight.name();
            const QString onAccent = t.highlightedText.name();

            QString s;

            // --- base UI font (enforced here so the stylesheet doesn't fall back
            // to the default; scoped to chrome classes, not QScintilla which draws
            // its own monospace text via Scintilla styles) ---
            s += QString(
                "QWidget { font-family: \"%1\"; }"
            ).arg(g_uiFontFamily);

            // --- scrollbars ---
            s += QString(
                "QScrollBar:vertical { background: %1; width: 12px; margin: 0px; border: none; }"
                "QScrollBar::handle:vertical { background: %2; min-height: 24px; border-radius: 4px; margin: 2px; }"
                "QScrollBar::handle:vertical:hover { background: %3; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
                "QScrollBar:horizontal { background: %1; height: 12px; margin: 0px; border: none; }"
                "QScrollBar::handle:horizontal { background: %2; min-width: 24px; border-radius: 4px; margin: 2px; }"
                "QScrollBar::handle:horizontal:hover { background: %3; }"
                "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
                "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }"
            ).arg(window, mid, muted);

            // --- menu bar / menus ---
            s += QString(
                "QMenuBar { background: %1; color: %2; border: none; }"
                "QMenuBar::item { background: transparent; padding: 5px 10px; }"
                "QMenuBar::item:selected { background: %3; border-radius: 4px; }"
                "QMenuBar::item:pressed { background: %3; border-radius: 4px; }"
                "QMenu { background: %4; color: %2; border: 1px solid %5; padding: 5px; }"
                "QMenu::item { padding: 6px 28px 6px 30px; border-radius: 4px; }"
                "QMenu::item:selected { background: %6; color: %7; }"
                "QMenu::item:disabled { color: %8; }"
                "QMenu::separator { height: 1px; background: %5; margin: 5px 10px; }"
                "QMenu::icon { left: 8px; }"
                "QMenu::indicator { width: 16px; height: 16px; left: 8px; }"
            ).arg(window, text, hover, base, mid, accent, onAccent, muted);

            // --- toolbar ---
            s += QString(
                "QToolBar { background: %1; border: none; padding: 3px; spacing: 2px; }"
                "QToolBar::separator { width: 1px; background: %2; margin: 4px 4px; }"
                "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 4px; }"
                "QToolButton:hover { background: %3; }"
                "QToolButton:pressed, QToolButton:checked { background: %2; }"
                "QToolButton::menu-button { border: none; width: 14px; }"
            ).arg(window, mid, hover);

            // --- header views (result table / tree headers) ---
            s += QString(
                "QHeaderView { background: %1; }"
                "QHeaderView::section { background: %1; color: %2; padding: 5px 8px; border: none;"
                    " border-right: 1px solid %3; border-bottom: 1px solid %3; }"
                "QHeaderView::section:hover { background: %4; }"
                "QTableCornerButton::section { background: %1; border: none; border-bottom: 1px solid %3; }"
            ).arg(altBase, muted, mid, hover);

            // --- item views: roomier rows, themed selection ---
            s += QString(
                "QTreeView, QTableView, QListView { selection-background-color: %1; selection-color: %2; outline: none; }"
                "QTreeView::item, QListView::item { padding: 4px 2px; }"
                "QTableView::item { padding: 3px 4px; }"
                "QTreeView::item:selected, QTableView::item:selected, QListView::item:selected { background: %1; color: %2; }"
            ).arg(accent, onAccent);

            // --- buttons ---
            // Primary buttons are filled brand green (white text) — the single
            // place, with selection/active, where the accent is used. Keyed off an
            // explicit `primary` dynamic property rather than the :default state,
            // so a merely focused/auto-default secondary button never turns green.
            const QString accentPress = t.highlight.darker(115).name();
            s += QString(
                "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 5px;"
                    " padding: 5px 14px; }"
                "QPushButton:hover { background: %4; }"
                "QPushButton:pressed { background: %3; }"
                "QPushButton:disabled { color: %6; background: %7; border-color: %3; }"
                "QPushButton[primary=\"true\"] { background: %5; color: %8; border: 1px solid %5; }"
                "QPushButton[primary=\"true\"]:hover { background: %9; border-color: %9; }"
                "QPushButton[primary=\"true\"]:pressed { background: %9; }"
                "QPushButton[primary=\"true\"]:disabled { background: %3; color: %6; border-color: %3; }"
            ).arg(base, text, mid, hover, accent, muted, window, onAccent, accentPress);

            // Destructive/commit buttons: filled danger red with white text, the
            // only place red is used as a fill (reserved for irreversible-action
            // confirmations). Keyed off the `danger` dynamic property.
            s += QString(
                "QPushButton[danger=\"true\"] { background: %1; color: %2; border: 1px solid %1; }"
                "QPushButton[danger=\"true\"]:hover { background: %3; border-color: %3; }"
                "QPushButton[danger=\"true\"]:pressed { background: %3; }"
                "QPushButton[danger=\"true\"]:disabled { background: %4; color: %5; border-color: %4; }"
            ).arg(t.danger.name(), onAccent, t.dangerPress.name(), mid, muted);

            // --- inputs ---
            // Flat fields and combos (rounded 1px border, brand-green focus ring).
            // A stylesheet'd non-editable QComboBox drops its current-item icon, so
            // icon-bearing combos (the environment picker) use FlatComboBox, which
            // re-draws the icon over this flat frame.
            s += QString(
                "QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {"
                    " background: %1; color: %2; border: 1px solid %3; border-radius: 6px;"
                    " padding: 4px 8px; min-height: 22px;"
                    " selection-background-color: %4; selection-color: %5; }"
                "QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus,"
                    " QDoubleSpinBox:focus, QComboBox:focus, QComboBox:on { border: 1px solid %4; }"
                "QComboBox::drop-down { border: none; width: 20px; }"
                "QComboBox QAbstractItemView { background: %1; border: 1px solid %3;"
                    " selection-background-color: %4; selection-color: %5; outline: none; }"
                "QToolTip { background: %1; color: %2; border: 1px solid %3; padding: 4px 6px; }"
            ).arg(base, text, mid, accent, onAccent);

            // --- dialog tabs (scoped to QDialog so the main work-area tabs, which
            // set their own stylesheet, are untouched). Flat with an underline
            // active indicator, matching the .dtab style in the dialogs mockup. ---
            s += QString(
                "QDialog QTabWidget::pane { border: 1px solid %3; background: %4; top: -1px; }"
                "QDialog QTabBar { background: transparent; }"
                "QDialog QTabBar::tab { background: transparent; color: %2; border: none;"
                    " border-bottom: 2px solid transparent; padding: 7px 14px; margin: 0px; }"
                "QDialog QTabBar::tab:hover { color: %1; }"
                "QDialog QTabBar::tab:selected { color: %1; border-bottom: 2px solid %5; }"
            ).arg(text, muted, mid, base, accent);

            return s;
        }

        const Tokens &tokens(bool dark)
        {
            return dark ? kDark : kLight;
        }

        bool isDark()
        {
            return QtUtils::isDarkMode();
        }

        QString uiFontFamily()
        {
            return g_uiFontFamily;
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
                QApplication::setStyle(new FlatProxyStyle(fusion));
            QApplication::setPalette(buildPalette(isDark()));

            // Inter is the design-study UI font; bundle it (OFL, see
            // resources/fonts/Inter-OFL.txt) and register it so the typography is
            // identical on every platform regardless of what is installed. Bind to
            // the exact family name the font registers under, falling back to the
            // platform default if the resource fails to load. No size bump —
            // inflating it clips text in the tighter fixed grid layouts. The
            // monospace editor font is set separately (GuiRegistry::font), so
            // query/result text is unaffected.
            static QString interFamily;
            if (interFamily.isEmpty()) {
                const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/docutaz/fonts/Inter-Regular.otf"));
                QFontDatabase::addApplicationFont(QStringLiteral(":/docutaz/fonts/Inter-SemiBold.otf"));
                const QStringList fams = (id >= 0) ? QFontDatabase::applicationFontFamilies(id) : QStringList();
                interFamily = fams.isEmpty() ? QStringLiteral("Inter") : fams.first();
                g_uiFontFamily = interFamily;
            }
            QFont uiFont(interFamily);
            uiFont.setPointSize(QApplication::font().pointSize());
            QApplication::setFont(uiFont);

            qApp->setStyleSheet(buildGlobalStyleSheet(current()));
        }

        QString globalStyleSheet()
        {
            return buildGlobalStyleSheet(current());
        }

        void markPrimary(QAbstractButton *button)
        {
            if (!button)
                return;
            button->setProperty("primary", true);
            // The button was already polished when created, so force a re-polish
            // for the [primary="true"] rule to apply.
            button->style()->unpolish(button);
            button->style()->polish(button);
        }

        void markDanger(QAbstractButton *button)
        {
            if (!button)
                return;
            button->setProperty("danger", true);
            button->style()->unpolish(button);
            button->style()->polish(button);
        }

        Notifier *Notifier::instance()
        {
            static Notifier *n = new Notifier(qApp);
            return n;
        }

        void installColorSchemeWatch()
        {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            QObject::connect(
                QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                Notifier::instance(), [](Qt::ColorScheme) {
                    // Re-pick the palette + global QSS for the new scheme, then
                    // let imperative widgets refresh via changed(). Glyph icons
                    // resolve their tint at paint time, so they follow along once
                    // the palette change triggers repaints.
                    apply();
                    Notifier::instance()->notify();
                });
#endif
        }
    }
}
