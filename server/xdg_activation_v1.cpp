/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_activation_v1_p.h"

#include "client.h"
#include "display.h"
#include "seat_p.h"

#include <unistd.h>

namespace Wrapland::Server
{

const struct xdg_activation_v1_interface XdgActivationV1::Private::s_interface = {
    resourceDestroyCallback,
    cb<getActivationTokenCallback>,
    cb<activateCallback>,
};

XdgActivationV1::Private::Private(Display* display, XdgActivationV1* qptr)
    : XdgActivationV1Global(qptr, display, &xdg_activation_v1_interface, &s_interface)
{
    create();
}

XdgActivationV1::Private::~Private() = default;

void XdgActivationV1::Private::getActivationTokenCallback(XdgActivationV1Bind* bind, uint32_t id)
{
    auto request = new XdgActivationTokenV1(
        bind->client()->handle(), bind->version(), id, bind->global()->handle());
    if (!request->d_ptr->resource()) {
        bind->post_no_memory();
        delete request;
    }
}

void XdgActivationV1::Private::activateCallback(XdgActivationV1Bind* bind,
                                                char const* token,
                                                wl_resource* wlSurface)
{
    auto activation = handle(bind->resource());
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);

    Q_EMIT activation->activate(token, surface);
}

XdgActivationV1::XdgActivationV1(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

XdgActivationV1::~XdgActivationV1() = default;

const struct xdg_activation_token_v1_interface XdgActivationTokenV1::Private::s_interface = {
    setSerialCallback,
    setAppIdCallback,
    setSurfaceCallback,
    commitCallback,
    destroyCallback,
};

XdgActivationTokenV1::Private::Private(Client* client,
                                       uint32_t version,
                                       uint32_t id,
                                       XdgActivationV1* device,
                                       XdgActivationTokenV1* qptr)
    : Wayland::Resource<XdgActivationTokenV1>(client,
                                              version,
                                              id,
                                              &xdg_activation_token_v1_interface,
                                              &s_interface,
                                              qptr)
    , device{device}
{
}

void XdgActivationTokenV1::Private::setSerialCallback([[maybe_unused]] wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      uint32_t serial,
                                                      wl_resource* wlSeat)
{
    auto priv = handle(wlResource)->d_ptr;
    auto seat = SeatGlobal::handle(wlSeat);
    if (priv->seat) {
        disconnect(priv->seat, &Seat::destroyed, priv->handle(), nullptr);
    }
    connect(seat, &Seat::destroyed, priv->handle(), [priv] { priv->seat = nullptr; });
    priv->seat = seat;
    priv->serial = serial;
}

void XdgActivationTokenV1::Private::setAppIdCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource,
                                                     char const* app_id)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->app_id = app_id;
}

void XdgActivationTokenV1::Private::setSurfaceCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       wl_resource* wlSurface)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->surface = Wayland::Resource<Surface>::handle(wlSurface);
}

void XdgActivationTokenV1::Private::commitCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    if (priv->device) {
        priv->device->token_requested(priv->handle());
    }
}

XdgActivationTokenV1::XdgActivationTokenV1(Client* client,
                                           uint32_t version,
                                           uint32_t id,
                                           XdgActivationV1* device)
    : d_ptr(new Private(client, version, id, device, this))
{
    d_ptr->destroy_connection
        = connect(device, &XdgActivationV1::destroyed, this, [this] { d_ptr->device = nullptr; });
    connect(client, &Client::disconnected, this, [this] { disconnect(d_ptr->destroy_connection); });
}

uint32_t XdgActivationTokenV1::serial() const
{
    return d_ptr->serial;
}

Seat* XdgActivationTokenV1::seat() const
{
    return d_ptr->seat;
}

std::string XdgActivationTokenV1::app_id() const
{
    return d_ptr->app_id;
}

Surface* XdgActivationTokenV1::surface() const
{
    return d_ptr->surface;
}

void XdgActivationTokenV1::done(std::string const& token)
{
    d_ptr->send<xdg_activation_token_v1_send_done>(token.c_str());
}

}
