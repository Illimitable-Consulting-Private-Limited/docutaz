#pragma once
#include <memory>
#include "docutaz/core/Event.h"

namespace Docutaz
{
    class EventWrapper : public QEvent
    {
    public:
        EventWrapper(Event *event, QList<QObject *> receivers);
        EventWrapper(Event *event, QObject * receiver);
        Event *event() const;
        const QList<QObject *> &receivers() const;

    private:
        const std::unique_ptr<Event> _event;
        const QList<QObject *> _receivers;
    };
}
