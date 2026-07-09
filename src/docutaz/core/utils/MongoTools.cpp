#include "docutaz/core/utils/MongoTools.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"

namespace Docutaz
{
    namespace MongoTools
    {
        namespace
        {
            QString exeName(const QString& base)
            {
#if defined(Q_OS_WIN)
                return base + ".exe";
#else
                return base;
#endif
            }

            // Strip surrounding quotes/whitespace from a user-typed path.
            QString clean(QString path)
            {
                path = path.trimmed();
                if (path.size() >= 2 && path.startsWith('"') && path.endsWith('"'))
                    path = path.mid(1, path.size() - 2);
                return path;
            }

            // Look for <base> inside a directory; returns the absolute path or empty.
            QString inDir(const QString& dir, const QString& base)
            {
                if (dir.isEmpty())
                    return {};
                const QFileInfo cand(QDir(dir), exeName(base));
                return cand.isFile() ? cand.absoluteFilePath() : QString{};
            }

            // Directories to search, highest priority first. A configured path may
            // point at the tools folder OR at one tool's executable — in the latter
            // case its containing folder is used (all four tools live together).
            QStringList candidateDirs()
            {
                QStringList dirs;

                const QString configured =
                    clean(AppRegistry::instance().settingsManager()->databaseToolsPath());
                if (!configured.isEmpty()) {
                    const QFileInfo fi(configured);
                    dirs << (fi.isFile() ? fi.absolutePath() : fi.absoluteFilePath());
                }

                const QString env = clean(QString::fromLocal8Bit(qgetenv("DOCUTAZ_DBTOOLS_DIR")));
                if (!env.isEmpty())
                    dirs << env;

#if defined(Q_OS_WIN)
                const QString programFiles = qEnvironmentVariable("ProgramFiles");
                // Versioned bundle dir, e.g. C:\Program Files\MongoDB\Tools\100\bin.
                if (!programFiles.isEmpty()) {
                    const QDir toolsRoot(programFiles + "\\MongoDB\\Tools");
                    if (toolsRoot.exists())
                        for (const QString& ver : toolsRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                            dirs << toolsRoot.absoluteFilePath(ver + "\\bin");
                }
#elif defined(Q_OS_MACOS)
                dirs << "/opt/homebrew/bin"        // Homebrew Apple Silicon
                     << "/usr/local/bin"           // Homebrew Intel / manual
                     << "/usr/bin";
#else
                dirs << "/app/bin"                 // bundled inside the Flatpak
                     << "/usr/bin"
                     << "/usr/local/bin"
                     << "/opt/mongodb-database-tools/bin";
#endif
                return dirs;
            }
        } // namespace

        QString findTool(const QString& baseName)
        {
            for (const QString& dir : candidateDirs()) {
                const QString hit = inDir(dir, baseName);
                if (!hit.isEmpty())
                    return hit;
            }
            // Final fallback: anything on PATH.
            const QString onPath = QStandardPaths::findExecutable(baseName);
            return onPath.isEmpty() ? QString{} : onPath;
        }

        QString findMongodump()    { return findTool("mongodump"); }
        QString findMongorestore() { return findTool("mongorestore"); }
        QString findMongoexport()  { return findTool("mongoexport"); }
        QString findMongoimport()  { return findTool("mongoimport"); }

        bool areToolsAvailable()
        {
            return !findMongodump().isEmpty() && !findMongorestore().isEmpty();
        }
    }
}
