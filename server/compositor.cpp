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

#include "client.h"
#include "display.h"
#include "region.h"
#include "surface_p.h"

#include "wayland/global.h"

#include <algorithm>
#include <vector>

namespace Wrapland::Server
{

constexpr uint32_t CompositorVersion = 4;
using CompositorGlobal = Wayland::Global<Compositor, CompositorVersion>;

class Compositor::Private : public CompositorGlobal
{
public:
    Private(Compositor* q, Display* display);
    ~Private() override;

    std::vector<Surface*> surfaces;

private:
    static void createSurfaceCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);
    static void createRegionCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);

    static const struct wl_compositor_interface s_interface;
};

Compositor::Private::Private(Compositor* q, Display* display)
    : CompositorGlobal(q, display, &wl_compositor_interface, &s_interface)
{
}

Compositor::Private::~Private() = default;

const struct wl_compositor_interface Compositor::Private::s_interface = {
    createSurfaceCallback,
    createRegionCallback,
};

Compositor::Compositor(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

Compositor::~Compositor() = default;

void Compositor::Private::createSurfaceCallback([[maybe_unused]] wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    auto surface = new Surface(bind->client()->handle(), bind->version(), id);
    // TODO(romangg): error handling (when resource not created)

    priv->surfaces.push_back(surface);
    connect(surface, &Surface::resourceDestroyed, priv->handle(), [priv, surface] {
        priv->surfaces.erase(std::remove(priv->surfaces.begin(), priv->surfaces.end(), surface),
                             priv->surfaces.end());
    });

    Q_EMIT priv->handle()->surfaceCreated(surface);
}

void Compositor::Private::createRegionCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               uint32_t id)
{
    auto compositor = handle(wlResource);
    auto bind = compositor->d_ptr->getBind(wlResource);

    auto region = new Region(bind->client()->handle(), bind->version(), id);
    // TODO(romangg): error handling (when resource not created)

    Q_EMIT compositor->regionCreated(region);
}

Surface* Compositor::getSurface(uint32_t id, Client* client)
{
    auto it = std::find_if(
        d_ptr->surfaces.cbegin(), d_ptr->surfaces.cend(), [id, client](Surface* surface) {
            return surface->d_ptr->client()->handle() == client && surface->d_ptr->id() == id;
        });
    return it != d_ptr->surfaces.cend() ? *it : nullptr;
}

}
