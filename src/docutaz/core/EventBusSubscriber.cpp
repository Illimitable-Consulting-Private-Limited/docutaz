#include "docutaz/core/EventBusSubscriber.h"
#include "docutaz/core/EventBusDispatcher.h"

namespace Docutaz
{
    EventBusSubscriber::EventBusSubscriber(EventBusDispatcher *dispatcher, QObject *receiver, QObject *sender) :
        receiver(receiver),
        dispatcher(dispatcher),
        sender(sender) {}
}
