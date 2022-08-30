/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "xdg_activation_v1.h"

#include "seat.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QObject>

#include <wayland-xdg-activation-v1-server-protocol.h>

namespace Wrapland::Server
{

class Display;

constexpr uint32_t XdgActivationV1Version = 1;
using XdgActivationV1Global = Wayland::Global<XdgActivationV1, XdgActivationV1Version>;
using XdgActivationV1Bind = Wayland::Bind<XdgActivationV1Global>;

class XdgActivationV1::Private : public XdgActivationV1Global
{
public:
    Private(Display* display, XdgActivationV1* q_ptr);
    ~Private() override;

private:
    static void getActivationTokenCallback(XdgActivationV1Bind* bind, uint32_t id);
    static void
    activateCallback(XdgActivationV1Bind* bind, char const* token, wl_resource* wlSurface);

    static const struct xdg_activation_v1_interface s_interface;
};

class XdgActivationTokenV1::Private : public Wayland::Resource<XdgActivationTokenV1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            XdgActivationV1* device,
            XdgActivationTokenV1* q_ptr);
    ~Private() override = default;

    uint32_t serial{0};
    Seat* seat{nullptr};
    std::string app_id;
    Surface* surface{nullptr};

    QMetaObject::Connection destroy_connection;
    XdgActivationV1* device;

private:
    static void setSerialCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  uint32_t serial,
                                  wl_resource* wlSeat);
    static void setAppIdCallback(wl_client* wlClient, wl_resource* wlResource, char const* app_id);
    static void
    setSurfaceCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlSurface);
    static void commitCallback(wl_client* wlClient, wl_resource* wlResource);

    static const struct xdg_activation_token_v1_interface s_interface;
};

}
