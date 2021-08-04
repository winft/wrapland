/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "display.h"
#include "seat.h"
#include "seat_p.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

template<typename Global>
void create_selection_source([[maybe_unused]] wl_client* wlClient,
                             wl_resource* wlResource,
                             uint32_t id)
{
    if (auto handle = Global::handle(wlResource)) {
        auto priv = handle->d_ptr.get();
        auto bind = priv->getBind(wlResource);

        using source_t = typename std::remove_pointer_t<decltype(handle)>::source_t;
        auto source = new source_t(bind->client()->handle(), bind->version(), id);
        if (!source) {
            return;
        }

        Q_EMIT priv->handle()->sourceCreated(source);
    }
}

template<typename Global>
void get_selection_device([[maybe_unused]] wl_client* wlClient,
                          wl_resource* wlResource,
                          uint32_t id,
                          wl_resource* wlSeat)
{
    if (auto handle = Global::handle(wlResource)) {
        auto priv = handle->d_ptr.get();
        auto bind = priv->getBind(wlResource);
        auto seat = SeatGlobal::handle(wlSeat);

        using Device = typename std::remove_pointer_t<decltype(handle)>::device_t;
        auto device = new Device(bind->client()->handle(), bind->version(), id, seat);
        if (!device) {
            return;
        }

        seat->d_ptr->register_device(device);
        Q_EMIT priv->handle()->deviceCreated(device);
    }
}

}
