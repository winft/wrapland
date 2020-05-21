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
#include "display.h"
#include "egl_stream_controller_p.h"
#include "logging.h"

#include "surface.h"

#include <QLibrary>
#include <wayland-util.h>

namespace Wrapland::Server
{

const struct wl_eglstream_controller_interface EglStreamController::Private::s_interface = {
    attachStreamConsumer,
    attachStreamConsumerAttribs,
};

EglStreamController::Private::Private(Display* display,
                                      const wl_interface* interface,
                                      EglStreamController* q)
    : EglStreamControllerGlobal(q, display, interface, &s_interface)
{
    create();
}

void EglStreamController::Private::attachStreamConsumer(wl_client* wlClient,
                                                        wl_resource* wlResource,
                                                        wl_resource* wlSurface,
                                                        wl_resource* eglStream)
{
    wl_array noAttribs = {0, 0, nullptr};
    attachStreamConsumerAttribs(wlClient, wlResource, wlSurface, eglStream, &noAttribs);
}

void EglStreamController::Private::attachStreamConsumerAttribs([[maybe_unused]] wl_client* wlClient,
                                                               wl_resource* wlResource,
                                                               wl_resource* wlSurface,
                                                               wl_resource* eglStream,
                                                               wl_array* attribs)
{
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);
    Q_EMIT handle(wlResource)->streamConsumerAttached(surface, eglStream, attribs);
}

EglStreamController::EglStreamController(Display* display, QObject* parent)
    : QObject(parent)
{
    // libnvidia-egl-wayland.so.1 may not be present on all systems, so we load it dynamically
    // TODO(romangg): Cast from QFunctionPointer changes alignment from 4 to 8. Disable the check
    //                for now since it is legacy code and a solution is not directly apparent. But
    //                can we make this clang-tidy check pass somehow?
    // NOLINTNEXTLINE(clang-diagnostic-cast-align)
    auto interface = reinterpret_cast<wl_interface*>(QLibrary::resolve(
        QLatin1String("libnvidia-egl-wayland.so.1"), "wl_eglstream_controller_interface"));

    if (interface == nullptr) {
        qCWarning(WRAPLAND_SERVER, "failed to resolve wl_eglstream_controller_interface");
        return;
    }

    d_ptr.reset(new Private(display, interface, this));
}

EglStreamController::~EglStreamController() = default;

}
