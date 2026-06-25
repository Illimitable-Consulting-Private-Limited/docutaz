#pragma once

#include <QColor>
#include <QObject>

QT_BEGIN_NAMESPACE
class QPalette;
class QWidget;
class QAbstractButton;
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

        // The resolved UI font family ("Inter" once the bundled font has loaded
        // in apply(); the literal "Inter" before that). Used so the editor and
        // result views share the same family as the chrome.
        QString uiFontFamily();

        // A curated QPalette built from the tokens for the given scheme.
        QPalette buildPalette(bool dark);

        // Apply the base style (Fusion) and the scheme-selected palette to the
        // application. Safe to call again on a colour-scheme change.
        void apply();

        // Application-wide QSS for shared flat chrome (slim themed scrollbars).
        // Callers that set their own qApp stylesheet must prepend this so the
        // shared rules survive (e.g. MainWindow appends its own widget rules).
        QString globalStyleSheet();

        // Tag a button as the primary/action button (filled brand green). Sets
        // the `primary` dynamic property the global QSS keys off and repolishes
        // so the rule takes effect even though the button was already polished.
        void markPrimary(QAbstractButton *button);

        // Single application-wide source of theme-change notifications. Widgets
        // that paint or cache theme colours imperatively (the editors, the
        // work-area tab strip, the result chrome) connect to changed() and
        // refresh themselves; palette-/QSS-driven widgets adapt on their own.
        class Notifier : public QObject
        {
            Q_OBJECT
        public:
            static Notifier *instance();
            void notify() { emit changed(); }
        signals:
            void changed();
        private:
            explicit Notifier(QObject *parent = nullptr) : QObject(parent) {}
        };

        // Watch the OS colour scheme (Qt 6.5+). When it flips, re-apply the
        // palette + global stylesheet and emit Notifier::changed(). No-op below
        // 6.5 (those builds use the one-shot palette heuristic at startup).
        void installColorSchemeWatch();
    }
}
