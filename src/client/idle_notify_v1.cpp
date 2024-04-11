/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "idle_notify_v1.h"

#include "event_queue.h"
#include "seat.h"
#include "wayland_pointer_p.h"

#include <wayland-ext-idle-notify-v1-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN idle_notifier_v1::Private
{
public:
    WaylandPointer<ext_idle_notifier_v1, ext_idle_notifier_v1_destroy> manager;
    EventQueue* queue = nullptr;
};

idle_notifier_v1::idle_notifier_v1(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

idle_notifier_v1::~idle_notifier_v1()
{
    release();
}

void idle_notifier_v1::release()
{
    d->manager.release();
}

bool idle_notifier_v1::isValid() const
{
    return d->manager.isValid();
}

void idle_notifier_v1::setup(ext_idle_notifier_v1* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!d->manager.isValid());
    d->manager.setup(manager);
}

EventQueue* idle_notifier_v1::eventQueue()
{
    return d->queue;
}

void idle_notifier_v1::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

idle_notification_v1* idle_notifier_v1::get_notification(quint32 msecs, Seat* seat, QObject* parent)
{
    Q_ASSERT(isValid());
    Q_ASSERT(seat);
    auto notification = new idle_notification_v1(parent);
    auto i = ext_idle_notifier_v1_get_idle_notification(d->manager, msecs, *seat);
    if (d->queue) {
        d->queue->addProxy(i);
    }
    notification->setup(i);
    return notification;
}

idle_notifier_v1::operator ext_idle_notifier_v1*() const
{
    return d->manager;
}

idle_notifier_v1::operator ext_idle_notifier_v1*()
{
    return d->manager;
}

class Q_DECL_HIDDEN idle_notification_v1::Private
{
public:
    explicit Private(idle_notification_v1* q);
    void setup(ext_idle_notification_v1* d);

    WaylandPointer<ext_idle_notification_v1, ext_idle_notification_v1_destroy> timeout;

private:
    static void idled_callback(void* data, ext_idle_notification_v1* notification);
    static void resumed_callback(void* data, ext_idle_notification_v1* notification);

    static const struct ext_idle_notification_v1_listener s_listener;

    idle_notification_v1* q;
};

ext_idle_notification_v1_listener const idle_notification_v1::Private::s_listener = {
    idled_callback,
    resumed_callback,
};

void idle_notification_v1::Private::idled_callback(void* data,
                                                   ext_idle_notification_v1* /*notification*/)
{
    Q_EMIT reinterpret_cast<Private*>(data)->q->idled();
}

void idle_notification_v1::Private::resumed_callback(void* data,
                                                     ext_idle_notification_v1* /*notification*/)
{
    Q_EMIT reinterpret_cast<Private*>(data)->q->resumed();
}

idle_notification_v1::Private::Private(idle_notification_v1* q)
    : q(q)
{
}

void idle_notification_v1::Private::setup(ext_idle_notification_v1* d)
{
    Q_ASSERT(d);
    Q_ASSERT(!timeout.isValid());
    timeout.setup(d);
    ext_idle_notification_v1_add_listener(timeout, &s_listener, this);
}

idle_notification_v1::idle_notification_v1(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

idle_notification_v1::~idle_notification_v1()
{
    release();
}

void idle_notification_v1::release()
{
    d->timeout.release();
}

bool idle_notification_v1::isValid() const
{
    return d->timeout.isValid();
}

void idle_notification_v1::setup(ext_idle_notification_v1* dataDevice)
{
    d->setup(dataDevice);
}

idle_notification_v1::operator ext_idle_notification_v1*()
{
    return d->timeout;
}

idle_notification_v1::operator ext_idle_notification_v1*() const
{
    return d->timeout;
}

}
