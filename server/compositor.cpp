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

namespace Wrapland
{
namespace Server
{

class Compositor::Private : public Wayland::Global<Compositor>
{
public:
    Private(Compositor* q, D_isplay* display);
    ~Private() override;

    uint32_t version() const override;

private:
    static void createSurfaceCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);
    static void createRegionCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);

    static const struct wl_compositor_interface s_interface;
    static const quint32 s_version;
};

const uint32_t Compositor::Private::s_version = 4;

Compositor::Private::Private(Compositor* q, D_isplay* display)
    : Wayland::Global<Compositor>(q, display, &wl_compositor_interface, &s_interface)
{
}

Compositor::Private::~Private() = default;

const struct wl_compositor_interface Compositor::Private::s_interface = {
    createSurfaceCallback,
    createRegionCallback,
};

uint32_t Compositor::Private::version() const
{
    return s_version;
}

Compositor::Compositor(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

Compositor::~Compositor()
{
    delete d_ptr;
}

void Compositor::Private::createSurfaceCallback(wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t id)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);

    auto surface = new Surface(client, handle->d_ptr->version(), id);
    // TODO: error handling (when resource not created)

    Q_EMIT handle->surfaceCreated(surface);
}

void Compositor::Private::createRegionCallback(wl_client* wlClient,
                                               wl_resource* wlResource,
                                               uint32_t id)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);

    auto region = new Region(client, handle->d_ptr->version(), id);
    // TODO: error handling (when resource not created)

    Q_EMIT handle->regionCreated(region);
}

}
}
