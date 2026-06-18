#pragma once

#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <mongo/client/dbclient_base.h>
#include <mongo/client/mongo_uri.h>
#include <mongo/util/net/hostandport.h>

namespace Docutaz
{
    class CredentialSettings;
    class SshSettings;
    class SslSettings;
    class ReplicaSetSettings;

    /**
     * @brief Represents connection record
     */
    class ConnectionSettings : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief Creates ConnectionSettings with default values
         */
        ConnectionSettings(bool isClone);
        
        /**
        * @brief Creates ConnectionSettings from mongo connection string URI
        */
        ConnectionSettings(const mongo::MongoURI& uri, bool isClone);

        explicit ConnectionSettings(QVariantMap map, bool isClone);

        /**
         * @brief Cleanup used resources
         */
        ~ConnectionSettings();

        /**
         * @brief Creates completely new ConnectionSettings by cloning this record.
         */
        ConnectionSettings *clone() const;

        /**
         * @brief Discards current state and applies state from 'source' ConnectionSettings.
         */
        void apply(const ConnectionSettings *source);

        /**
         * @brief Converts to QVariantMap
         */
        QVariant toVariant() const;
        void fromVariant(const QVariantMap &map);

        /**
         * @brief Name of connection
         */
        std::string connectionName() const { return _connectionName; }
        void setConnectionName(const std::string &connectionName) { _connectionName = connectionName; }

        /**
         * @brief Server host
         */
        std::string serverHost() const { return _host; }
        void setServerHost(const std::string &serverHost) { _host = serverHost; }

        /**
         * @brief Port of server
         */
        int serverPort() const { return _port; }
        void setServerPort(const int port) { _port = port; }

        /**
         * @brief Default database
         */
        std::string defaultDatabase() const { return _defaultDatabase; }
        void setDefaultDatabase(const std::string &defaultDatabase) { _defaultDatabase = defaultDatabase; }

        /**
         * Was this connection imported from somewhere?
         */
        bool imported() const { return _imported; }
        void setImported(bool imported) { _imported = imported; }

        /**
         * @brief Adds credential to this connection
         */
        void addCredential(CredentialSettings *credential);

        /**
         * @brief Clears and releases memory occupied by credentials
         */
        void clearCredentials();

        /**
         * @brief Checks whether this connection has primary credential
         * which is also enabled.
         */
        bool hasEnabledPrimaryCredential() const;

        /**
         * @brief Returns primary credential
         */
        CredentialSettings *primaryCredential() const;

        /**
         * @brief Returns number of credentials
         */
        int credentialCount() const { return _credentials.size(); }

        /**
         * @brief Returns all credentials
         */
        QList<CredentialSettings *> credentials() const { return _credentials; }

        /**
         * @brief Checks that auth required
         */
        /*bool isAuthNeeded() const {
            bool userSpecified = !_userName.isEmpty();
            bool passwordSpecified = !_userPassword.isEmpty();

            return (userSpecified || passwordSpecified);
        }*/

        /**
         * @brief Returns connection full address (i.e. locahost:8090)
         */
        std::string getFullAddress() const;

        std::string getReadableName() const
        {
            if (_connectionName.empty())
                return getFullAddress();

            return _connectionName;
        }

        mongo::HostAndPort hostAndPort() const;
        SshSettings *sshSettings() const { return _sshSettings.get(); }
        SslSettings *sslSettings() const { return _sslSettings.get(); }
        ReplicaSetSettings *replicaSetSettings() const { return _replicaSetSettings.get(); }

        bool isReplicaSet() const { return _isReplicaSet; }
        void setReplicaSet(bool flag) { _isReplicaSet = flag; }

        // DNS seed list ("mongodb+srv://") connection, e.g. MongoDB Atlas. When
        // set, serverHost() holds the SRV hostname (no port), and the connection
        // URI is built with the mongodb+srv:// scheme so the driver resolves the
        // real hosts / replica-set / TLS from DNS — rather than connecting to the
        // SRV name directly (which has no A record and fails to resolve).
        bool isSrv() const { return _isSrv; }
        void setSrv(bool flag) { _isSrv = flag; }
       
        QString uuid() const { return _uuid; }
        void setUuid(QString const& uuid) { _uuid = uuid; }

        // Environment tag used to colour-code the connection in the UI (explorer
        // row, shell tab, an accent strip above the editor) — a "don't run a
        // delete on prod" safeguard. Stored as a stable key: "" (none),
        // "production", "staging", "development", "other". See
        // gui/ConnectionEnvironment.h for the key→colour/label mapping.
        std::string environment() const { return _environment; }
        void setEnvironment(const std::string &env) { _environment = env; }

        // Percent-encode a connection-string userinfo component (username or
        // password) per RFC 3986: every byte outside the unreserved set
        // (A-Z a-z 0-9 - . _ ~) is encoded. Required so a password containing
        // ':', '/', '?', '#', '[', ']', '@', '%', etc. doesn't corrupt the URI.
        static std::string percentEncodeUserInfo(const std::string &component);

        // Build a mongosh/driver connection URI (scheme, userinfo, host(s) or SRV
        // name, default db and auth/TLS/replica-set/timeout options) for the given
        // connection. Shared by the worker (to connect its own server) and the
        // "copy results" feature (to reach an arbitrary target connection).
        // timeoutSec drives serverSelection/connect timeouts. Note: this does NOT
        // account for an SSH tunnel — callers routing through one must rewrite the
        // host/port themselves.
        static std::string buildMongoUri(const ConnectionSettings *conn, int timeoutSec = 10);

    private:
        CredentialSettings *findCredential(const std::string &databaseName) const;

        std::string _connectionName;
        std::string _host;
        int _port;
        std::string _defaultDatabase;
        mutable QList<CredentialSettings *> _credentials;
        std::unique_ptr<SshSettings> _sshSettings;
        std::unique_ptr<SslSettings> _sslSettings;
        bool _isReplicaSet;
        bool _isSrv = false;
        std::string _environment;   // "" | production | staging | development | other
        std::unique_ptr<ReplicaSetSettings> _replicaSetSettings;
        
        // Was this connection imported from somewhere?
        bool _imported;

        // Flag to check if this is a clone(copy) or original ConnectionSettings
        // Note: If this is not a clone connection settings, this object is original connection 
        //       ConnectionSettings object which is loaded/saved into Docutaz config. file.
        bool _clone = false;

        // If this is a clone connection settings, unique ID will be used to identify from which 
        // original connection settings this object is cloned.
        // -1 for invalid(uninitialized) unique ID which should not be seen in theory
        int _uniqueId = -1;

        // UUID string taken from QUuid. 
        // It is used to identify the unique ID of a connection settings object
        QString _uuid;
    };
}

Q_DECLARE_METATYPE(Docutaz::ConnectionSettings *)
