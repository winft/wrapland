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

template<typename Source>
void offer_mime_type(Source src_priv, char const* mimeType)
{
    src_priv->mimeTypes.push_back(mimeType);
    Q_EMIT src_priv->q_ptr->mime_type_offered(mimeType);
}

template<typename Source>
void receive_mime_type_offer(Source source, char const* mimeType, int32_t fd)
{
    if (!source) {
        close(fd);
        return;
    }
    source->request_data(mimeType, fd);
}

template<typename Handle, typename Priv>
void set_selection(Handle handle, Priv priv, wl_resource* wlSource)
{
    using source_res_type = typename std::remove_pointer_t<decltype(priv)>::source_res_t;
    auto source_res = wlSource ? Wayland::Resource<source_res_type>::handle(wlSource) : nullptr;
    auto source = source_res ? source_res->src() : nullptr;

    if (priv->selection == source) {
        return;
    }

    QObject::disconnect(priv->selectionDestroyedConnection);

    if (priv->selection) {
        priv->selection->cancel();
    }

    priv->selection = source;

    if (source) {
        auto clearSelection = [handle, priv] { set_selection(handle, priv, nullptr); };
        priv->selectionDestroyedConnection = QObject::connect(
            source_res, &source_res_type::resourceDestroyed, handle, clearSelection);
    } else {
        priv->selectionDestroyedConnection = QMetaObject::Connection();
    }
    Q_EMIT handle->selection_changed();
}

}
