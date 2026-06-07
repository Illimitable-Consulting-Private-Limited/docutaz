#include "docutaz/core/AppRegistry.h"

#include "docutaz/core/EventBus.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/domain/App.h"

namespace Docutaz
{
    AppRegistry::AppRegistry() :
        _bus(new EventBus()),
        _settingsManager(new SettingsManager()),
        _app(new App(_bus.get()))
    {
    }

    AppRegistry::~AppRegistry()
    {   
    }
}

