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

constexpr uint32_t KdeIdleVersion = 1;
using KdeIdleGlobal = Wayland::Global<KdeIdle, KdeIdleVersion>;
using KdeIdleBind = Wayland::Bind<KdeIdleGlobal>;

class KdeIdle::Private : public KdeIdleGlobal
{
public:
    Private(Display* display, KdeIdle* q_ptr);
    ~Private() override;

private:
    static void
    getIdleTimeoutCallback(KdeIdleBind* bind, uint32_t id, wl_resource* wlSeat, uint32_t timeout);

    static const struct org_kde_kwin_idle_interface s_interface;
};

class IdleTimeout::Private : public Wayland::Resource<IdleTimeout>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            std::chrono::milliseconds duration,
            Seat* seat,
            IdleTimeout* q_ptr);
    ~Private() override;

    std::chrono::milliseconds duration;
    Seat* seat;

private:
    static void simulateUserActivityCallback(wl_client* wlClient, wl_resource* wlResource);
    static const struct org_kde_kwin_idle_timeout_interface s_interface;
};

}
