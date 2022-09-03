/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "plasma_activation_feedback.h"

#include "event_queue.h"
#include "wayland_pointer_p.h"

#include <wayland-plasma-window-management-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN plasma_activation_feedback::Private
{
public:
    Private(plasma_activation_feedback* q);

    void setup(org_kde_plasma_activation_feedback* activation_feedback);

    WaylandPointer<org_kde_plasma_activation_feedback, org_kde_plasma_activation_feedback_destroy>
        activation_feedback_ptr;
    EventQueue* queue{nullptr};

    plasma_activation_feedback* q_ptr;

private:
    static void activation_callback(void* data,
                                    org_kde_plasma_activation_feedback* feedback,
                                    org_kde_plasma_activation* activation);

    static org_kde_plasma_activation_feedback_listener const s_listener;
};

class plasma_activation::Private
{
public:
    Private(plasma_activation* q);
    void setup(struct org_kde_plasma_activation* activation);

    WaylandPointer<struct org_kde_plasma_activation, org_kde_plasma_activation_destroy>
        activation_ptr;

    std::string app_id;
    bool finished{false};

private:
    static void
    app_id_callback(void* data, struct org_kde_plasma_activation* wlActivation, char const* app_id);
    static void finished_callback(void* data, struct org_kde_plasma_activation* wlActivation);

    plasma_activation* q_ptr;

    static org_kde_plasma_activation_listener const s_listener;
};

org_kde_plasma_activation_feedback_listener const plasma_activation_feedback::Private::s_listener
    = {
        activation_callback,
};

void plasma_activation_feedback::Private::activation_callback(
    void* data,
    [[maybe_unused]] org_kde_plasma_activation_feedback* feedback,
    org_kde_plasma_activation* activation)
{
    auto priv = reinterpret_cast<plasma_activation_feedback::Private*>(data);
    assert(priv->activation_feedback_ptr == feedback);

    if (!priv->q_ptr) {
        return;
    }

    auto activation_wrap = new plasma_activation();
    activation_wrap->d_ptr->setup(activation);
    Q_EMIT priv->q_ptr->activation(activation_wrap);
}

plasma_activation_feedback::Private::Private(plasma_activation_feedback* q)
    : q_ptr(q)
{
}

void plasma_activation_feedback::Private::setup(
    org_kde_plasma_activation_feedback* activation_feedback)
{
    assert(activation_feedback);
    assert(!this->activation_feedback_ptr);
    this->activation_feedback_ptr.setup(activation_feedback);
    org_kde_plasma_activation_feedback_add_listener(activation_feedback, &s_listener, this);
}

plasma_activation_feedback::plasma_activation_feedback(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

plasma_activation_feedback::~plasma_activation_feedback()
{
    release();
}

void plasma_activation_feedback::setup(org_kde_plasma_activation_feedback* activation_feedback)
{
    d_ptr->setup(activation_feedback);
}

void plasma_activation_feedback::release()
{
    d_ptr->activation_feedback_ptr.release();
}

plasma_activation_feedback::operator org_kde_plasma_activation_feedback*()
{
    return d_ptr->activation_feedback_ptr;
}

plasma_activation_feedback::operator org_kde_plasma_activation_feedback*() const
{
    return d_ptr->activation_feedback_ptr;
}

bool plasma_activation_feedback::isValid() const
{
    return d_ptr->activation_feedback_ptr.isValid();
}

void plasma_activation_feedback::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* plasma_activation_feedback::eventQueue()
{
    return d_ptr->queue;
}

org_kde_plasma_activation_listener const plasma_activation::Private::s_listener = {
    app_id_callback,
    finished_callback,
};

void plasma_activation::Private::app_id_callback(void* data,
                                                 struct org_kde_plasma_activation* wlActivation,
                                                 char const* app_id)
{
    auto priv = reinterpret_cast<plasma_activation::Private*>(data);
    assert(priv->activation_ptr == wlActivation);

    priv->app_id = app_id;
    Q_EMIT priv->q_ptr->app_id_changed();
}

void plasma_activation::Private::finished_callback(void* data,
                                                   struct org_kde_plasma_activation* wlActivation)
{
    auto priv = reinterpret_cast<plasma_activation::Private*>(data);
    assert(priv->activation_ptr == wlActivation);

    priv->finished = true;
    Q_EMIT priv->q_ptr->finished();
}

plasma_activation::Private::Private(plasma_activation* q)
    : q_ptr(q)
{
}

void plasma_activation::Private::setup(struct org_kde_plasma_activation* activation)
{
    assert(activation);
    assert(!this->activation_ptr);
    this->activation_ptr.setup(activation);
    org_kde_plasma_activation_add_listener(activation, &s_listener, this);
}

plasma_activation::plasma_activation(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

plasma_activation::~plasma_activation() = default;

void plasma_activation::setup(struct org_kde_plasma_activation* activation)
{
    d_ptr->setup(activation);
}

bool plasma_activation::isValid() const
{
    return d_ptr->activation_ptr.isValid();
}

std::string const& plasma_activation::app_id() const
{
    return d_ptr->app_id;
}

bool plasma_activation::is_finished() const
{
    return d_ptr->finished;
}

}
