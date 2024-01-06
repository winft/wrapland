/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "security_context_v1.h"

#include "event_queue.h"
#include "seat.h"
#include "wayland_pointer_p.h"

#include <wayland-security-context-v1-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN security_context_manager_v1::Private
{
public:
    WaylandPointer<wp_security_context_manager_v1, wp_security_context_manager_v1_destroy> manager;
    EventQueue* queue = nullptr;
};

security_context_manager_v1::security_context_manager_v1(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

security_context_manager_v1::~security_context_manager_v1()
{
    release();
}

void security_context_manager_v1::release()
{
    d->manager.release();
}

bool security_context_manager_v1::isValid() const
{
    return d->manager.isValid();
}

void security_context_manager_v1::setup(wp_security_context_manager_v1* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!d->manager.isValid());
    d->manager.setup(manager);
}

EventQueue* security_context_manager_v1::eventQueue()
{
    return d->queue;
}

void security_context_manager_v1::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

security_context_v1*
security_context_manager_v1::create_listener(int32_t listen_fd, int32_t close_fd, QObject* parent)
{
    Q_ASSERT(isValid());
    Q_ASSERT(listen_fd > 0);
    Q_ASSERT(close_fd > 0);
    auto notification = new security_context_v1(parent);
    auto i = wp_security_context_manager_v1_create_listener(d->manager, listen_fd, close_fd);
    if (d->queue) {
        d->queue->addProxy(i);
    }
    notification->setup(i);
    return notification;
}

security_context_manager_v1::operator wp_security_context_manager_v1*() const
{
    return d->manager;
}

security_context_manager_v1::operator wp_security_context_manager_v1*()
{
    return d->manager;
}

class Q_DECL_HIDDEN security_context_v1::Private
{
public:
    explicit Private(security_context_v1* q);
    void setup(wp_security_context_v1* d);

    WaylandPointer<wp_security_context_v1, wp_security_context_v1_destroy> context;

private:
    security_context_v1* q;
};

security_context_v1::Private::Private(security_context_v1* q)
    : q(q)
{
}

void security_context_v1::Private::setup(wp_security_context_v1* d)
{
    Q_ASSERT(d);
    Q_ASSERT(!context.isValid());
    context.setup(d);
}

security_context_v1::security_context_v1(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

security_context_v1::~security_context_v1()
{
    release();
}

void security_context_v1::release()
{
    d->context.release();
}

bool security_context_v1::isValid() const
{
    return d->context.isValid();
}

void security_context_v1::setup(wp_security_context_v1* dataDevice)
{
    d->setup(dataDevice);
}

void security_context_v1::set_sandbox_engine(std::string const& engine) const
{
    Q_ASSERT(isValid());
    wp_security_context_v1_set_sandbox_engine(d->context, engine.c_str());
}

void security_context_v1::set_app_id(std::string const& engine) const
{
    Q_ASSERT(isValid());
    wp_security_context_v1_set_app_id(d->context, engine.c_str());
}

void security_context_v1::set_instance_id(std::string const& engine) const
{
    Q_ASSERT(isValid());
    wp_security_context_v1_set_instance_id(d->context, engine.c_str());
}

void security_context_v1::commit() const
{
    Q_ASSERT(isValid());
    wp_security_context_v1_commit(d->context);
}

security_context_v1::operator wp_security_context_v1*()
{
    return d->context;
}

security_context_v1::operator wp_security_context_v1*() const
{
    return d->context;
}

}
