/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "seat.h"
#include "seat_p.h"
#include "wayland/bind.h"
#include "wayland/global.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

template<typename Global>
class device_manager : public Global
{
public:
    using Bind = Wayland::Bind<Global>;

    using Global::Global;

    static void get_device(Bind* bind, uint32_t id, wl_resource* wlSeat);
    static void create_source(Bind* bind, uint32_t id);
};

template<typename Global>
void device_manager<Global>::get_device(Bind* bind, uint32_t id, wl_resource* wlSeat)
{
    auto seat = SeatGlobal::handle(wlSeat);
    bind->global()->handle()->get_device(bind->client()->handle(), bind->version(), id, seat);
}

template<typename Global>
void device_manager<Global>::create_source(Bind* bind, uint32_t id)
{
    bind->global()->handle()->create_source(bind->client()->handle(), bind->version(), id);
}

}
