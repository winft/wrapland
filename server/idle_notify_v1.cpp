/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "display.h"
#include "idle_notify_v1_p.h"
#include "seat_p.h"

namespace Wrapland::Server
{

const struct ext_idle_notifier_v1_interface idle_notifier_v1::Private::s_interface = {
    resourceDestroyCallback,
    cb<get_idle_notification_callback>,
};

idle_notifier_v1::Private::Private(Display* display, idle_notifier_v1* q_ptr)
    : idle_notifier_v1_global(q_ptr, display, &ext_idle_notifier_v1_interface, &s_interface)
{
    create();
}

void idle_notifier_v1::Private::get_idle_notification_callback(
    idle_notifier_v1_global::bind_t* bind,
    uint32_t id,
    uint32_t timeout,
    wl_resource* wlSeat)
{
    auto priv = bind->global()->handle->d_ptr.get();
    auto seat = SeatGlobal::get_handle(wlSeat);

    auto duration = std::max(std::chrono::milliseconds(timeout), std::chrono::milliseconds::zero());

    auto notification
        = new idle_notification_v1(bind->client->handle, bind->version, id, duration, seat);
    if (!notification->d_ptr->resource) {
        bind->post_no_memory();
        delete notification;
        return;
    }

    Q_EMIT priv->handle->notification_created(notification);
}

idle_notifier_v1::idle_notifier_v1(Display* display)
    : d_ptr(new Private(display, this))
{
}

idle_notifier_v1::~idle_notifier_v1() = default;

const struct ext_idle_notification_v1_interface idle_notification_v1::Private::s_interface = {
    destroyCallback,
};

idle_notification_v1::Private::Private(Client* client,
                                       uint32_t version,
                                       uint32_t id,
                                       std::chrono::milliseconds duration,
                                       Seat* seat,
                                       idle_notification_v1* q_ptr)
    : Wayland::Resource<idle_notification_v1>(client,
                                              version,
                                              id,
                                              &ext_idle_notification_v1_interface,
                                              &s_interface,
                                              q_ptr)
    , duration{duration}
    , seat{seat}
{
}

idle_notification_v1::Private::~Private() = default;

idle_notification_v1::idle_notification_v1(Client* client,
                                           uint32_t version,
                                           uint32_t id,
                                           std::chrono::milliseconds duration,
                                           Seat* seat)
    : d_ptr(new Private(client, version, id, duration, seat, this))
{
}

idle_notification_v1::~idle_notification_v1() = default;

std::chrono::milliseconds idle_notification_v1::duration() const
{
    return d_ptr->duration;
}

Seat* idle_notification_v1::seat() const
{
    return d_ptr->seat;
}

void idle_notification_v1::idle()
{
    d_ptr->send<ext_idle_notification_v1_send_idled>();
}

void idle_notification_v1::resume()
{
    d_ptr->send<ext_idle_notification_v1_send_resumed>();
}

}
