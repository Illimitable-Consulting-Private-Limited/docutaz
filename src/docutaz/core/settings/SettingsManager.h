#pragma once

#include <QString>
#include <QVariantMap>
#include <QSet>
#include <QDir>

#include <vector>
#include <cstdlib>

#include "docutaz/core/Enums.h"

namespace Docutaz
{
    class ConnectionSettings;
    struct ConfigFileAndImportFunction;
        
    // Current cache directory
    auto const CacheDir = QString("%1/.Docutaz/%2/cache/").arg(QDir::homePath())
                                                          .arg(PROJECT_VERSION);
    // Current config file
    auto const ConfigFilePath = QString("%1/.Docutaz/%2/docutaz.json").arg(QDir::homePath())
                                                                      .arg(PROJECT_VERSION);
    // Current config file directory
    auto const ConfigDir = QString("%1/.Docutaz/%2/").arg(QDir::homePath())
                                                     .arg(PROJECT_VERSION);

/* ----------------------------- SettingsManager ------------------------------ */

    /**
     * @brief SettingsManager gives you access to all settings, that is used
     *        by Docutaz. It can load() and save() them. Config file usually
     *        located here: ~/.Docutaz/<version>/docutaz.json
     *
     *        You can access this manager via:
     *        AppRegistry::instance().settingsManager()
     *
     * @threadsafe no
     */
    class SettingsManager
    {
    public:
        typedef std::vector<ConnectionSettings *> ConnectionSettingsContainerType;
        typedef QMap<QString, QVariant> ToolbarSettingsContainerType;

        /**
         * @brief Creates SettingsManager for config file in default location
         *        (usually ~/.Docutaz/<version>/docutaz.json)
         */
        SettingsManager();

        /**
         * @brief Cleanup owned objects
         */
        ~SettingsManager();

        /**
         * @brief Load settings from config file.
         * @return true if success, false otherwise
         */
        bool load();

        /**
         * @brief Saves all settings to config file.
         * @return true if success, false otherwise
         */
        bool save();

        /**
         * @brief Adds connection to the end of list.
         * Connection now will be owned by SettingsManager.
         */
        static void addConnection(ConnectionSettings *connection);

        /**
         * @brief Removes connection by index
         */
        void removeConnection(ConnectionSettings *connection);


        /**
        * @brief  Finds and returns original (non-clone) connection settings which is 
        *         loaded/saved from/into Docutaz config. file.
        * @return If uniqueID is valid returns original connection settings, 
        *         nullptr otherwise.
        */
        ConnectionSettings* getConnectionSettingsByUuid(QString const& uuid) const;
        ConnectionSettings* getConnectionSettingsByUuid(std::string const& uuid) const;

        void reorderConnections(const ConnectionSettingsContainerType &connections);

        void setToolbarSettings(QString toolbarName, bool visible);

        /**
         * @brief Returns list of connections
         */
        ConnectionSettingsContainerType connections() const { return _connections; }
        
        ToolbarSettingsContainerType toolbars() const { return _toolbars; }

        void setUuidEncoding(UUIDEncoding encoding) { _uuidEncoding = encoding; }
        UUIDEncoding uuidEncoding() const { return _uuidEncoding; }

        void setTimeZone(SupportedTimes timeZ) { _timeZone = timeZ; }
        SupportedTimes timeZone() const { return _timeZone; }

        void setViewMode(ViewMode viewMode) { _viewMode = viewMode; }
        ViewMode viewMode() const { return _viewMode; }

        void setAutocompletionMode(AutocompletionMode mode) { _autocompletionMode = mode; }
        AutocompletionMode autocompletionMode() const { return _autocompletionMode; }

        void setAutoExpand(bool isExpand) { _autoExpand = isExpand; }
        bool autoExpand() const { return _autoExpand; }

        void setAutoExec(bool isAutoExec) { _autoExec = isAutoExec; }
        bool autoExec() const { return _autoExec; }

        void setMinimizeToTray(bool isMinimizingToTray) { _minimizeToTray = isMinimizingToTray; }
        bool minimizeToTray() const { return _minimizeToTray; }

        void setLineNumbers(bool showLineNumbers) { _lineNumbers = showLineNumbers; }
        bool lineNumbers() const { return _lineNumbers; }

        void setLoadMongoRcJs(bool isLoadJs) { _loadMongoRcJs = isLoadJs; }
        bool loadMongoRcJs() const { return _loadMongoRcJs; }

        void setMongoshPath(const QString& path) { _mongoshPath = path; }
        QString mongoshPath() const { return _mongoshPath; }

        // When true, all shell tabs of one connection share a single mongosh
        // subprocess instead of each spawning its own. Lower memory and instant
        // tab opening, at the cost of serialized execution (a long query in one
        // tab blocks its siblings). Default false.
        void setShareShellPerConnection(bool v) { _shareShellPerConnection = v; }
        bool shareShellPerConnection() const { return _shareShellPerConnection; }

        void setDisableConnectionShortcuts(bool isDisable) { _disableConnectionShortcuts = isDisable; }
        bool disableConnectionShortcuts() const { return _disableConnectionShortcuts; }

        void addAcceptedEulaVersion(QString const& version) { _acceptedEulaVersions.insert(version); }
        QSet<QString> const& acceptedEulaVersions() const { return _acceptedEulaVersions; }

        void setBatchSize(int batchSize) { _batchSize = batchSize; }
        int batchSize() const { return _batchSize; }

        void setCheckForUpdates(bool value) { _checkForUpdates = value; }
        bool checkForUpdates() const { return _checkForUpdates; }

        void setSaveQueryHistory(bool value) { _saveQueryHistory = value; }
        bool saveQueryHistory() const { return _saveQueryHistory; }

        QString currentStyle() const { return _currentStyle; }
        void setCurrentStyle(const QString& style);

        QString textFontFamily() const { return _textFontFamily; }
        void setTextFontFamily(const QString& fontFamily);

        int textFontPointSize() const { return _textFontPointSize; }
        void setTextFontPointSize(int pointSize);

        int mongoTimeoutSec() const { return _mongoTimeoutSec; }
        int shellTimeoutSec() const { return _shellTimeoutSec; }

        void setShellTimeoutSec(int newValue) { _shellTimeoutSec = std::abs(newValue); }

        // True when settings from previous versions of Robomongo are imported
        void setImported(bool imported) { _imported = imported; }
        bool imported() const { return _imported; }

        void addCacheData(QString const& key, QVariant const& value);
        QVariant cacheData(QString const& key) const;

        // Designed to be set only by human users
        bool disableHttpsFeatures() const { return _disableHttpsFeatures; }
        bool debugMode() const { return _debugMode; }

        /**
         * Returns number of imported connections
         */
        int importedConnectionsCount();

    private:

        /**
         * Load settings from the map. Existing settings will be overwritten.
         */
        void loadFromMap(QVariantMap &map);

        /**
         * Save all settings to map.
         */
        QVariantMap convertToMap() const;

        /**
         * Load connection settings from previous versions of Robomongo
         */
        void importFromOldVersion();

        // Imports connections from oldConfigFilePath into current config file
        bool importFromFile(QString const& oldConfigFilePath);
        
        static bool importConnectionsFrom_0_8_5();

        /**
         * @brief Version of settings schema currently loaded
         */
        QString _version;

        UUIDEncoding _uuidEncoding;
        SupportedTimes _timeZone;
        ViewMode _viewMode;
        AutocompletionMode _autocompletionMode;
        bool _loadMongoRcJs;
        QString _mongoshPath;
        bool _shareShellPerConnection = false;
        bool _autoExpand;
        bool _autoExec;
        bool _minimizeToTray;
        bool _lineNumbers;
        bool _disableConnectionShortcuts;
        bool _disableHttpsFeatures = false;
        bool _checkForUpdates = true;
        bool _saveQueryHistory = true;
        bool _debugMode = false;
        QSet<QString> _acceptedEulaVersions;
        int _batchSize;
        QString _currentStyle;
        QString _textFontFamily;
        int _textFontPointSize;

        int _mongoTimeoutSec;
        int _shellTimeoutSec;

        // True when settings from previous versions of Robomongo are imported
        bool _imported;

        // Various cache data
        QMap<QString, QVariant> _cacheData;

        /**
         * @brief List of connections
         */
        static std::vector<ConnectionSettings*> _connections;

        ToolbarSettingsContainerType _toolbars;

        // List of config. file absolute paths of old versions
        // Must be updated with care and with every new version. Details on cpp file.       
        static std::vector<QString> const _configFilesOfOldVersions;
    };
}
