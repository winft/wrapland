/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

namespace Wrapland::Server
{

template<typename Handle, typename Priv>
void offer_mime_type(Handle handle, Priv priv, char const* mimeType)
{
    priv->mimeTypes.push_back(mimeType);
    Q_EMIT handle->mimeTypeOffered(mimeType);
}

}
