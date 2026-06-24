#pragma once
#include <QString>
#include <QModelIndex>

QT_BEGIN_NAMESPACE
class QThread;
class QTreeWidgetItem;
class QAbstractItemModel;
class QWidget;
QT_END_NAMESPACE

#ifdef QT_NO_DEBUG
#define VERIFY(x) (x)
#else //QT_NO_DEBUG
#define VERIFY(x) Q_ASSERT(x)
#endif //QT_NO_DEBUG

namespace Docutaz
{
    namespace QtUtils
    {
        template<typename T>
        QString toQString(const T &value);

        std::string toStdString(const QString &value);

        std::string toStdStringSafe(const QString &value);

        void cleanUpThread(QThread *const thread);

        void clearChildItems(QTreeWidgetItem *root);

        // True when the active palette is a dark one (dark desktop theme). The
        // app follows the OS palette and has no theme of its own, so widgets
        // that hardcode light colours use this to pick legible alternatives.
        // Pass a widget to read its palette, or nullptr to use the app palette.
        bool isDarkPalette(const QWidget *w = nullptr);

        // Single source of truth for whether the UI should render dark. On
        // Qt 6.5+ this follows the OS appearance via QStyleHints::colorScheme()
        // (and tracks user light/dark switches); below 6.5, or when the platform
        // reports an unknown scheme, it falls back to isDarkPalette(). Prefer
        // this over isDarkPalette() in new code so the theme follows the system.
        bool isDarkMode(const QWidget *w = nullptr);

        template<typename Type>
        inline Type item(const QModelIndex &index)
        {
            return static_cast<Type>(index.internalPointer());
        }

        struct HackQModelIndex
        {
            int r, c;
            void* i;
            const QAbstractItemModel *m;
        };
    }
}
