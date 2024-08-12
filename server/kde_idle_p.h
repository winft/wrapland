/*
    SPDX-FileCopyrightText: 2020 Faveraux Adrien <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "kde_idle.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-idle-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t kde_idle_version = 1;
using kde_idle_global = Wayland::Global<kde_idle, kde_idle_version>;

class kde_idle::Private : public kde_idle_global
{
public:
    Private(Display* display, kde_idle* q_ptr);

private:
    static void get_idle_timeout_callback(kde_idle_global::bind_t* bind,
                                          uint32_t id,
                                          wl_resource* wlSeat,
                                          uint32_t timeout);

    static const struct org_kde_kwin_idle_interface s_interface;
};

class kde_idle_timeout::Private : public Wayland::Resource<kde_idle_timeout>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            std::chrono::milliseconds duration,
            Seat* seat,
            kde_idle_timeout* q_ptr);
    ~Private() override;

    std::chrono::milliseconds duration;
    Seat* seat;

private:
    static void simulate_user_activity_callback(wl_client* wlClient, wl_resource* wlResource);
    static const struct org_kde_kwin_idle_timeout_interface s_interface;
};

}
