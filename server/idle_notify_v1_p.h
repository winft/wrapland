/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "idle_notify_v1.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-ext-idle-notify-v1-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t idle_notifier_v1_version = 1;
using idle_notifier_v1_global = Wayland::Global<idle_notifier_v1, idle_notifier_v1_version>;
using idle_notifier_v1_bind = Wayland::Bind<idle_notifier_v1_global>;

class idle_notifier_v1::Private : public idle_notifier_v1_global
{
public:
    Private(Display* display, idle_notifier_v1* q_ptr);
    ~Private() override;

private:
    static void get_idle_notification_callback(idle_notifier_v1_bind* bind,
                                               uint32_t id,
                                               uint32_t timeout,
                                               wl_resource* wlSeat);

    static const struct ext_idle_notifier_v1_interface s_interface;
};

class idle_notification_v1::Private : public Wayland::Resource<idle_notification_v1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            std::chrono::milliseconds duration,
            Seat* seat,
            idle_notification_v1* q_ptr);
    ~Private() override;

    std::chrono::milliseconds duration;
    Seat* seat;

private:
    static const struct ext_idle_notification_v1_interface s_interface;
};

}
