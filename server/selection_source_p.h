/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "wayland/resource.h"

#include <QObject>

namespace Wrapland::Server
{

template<typename Resource>
void add_offered_mime_type([[maybe_unused]] wl_client* wlClient,
                           wl_resource* wlResource,
                           char const* mimeType)
{
    auto handle = Resource::handle(wlResource);
    handle->d_ptr->mimeTypes.push_back(mimeType);
    Q_EMIT handle->mimeTypeOffered(mimeType);
}

}
