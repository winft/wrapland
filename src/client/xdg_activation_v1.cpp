/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_activation_v1.h"

#include "event_queue.h"
#include "seat.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <wayland-xdg-activation-v1-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN XdgActivationV1::Private
{
public:
    Private(XdgActivationV1* q);

    void setup(xdg_activation_v1* activation);

    WaylandPointer<xdg_activation_v1, xdg_activation_v1_destroy> activation;
    EventQueue* queue = nullptr;

    XdgActivationV1* q_ptr;
};

XdgActivationV1::Private::Private(XdgActivationV1* q)
    : q_ptr(q)
{
}

void XdgActivationV1::Private::setup(xdg_activation_v1* activation)
{
    assert(activation);
    assert(!this->activation);
    this->activation.setup(activation);
}

XdgActivationV1::XdgActivationV1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

XdgActivationV1::~XdgActivationV1()
{
    release();
}

void XdgActivationV1::setup(xdg_activation_v1* activation)
{
    d_ptr->setup(activation);
}

void XdgActivationV1::release()
{
    d_ptr->activation.release();
}

XdgActivationV1::operator xdg_activation_v1*()
{
    return d_ptr->activation;
}

XdgActivationV1::operator xdg_activation_v1*() const
{
    return d_ptr->activation;
}

bool XdgActivationV1::isValid() const
{
    return d_ptr->activation.isValid();
}

void XdgActivationV1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* XdgActivationV1::eventQueue()
{
    return d_ptr->queue;
}

XdgActivationTokenV1* XdgActivationV1::create_token(QObject* parent)
{
    assert(isValid());
    auto token = new XdgActivationTokenV1(parent);
    auto proxy = xdg_activation_v1_get_activation_token(d_ptr->activation);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(proxy);
    }
    token->setup(proxy);
    return token;
}

void XdgActivationV1::activate(std::string const& token, Surface* surface)
{
    assert(surface);
    xdg_activation_v1_activate(d_ptr->activation, token.c_str(), *surface);
}

class XdgActivationTokenV1::Private
{
public:
    Private(XdgActivationTokenV1* q);
    void setup(struct xdg_activation_token_v1* token);

    WaylandPointer<struct xdg_activation_token_v1, xdg_activation_token_v1_destroy> token;

private:
    static void
    doneCallback(void* data, struct xdg_activation_token_v1* wlToken, char const* token);

    XdgActivationTokenV1* q_ptr;

    static xdg_activation_token_v1_listener const s_listener;
};

xdg_activation_token_v1_listener const XdgActivationTokenV1::Private::s_listener = {
    doneCallback,
};

void XdgActivationTokenV1::Private::doneCallback(void* data,
                                                 struct xdg_activation_token_v1* wlToken,
                                                 char const* token)
{
    auto priv = reinterpret_cast<XdgActivationTokenV1::Private*>(data);
    assert(priv->token == wlToken);

    Q_EMIT priv->q_ptr->done(token);
}

XdgActivationTokenV1::Private::Private(XdgActivationTokenV1* q)
    : q_ptr(q)
{
}

void XdgActivationTokenV1::Private::setup(struct xdg_activation_token_v1* token)
{
    assert(token);
    assert(!this->token);
    this->token.setup(token);
    xdg_activation_token_v1_add_listener(token, &s_listener, this);
}

XdgActivationTokenV1::XdgActivationTokenV1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

XdgActivationTokenV1::~XdgActivationTokenV1() = default;

void XdgActivationTokenV1::setup(struct xdg_activation_token_v1* token)
{
    d_ptr->setup(token);
}

bool XdgActivationTokenV1::isValid() const
{
    return d_ptr->token.isValid();
}

void XdgActivationTokenV1::set_serial(uint32_t serial, Seat* seat)
{
    xdg_activation_token_v1_set_serial(d_ptr->token, serial, *seat);
}

void XdgActivationTokenV1::set_app_id(std::string const& app_id)
{
    xdg_activation_token_v1_set_app_id(d_ptr->token, app_id.c_str());
}

void XdgActivationTokenV1::set_surface(Surface* surface)
{
    xdg_activation_token_v1_set_surface(d_ptr->token, *surface);
}

void XdgActivationTokenV1::commit()
{
    xdg_activation_token_v1_commit(d_ptr->token);
}

}
