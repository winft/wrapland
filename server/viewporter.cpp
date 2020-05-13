/********************************************************************
Copyright © 2020 Adrien Faveraux <ad1rie3@hotmail.fr>

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

#include "display.h"
#include "wayland/client.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "surface.h"
#include "surface_p.h"
#include "viewporter_p.h"

#include <wayland-server.h>
#include <wayland-viewporter-server-protocol.h>

namespace Wrapland::Server
{

const struct wp_viewporter_interface Viewporter::Private::s_interface = {
    resourceDestroyCallback,
    getViewportCallback,
};

Viewporter::Private::Private(D_isplay* display, Viewporter* qptr)
    : Wayland::Global<Viewporter>(qptr, display, &wp_viewporter_interface, &s_interface)
{
    create();
}
Viewporter::Private::~Private() = default;
void Viewporter::Private::getViewportCallback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              uint32_t id,
                                              wl_resource* wlSurface)
{
    handle(wlResource)->d_ptr->getViewport(wlResource, id, wlSurface);
}

void Viewporter::Private::getViewport(wl_resource* wlResource, uint32_t id, wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);
    auto bind = getBind(wlResource);
    if (!surface) {
        // TODO: send error msg?
        return;
    }

    if (!surface->d_ptr->viewport.isNull()) {
        // Surface already has a viewport. That's a protocol error.
        bind->postError(WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS, "Surface already has viewport");
        return;
    }

    auto viewport = new Viewport(bind->client()->handle(), bind->version(), id, surface);
    if (!viewport->d_ptr->resource()) {
        wl_resource_post_no_memory(wlResource);
        delete viewport;
        return;
    }
    surface->d_ptr->installViewport(viewport);

    Q_EMIT handle()->viewportCreated(viewport);
}

Viewporter::Viewporter(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

Viewporter::~Viewporter() = default;

const struct wp_viewport_interface Viewport::Private::s_interface = {
    destroyCallback,
    setSourceCallback,
    setDestinationCallback,
};

Viewport::Private::Private(Client* client,
                           uint32_t version,
                           uint32_t id,
                           Surface* _surface,
                           Viewport* q)
    : Wayland::Resource<Viewport>(client, version, id, &wp_viewport_interface, &s_interface, q)
    , surface(_surface)
{
}

Viewport::Private::~Private() = default;

Viewport::Viewport(Client* client, uint32_t version, uint32_t id, Surface* surface, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(client, version, id, surface, this))
{
    connect(surface, &Surface::resourceDestroyed, this, [this] { d_ptr->surface = nullptr; });
}

Viewport::~Viewport() = default;

void Viewport::Private::setSourceCallback([[maybe_unused]] wl_client* wlClient,
                                          wl_resource* wlResource,
                                          wl_fixed_t x,
                                          wl_fixed_t y,
                                          wl_fixed_t width,
                                          wl_fixed_t height)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->setSource(wl_fixed_to_double(x),
                    wl_fixed_to_double(y),
                    wl_fixed_to_double(width),
                    wl_fixed_to_double(height));
}

void Viewport::Private::setSource(double x, double y, double width, double height)
{
    if (!surface) {
        postError(WP_VIEWPORT_ERROR_NO_SURFACE, "Viewport without surface");
        return;
    }
    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        auto cmp = [](double number) { return !qFuzzyCompare(number, -1.); };
        if (cmp(x) || cmp(y) || cmp(width) || cmp(height)) {
            postError(WP_VIEWPORT_ERROR_BAD_VALUE, "Source rectangle not well defined");
            return;
        }
    }

    Q_EMIT handle()->sourceRectangleSet(QRectF(x, y, width, height));
}

void Viewport::Private::setDestinationCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               int32_t width,
                                               int32_t height)
{
    handle(wlResource)->d_ptr->setDestination(width, height);
}

void Viewport::Private::setDestination(int width, int height)
{
    if (!surface) {
        postError(WP_VIEWPORT_ERROR_NO_SURFACE, "Viewport without surface");
        return;
    }
    if ((width <= 0 && width != -1) || (height <= 0 && height != -1)) {
        postError(WP_VIEWPORT_ERROR_BAD_VALUE, "Destination size not well defined");
        return;
    }

    Q_EMIT handle()->destinationSizeSet(QSize(width, height));
}

}
