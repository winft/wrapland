/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "seat.h"
#include "seat_p.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <unistd.h>

namespace Wrapland::Server
{

template<typename Resource>
void receive_selection_offer([[maybe_unused]] wl_client* wlClient,
                             wl_resource* wlResource,
                             char const* mimeType,
                             int32_t fd)
{
    auto handle = Resource::handle(wlResource);
    if (auto source = handle->d_ptr->source) {
        source->requestData(mimeType, fd);
    } else {
        close(fd);
    }
}

}
