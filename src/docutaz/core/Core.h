#pragma once

#include <memory>

namespace mongo
{
    class DBClientConnection;
}

/*
** Smart pointers for Mongo* staff
*/
namespace Docutaz
{
    class AppRegistry;
    typedef std::unique_ptr<AppRegistry> AppRegistryScopedPtr;

    class SettingsManager;
    typedef std::unique_ptr<SettingsManager> SettingsManagerScopedPtr;

    class App;
    typedef std::unique_ptr<App> AppScopedPtr;

    class EventBus;
    typedef std::unique_ptr<EventBus> EventBusScopedPtr;

    class MongoCollection;
    typedef std::shared_ptr<MongoCollection> MongoCollectionPtr;

    class MongoDocument;
    typedef std::shared_ptr<MongoDocument> MongoDocumentPtr;

    // todo: Use enum class
    enum ConnectionType {
        // This type of connection is shown in Explorer and also opens SSH tunnel for secondary 
        // connections (if needed)
        ConnectionPrimary      = 0,

        // Never shown in Explorer and uses SSH tunnel from primary connection (if needed)
        ConnectionSecondary    = 1,

        // The same as Primary, but is specifically for testing connections.
        ConnectionTest         = 2,

        // Never shown in Explorer and can be used to refresh (via reconnecting) current connection view 
        // (i.e. db version, storage engine, current replica set primary, status of replica set etc...)
        ConnectionRefresh      = 3
    };
}
