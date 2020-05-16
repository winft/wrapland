/****************************************************************************
Copyright 2019 NVIDIA Inc.

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
****************************************************************************/
#pragma once

#include "eglstream_controller.h"
#include "wayland/global.h"

#include <wayland-eglstream-controller-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t EglStreamControllerrVersion = 1;
using EglStreamControllerGlobal = Wayland::Global<EglStreamController, EglStreamControllerrVersion>;

class EglStreamController::Private : public EglStreamControllerGlobal
{
public:
    Private(D_isplay* display, const wl_interface* interface, EglStreamController* controller);

private:
    static void attachStreamConsumer(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     wl_resource* wlSurface,
                                     wl_resource* eglStream);

    static void attachStreamConsumerAttribs(wl_client* wlClient,
                                            wl_resource* wlResource,
                                            wl_resource* wlSurface,
                                            wl_resource* eglStream,
                                            wl_array* attribs);

    static const struct wl_eglstream_controller_interface s_interface;
};

}
