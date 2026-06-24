#include "docutaz/core/utils/QtUtils.h"

#include <QThread>
#include <QTreeWidgetItem>
#include <QApplication>
#include <QPalette>
#include <QWidget>
#include <QStyleHints>

namespace Docutaz
{
    namespace QtUtils
    {
        template<>
        QString toQString<std::string>(const std::string &value)
        {
            //static QTextCodec *LOCALECODEC = QTextCodec::codecForLocale();
            return QString::fromUtf8(value.c_str(), value.size());
        }

        template<>
        QString toQString<std::wstring>(const std::wstring &value)
        {
            return  QString((const QChar*)value.c_str(), value.length());
        }

        std::string toStdString(const QString &value)
        {
            QByteArray sUtf8 = value.toUtf8();
            return std::string(sUtf8.constData(), sUtf8.length());
        }

        std::string toStdStringSafe(const QString &value)
        {
#ifdef Q_OS_WIN
            QByteArray sUtf8 = value.toLocal8Bit();            
#else
            QByteArray sUtf8 = value.toUtf8();
#endif    
            return std::string(sUtf8.constData(), sUtf8.length());
        }

        void cleanUpThread(QThread *const thread)
        {
            if (thread && thread->isRunning()) {
                //thread->stop();
                thread->wait();
            }
        }

        void clearChildItems(QTreeWidgetItem *const root)
        {
            int itemCount = root->childCount();
            for (int i = 0; i < itemCount; ++i) {
                QTreeWidgetItem *item = root->child(0);
                root->removeChild(item);
                delete item;
            }
        }

        bool isDarkPalette(const QWidget *const w)
        {
            const QPalette pal = w ? w->palette() : qApp->palette();
            return pal.color(QPalette::Window).lightness() < 128;
        }

        bool isDarkMode(const QWidget *const w)
        {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            // OS-reported appearance; tracks live light/dark switches on 6.5+.
            const Qt::ColorScheme scheme = qApp->styleHints()->colorScheme();
            if (scheme != Qt::ColorScheme::Unknown)
                return scheme == Qt::ColorScheme::Dark;
#endif
            return isDarkPalette(w);
        }
    }
}
