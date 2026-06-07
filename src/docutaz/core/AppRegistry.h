#pragma once

#include "docutaz/core/Core.h"
#include "docutaz/core/utils/SingletonPattern.hpp"

namespace Docutaz
{
    class SettingsManager;

    class AppRegistry: public Patterns::LazySingleton<AppRegistry>
    {
        friend class Patterns::LazySingleton<AppRegistry>;
    public:

        SettingsManager *const settingsManager() const { return _settingsManager.get(); }
        App *const app() const { return _app.get(); }
        EventBus *const bus() const { return _bus.get(); }

    private:
        AppRegistry();
        ~AppRegistry();

        const EventBusScopedPtr _bus;
        const SettingsManagerScopedPtr _settingsManager;
        const AppScopedPtr _app;
    };
}
