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
#include "viewporter_interface.h"

#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "surface_interface_p.h"

#include <wayland-server.h>
#include <wayland-viewporter-server-protocol.h>

namespace Wrapland
{

namespace Server
{

class ViewporterInterface::Private : public Global::Private
{
public:
    Private(ViewporterInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void getViewport(wl_client *client, wl_resource *resource, uint32_t id,
                     wl_resource *surface);

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void getViewportCallback(wl_client *client, wl_resource *resource, uint32_t id,
                                    wl_resource *surface);
    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        auto viewporter = reinterpret_cast<QPointer<ViewporterInterface>*>(
                              wl_resource_get_user_data(r))->data();
        if (viewporter) {
            return static_cast<Private*>(viewporter->d.data());
         }
        return nullptr;
    }

    ViewporterInterface *q;
    static const struct wp_viewporter_interface s_interface;
    static const quint32 s_version;
};

const quint32 ViewporterInterface::Private::s_version = 1;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct wp_viewporter_interface ViewporterInterface::Private::s_interface = {
    destroyCallback,
    getViewportCallback
};
#endif

ViewporterInterface::Private::Private(ViewporterInterface *q, Display *d)
    : Global::Private(d, &wp_viewporter_interface, s_version)
    , q(q)
{
}

void ViewporterInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&wp_viewporter_interface, qMin(version, s_version),
                                              id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    auto ref = new QPointer<ViewporterInterface>(q); // Is deleted in unbind.
    wl_resource_set_implementation(resource, &s_interface, ref, unbind);
}

void ViewporterInterface::Private::unbind(wl_resource *resource)
{
    delete reinterpret_cast<QPointer<ViewporterInterface>*>(wl_resource_get_user_data(resource));
}

void ViewporterInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    unbind(resource);
}

void ViewporterInterface::Private::getViewportCallback(wl_client *client, wl_resource *resource,
                                                  uint32_t id, wl_resource *surface)
{
    auto *privateInstance = cast(resource);
    if (!privateInstance) {
        // When global is deleted.
        return;
    }
    privateInstance->getViewport(client, resource, id, surface);
}

void ViewporterInterface::Private::getViewport(wl_client *client, wl_resource *resource,
                                               uint32_t id, wl_resource *surface)
{
    auto *surfaceInterface = SurfaceInterface::get(surface);
    if (!surfaceInterface) {
        // TODO: send error msg?
        return;
    }

    if (!surfaceInterface->d_func()->viewport.isNull()) {
        // Surface already has a viewport. That's a protocol error.
        wl_resource_post_error(resource, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
                               "Surface already has viewport");
        return;
    }

    ViewportInterface *viewport = new ViewportInterface(q, resource, surfaceInterface);
    viewport->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!viewport->resource()) {
        wl_resource_post_no_memory(resource);
        delete viewport;
        return;
    }
    surfaceInterface->d_func()->installViewport(viewport);

    Q_EMIT q->viewportCreated(viewport);
}

ViewporterInterface::ViewporterInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

ViewporterInterface::~ViewporterInterface() = default;

class ViewportInterface::Private : public Resource::Private
{
public:
    Private(ViewportInterface *q, ViewporterInterface *viewporter, wl_resource *parentResource,
            SurfaceInterface *_surface);
    ~Private() override;

    SurfaceInterface *surface;

private:
    ViewportInterface *q_func() {
        return reinterpret_cast<ViewportInterface *>(q);
    }

    void setSource(double x, double y, double width, double height);
    void setDestination(int width, int height);

    static void setSourceCallback(wl_client *client, wl_resource *resource,
                                  wl_fixed_t x, wl_fixed_t y,
                                  wl_fixed_t width, wl_fixed_t height);
    static void setDestinationCallback(wl_client *client, wl_resource *resource,
                                       int32_t width, int32_t height);

    static const struct wp_viewport_interface s_interface;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct wp_viewport_interface ViewportInterface::Private::s_interface = {
    resourceDestroyedCallback,
    setSourceCallback,
    setDestinationCallback
};
#endif

void ViewportInterface::Private::setSourceCallback(wl_client *client, wl_resource *resource,
                                                   wl_fixed_t x, wl_fixed_t y,
                                                   wl_fixed_t width, wl_fixed_t height)
{
    Q_UNUSED(client)
    cast<Private>(resource)->setSource(wl_fixed_to_double(x), wl_fixed_to_double(y),
                                       wl_fixed_to_double(width), wl_fixed_to_double(height));
}

void ViewportInterface::Private::setSource(double x, double y, double width, double height)
{
    if (!surface) {
        wl_resource_post_error(parentResource, WP_VIEWPORT_ERROR_NO_SURFACE,
                               "Viewport without surface");
        return;
    }
    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        auto cmp = [](double number) { return !qFuzzyCompare(number, -1.); };
        if (cmp(x) || cmp(y) || cmp(width) || cmp(height)) {
            wl_resource_post_error(parentResource, WP_VIEWPORT_ERROR_BAD_VALUE,
                                   "Source rectangle not well defined");
            return;
        }
    }
    Q_EMIT q_func()->sourceRectangleSet(QRectF(x, y, width, height));
}

void ViewportInterface::Private::setDestinationCallback(wl_client *client, wl_resource *resource,
                                                        int32_t width, int32_t height)
{
    Q_UNUSED(client)
    cast<Private>(resource)->setDestination(width, height);
}

void ViewportInterface::Private::setDestination(int width, int height)
{
    if (!surface) {
        wl_resource_post_error(parentResource, WP_VIEWPORT_ERROR_NO_SURFACE,
                               "Viewport without surface");
        return;
    }
    if ((width <= 0 && width != -1) || (height <= 0 && height != -1)) {
        wl_resource_post_error(parentResource, WP_VIEWPORT_ERROR_BAD_VALUE,
                               "Destination size not well defined");
        return;
    }
    Q_EMIT q_func()->destinationSizeSet(QSize(width, height));
}

ViewportInterface::Private::Private(ViewportInterface *q, ViewporterInterface *viewporter,
                                    wl_resource *parentResource, SurfaceInterface *_surface)
    : Resource::Private(q, viewporter, parentResource, &wp_viewport_interface, &s_interface)
    , surface(_surface)
{
}

ViewportInterface::Private::~Private() = default;

ViewportInterface::ViewportInterface(ViewporterInterface *viewporter, wl_resource *parentResource,
                                     SurfaceInterface *surface)
    : Resource(new Private(this, viewporter, parentResource, surface))
{
    connect(surface, &SurfaceInterface::unbound, this, [this] {
        d_func()->surface = nullptr;
    });
}

ViewportInterface::~ViewportInterface() = default;

ViewportInterface::Private *ViewportInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
