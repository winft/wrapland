/*
    SPDX-FileCopyrightText: 2020 Faveraux Adrien <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "display.h"
#include "kde_idle_p.h"
#include "seat_p.h"

namespace Wrapland::Server
{

const struct org_kde_kwin_idle_interface KdeIdle::Private::s_interface
    = {cb<getIdleTimeoutCallback>};

KdeIdle::Private::Private(Display* display, KdeIdle* q_ptr)
    : Wayland::Global<KdeIdle>(q_ptr, display, &org_kde_kwin_idle_interface, &s_interface)
{
    create();
}

KdeIdle::Private::~Private() = default;

void KdeIdle::Private::getIdleTimeoutCallback(KdeIdleBind* bind,
                                              uint32_t id,
                                              wl_resource* wlSeat,
                                              uint32_t timeout)
{
    auto priv = bind->global()->handle->d_ptr.get();
    auto seat = SeatGlobal::get_handle(wlSeat);

    auto duration = std::max(std::chrono::milliseconds(timeout), std::chrono::milliseconds::zero());

    auto idleTimeout = new IdleTimeout(bind->client->handle, bind->version, id, duration, seat);
    if (!idleTimeout->d_ptr->resource) {
        bind->post_no_memory();
        delete idleTimeout;
        return;
    }

    Q_EMIT priv->handle->timeout_created(idleTimeout);
}

KdeIdle::KdeIdle(Display* display)
    : d_ptr(new Private(display, this))
{
}

KdeIdle::~KdeIdle() = default;

const struct org_kde_kwin_idle_timeout_interface IdleTimeout::Private::s_interface = {
    destroyCallback,
    simulateUserActivityCallback,
};

IdleTimeout::Private::Private(Client* client,
                              uint32_t version,
                              uint32_t id,
                              std::chrono::milliseconds duration,
                              Seat* seat,
                              IdleTimeout* q_ptr)
    : Wayland::Resource<IdleTimeout>(client,
                                     version,
                                     id,
                                     &org_kde_kwin_idle_timeout_interface,
                                     &s_interface,
                                     q_ptr)
    , duration{duration}
    , seat{seat}
{
}

IdleTimeout::Private::~Private() = default;

void IdleTimeout::Private::simulateUserActivityCallback([[maybe_unused]] wl_client* wlClient,
                                                        wl_resource* wlResource)
{
    Q_EMIT get_handle(wlResource)->simulate_user_activity();
}

IdleTimeout::IdleTimeout(Client* client,
                         uint32_t version,
                         uint32_t id,
                         std::chrono::milliseconds duration,
                         Seat* seat)
    : d_ptr(new Private(client, version, id, duration, seat, this))
{
}

IdleTimeout::~IdleTimeout() = default;

std::chrono::milliseconds IdleTimeout::duration() const
{
    return d_ptr->duration;
}

Seat* IdleTimeout::seat() const
{
    return d_ptr->seat;
}

void IdleTimeout::idle()
{
    d_ptr->send<org_kde_kwin_idle_timeout_send_idle>();
}

void IdleTimeout::resume()
{
    d_ptr->send<org_kde_kwin_idle_timeout_send_resumed>();
}

}
