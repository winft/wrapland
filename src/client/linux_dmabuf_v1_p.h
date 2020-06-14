/********************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#pragma once

#include "linux_dmabuf_v1.h"
#include "wayland_pointer_p.h"

#include <QHash>
#include <QObject>
#include <QPoint>
#include <QSize>

// STD
#include <memory>

// wayland
#include <wayland-linux-dmabuf-unstable-v1-client-protocol.h>

struct wl_buffer;
struct zwp_linux_dmabuf_v1;
struct zwp_linux_buffer_params_v1;

namespace Wrapland::Client
{

class Q_DECL_HIDDEN LinuxDmabufV1::Private
{
public:
    Private(LinuxDmabufV1* q);

    QHash<uint32_t, LinuxDmabufV1::Modifier> modifiers;

    static void
    callbackFormat(void* data, zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1, uint32_t format);
    static void callbackModifier(void* data,
                                 zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
                                 uint32_t format,
                                 uint32_t modifier_hi,
                                 uint32_t modifier_lo);

    WaylandPointer<zwp_linux_dmabuf_v1, zwp_linux_dmabuf_v1_destroy> dmabuf;
    EventQueue* queue = nullptr;
    LinuxDmabufV1* q_ptr;
    static const zwp_linux_dmabuf_v1_listener s_listener;
};

class Q_DECL_HIDDEN ParamsV1::Private
{
public:
    Private(ParamsV1* q);

    WaylandPointer<zwp_linux_buffer_params_v1, zwp_linux_buffer_params_v1_destroy> params;

    static void callbackCreateSucceeded(void* data,
                                        zwp_linux_buffer_params_v1* wlParams,
                                        wl_buffer* wlBuffer);
    static void callbackBufferCreationFail(void* data,
                                           zwp_linux_buffer_params_v1* wlParams);

    static const zwp_linux_buffer_params_v1_listener s_listener;
    wl_buffer* createdBuffer = nullptr;
    ParamsV1* q_ptr;
};

}
