#include "robomongo/core/EventBusSubscriber.h"
#include "robomongo/core/EventBusDispatcher.h"

namespace Docutaz
{
    EventBusSubscriber::EventBusSubscriber(EventBusDispatcher *dispatcher, QObject *receiver, QObject *sender) :
        receiver(receiver),
        dispatcher(dispatcher),
        sender(sender) {}
}
