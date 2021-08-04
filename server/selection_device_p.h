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

namespace Wrapland::Server
{
class DataSource;

template<typename Resource>
void set_selection_callback([[maybe_unused]] wl_client* wlClient,
                            wl_resource* wlResource,
                            wl_resource* wlSource,
                            [[maybe_unused]] uint32_t id)
{
    if (auto handle = Resource::handle(wlResource)) {
        set_selection(handle, wlSource);
    }
}

template<typename Handle>
void set_selection(Handle* handle, wl_resource* wlSource)
{
    // TODO(unknown author): verify serial

    using source_type = typename std::remove_pointer_t<decltype(handle)>::source_t;
    auto source = wlSource ? Wayland::Resource<source_type>::handle(wlSource) : nullptr;

    if constexpr (std::is_same<source_type, DataSource>::value) {
        // TODO(romangg): move errors into Wayland namespace.
        if (source && source->supportedDragAndDropActions()
            && wl_resource_get_version(wlSource) >= WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
            wl_resource_post_error(
                wlSource, WL_DATA_SOURCE_ERROR_INVALID_SOURCE, "Data source is for drag and drop");
            return;
        }
    }

    auto priv = handle->d_ptr;

    if (priv->selection == source) {
        return;
    }

    QObject::disconnect(priv->selectionDestroyedConnection);

    if (priv->selection) {
        priv->selection->cancel();
    }

    priv->selection = source;

    if (priv->selection) {
        auto clearSelection = [handle] { set_selection(handle, nullptr); };
        priv->selectionDestroyedConnection = QObject::connect(
            priv->selection, &source_type::resourceDestroyed, handle, clearSelection);
        Q_EMIT handle->selectionChanged(priv->selection);
    } else {
        priv->selectionDestroyedConnection = QMetaObject::Connection();
        Q_EMIT handle->selectionCleared();
    }
}

}
