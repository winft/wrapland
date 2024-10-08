/****************************************************************************
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
****************************************************************************/
#include "wayland/client.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "client.h"
#include "display.h"
#include "region.h"
#include "slide_p.h"
#include "surface.h"
#include "surface_p.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_kwin_slide_manager_interface SlideManager::Private::s_interface = {
    cb<createCallback>,
    cb<unsetCallback>,
};

SlideManager::Private::Private(Display* display, SlideManager* qptr)
    : SlideManagerGlobal(qptr, display, &org_kde_kwin_slide_manager_interface, &s_interface)
{
    create();
}

void SlideManager::Private::createCallback(SlideManagerGlobal::bind_t* bind,
                                           uint32_t id,
                                           wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::get_handle(wlSurface);

    auto slide = new Slide(bind->client->handle, bind->version, id);
    if (!slide->d_ptr->resource) {
        bind->post_no_memory();
        delete slide;
        return;
    }

    surface->d_ptr->setSlide(slide);
}

void SlideManager::Private::unsetCallback([[maybe_unused]] SlideManagerGlobal::bind_t* bind,
                                          wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::get_handle(wlSurface);
    surface->d_ptr->setSlide(nullptr);
}

SlideManager::SlideManager(Display* display)
    : d_ptr(new Private(display, this))
{
}

SlideManager::~SlideManager() = default;

const struct org_kde_kwin_slide_interface Slide::Private::s_interface = {
    commitCallback,
    setLocationCallback,
    setOffsetCallback,
    destroyCallback,
};

Slide::Private::Private(Client* client, uint32_t version, uint32_t id, Slide* qptr)
    : Wayland::Resource<Slide>(client,
                               version,
                               id,
                               &org_kde_kwin_slide_interface,
                               &s_interface,
                               qptr)
{
}

void Slide::Private::commitCallback([[maybe_unused]] wl_client* wlClient, wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->currentLocation = priv->pendingLocation;
    priv->currentOffset = priv->pendingOffset;
}

void Slide::Private::setLocationCallback([[maybe_unused]] wl_client* wlClient,
                                         wl_resource* wlResource,
                                         uint32_t location)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->pendingLocation = static_cast<Slide::Location>(location);
}

void Slide::Private::setOffsetCallback([[maybe_unused]] wl_client* wlClient,
                                       wl_resource* wlResource,
                                       int32_t offset)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->pendingOffset = offset;
}

Slide::Slide(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}

Slide::Location Slide::location() const
{
    return d_ptr->currentLocation;
}

uint32_t Slide::offset() const
{
    return d_ptr->currentOffset;
}

}
