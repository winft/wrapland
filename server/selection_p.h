/*
    SPDX-FileCopyrightText: 2020, 2021 Roman Gilg <subdiff@gmail.com>
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

template<typename Handle, typename Priv>
void offer_mime_type(Handle handle, Priv priv, char const* mimeType)
{
    priv->mimeTypes.push_back(mimeType);
    Q_EMIT handle->mimeTypeOffered(mimeType);
}

template<typename Source>
void receive_mime_type_offer(Source source, char const* mimeType, int32_t fd)
{
    if (!source) {
        close(fd);
        return;
    }
    source->requestData(mimeType, fd);
}

template<typename Handle, typename Priv>
void set_selection(Handle handle, Priv priv, wl_resource* wlSource)
{
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

    if (priv->selection == source) {
        return;
    }

    QObject::disconnect(priv->selectionDestroyedConnection);

    if (priv->selection) {
        priv->selection->cancel();
    }

    priv->selection = source;

    if (priv->selection) {
        auto clearSelection = [handle, priv] { set_selection(handle, priv, nullptr); };
        priv->selectionDestroyedConnection = QObject::connect(
            priv->selection, &source_type::resourceDestroyed, handle, clearSelection);
        Q_EMIT handle->selectionChanged(priv->selection);
    } else {
        priv->selectionDestroyedConnection = QMetaObject::Connection();
        Q_EMIT handle->selectionCleared();
    }
}

}
