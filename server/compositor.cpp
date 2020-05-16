/********************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

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
#include "compositor.h"

#include "display.h"
#include "region.h"
#include "surface.h"

#include "wayland/global.h"

namespace Wrapland::Server
{

constexpr uint32_t CompositorVersion = 4;
using CompositorGlobal = Wayland::Global<Compositor, CompositorVersion>;

class Compositor::Private : public CompositorGlobal
{
public:
    Private(Compositor* q, D_isplay* display);
    ~Private() override;

private:
    static void createSurfaceCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);
    static void createRegionCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);

    static const struct wl_compositor_interface s_interface;
};

Compositor::Private::Private(Compositor* q, D_isplay* display)
    : CompositorGlobal(q, display, &wl_compositor_interface, &s_interface)
{
}

Compositor::Private::~Private() = default;

const struct wl_compositor_interface Compositor::Private::s_interface = {
    createSurfaceCallback,
    createRegionCallback,
};

Compositor::Compositor(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

Compositor::~Compositor() = default;

void Compositor::Private::createSurfaceCallback(wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t id)
{
    auto compositor = handle(wlResource);
    auto client = compositor->d_ptr->display()->handle()->getClient(wlClient);

    auto surface = new Surface(client, compositor->d_ptr->version(), id);
    // TODO: error handling (when resource not created)

    Q_EMIT compositor->surfaceCreated(surface);
}

void Compositor::Private::createRegionCallback(wl_client* wlClient,
                                               wl_resource* wlResource,
                                               uint32_t id)
{
    auto compositor = handle(wlResource);
    auto client = compositor->d_ptr->display()->handle()->getClient(wlClient);

    auto region = new Region(client, compositor->d_ptr->version(), id);
    // TODO: error handling (when resource not created)

    Q_EMIT compositor->regionCreated(region);
}

}
