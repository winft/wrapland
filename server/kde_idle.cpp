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

const struct org_kde_kwin_idle_interface kde_idle::Private::s_interface = {
    cb<get_idle_timeout_callback>,
};

kde_idle::Private::Private(Display* display, kde_idle* q_ptr)
    : kde_idle_global(q_ptr, display, &org_kde_kwin_idle_interface, &s_interface)
{
    create();
}

void kde_idle::Private::get_idle_timeout_callback(kde_idle_bind* bind,
                                                  uint32_t id,
                                                  wl_resource* wlSeat,
                                                  uint32_t timeout)
{
    auto priv = bind->global()->handle->d_ptr.get();
    auto seat = SeatGlobal::get_handle(wlSeat);

    auto duration = std::max(std::chrono::milliseconds(timeout), std::chrono::milliseconds::zero());

    auto idle_timeout
        = new kde_idle_timeout(bind->client->handle, bind->version, id, duration, seat);
    if (!idle_timeout->d_ptr->resource) {
        bind->post_no_memory();
        delete idle_timeout;
        return;
    }

    Q_EMIT priv->handle->timeout_created(idle_timeout);
}

kde_idle::kde_idle(Display* display)
    : d_ptr(new Private(display, this))
{
}

kde_idle::~kde_idle() = default;

const struct org_kde_kwin_idle_timeout_interface kde_idle_timeout::Private::s_interface = {
    destroyCallback,
    simulate_user_activity_callback,
};

kde_idle_timeout::Private::Private(Client* client,
                                   uint32_t version,
                                   uint32_t id,
                                   std::chrono::milliseconds duration,
                                   Seat* seat,
                                   kde_idle_timeout* q_ptr)
    : Wayland::Resource<kde_idle_timeout>(client,
                                          version,
                                          id,
                                          &org_kde_kwin_idle_timeout_interface,
                                          &s_interface,
                                          q_ptr)
    , duration{duration}
    , seat{seat}
{
}

kde_idle_timeout::Private::~Private() = default;

void kde_idle_timeout::Private::simulate_user_activity_callback(wl_client* /*wlClient*/,
                                                                wl_resource* wlResource)
{
    Q_EMIT get_handle(wlResource)->simulate_user_activity();
}

kde_idle_timeout::kde_idle_timeout(Client* client,
                                   uint32_t version,
                                   uint32_t id,
                                   std::chrono::milliseconds duration,
                                   Seat* seat)
    : d_ptr(new Private(client, version, id, duration, seat, this))
{
}

kde_idle_timeout::~kde_idle_timeout() = default;

std::chrono::milliseconds kde_idle_timeout::duration() const
{
    return d_ptr->duration;
}

Seat* kde_idle_timeout::seat() const
{
    return d_ptr->seat;
}

void kde_idle_timeout::idle()
{
    d_ptr->send<org_kde_kwin_idle_timeout_send_idle>();
}

void kde_idle_timeout::resume()
{
    d_ptr->send<org_kde_kwin_idle_timeout_send_resumed>();
}

}
