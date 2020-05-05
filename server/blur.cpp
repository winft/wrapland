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
#include <QRegion>

#include "wayland/client.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "blur_p.h"
#include "client.h"
#include "display.h"
#include "region.h"
#include "surface.h"
#include "surface_p.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_kwin_blur_manager_interface BlurManager::Private::s_interface = {
    createCallback,
    unsetCallback,
};

BlurManager::Private::Private(D_isplay* display, BlurManager* qptr)
    : BlurManagerGlobal(qptr, display, &org_kde_kwin_blur_manager_interface, &s_interface)
{
    create();
}

BlurManager::Private::~Private() = default;

void BlurManager::Private::createCallback(wl_client* wlClient,
                                          wl_resource* wlResource,
                                          uint32_t id,
                                          wl_resource* wlSurface)
{
    auto manager = fromResource(wlResource);
    manager->d_ptr->createBlur(wlClient, wlResource, id, wlSurface);
}

void BlurManager::Private::createBlur([[maybe_unused]] wl_client* wlClient,
                                      wl_resource* wlResource,
                                      uint32_t id,
                                      wl_resource* wlSurface)
{
    auto bind = fromResource(wlResource)->d_ptr->getBind(wlResource);
    auto surface = Wayland::Resource<Surface>::fromResource(wlSurface)->handle();

    auto blur = new Blur(bind->client()->handle(), bind->version(), id);
    if (!blur->d_ptr->resource()) {
        wl_resource_post_no_memory(wlResource);
        delete blur;
        return;
    }
    surface->d_ptr->setBlur(QPointer<Blur>(blur));
}

void BlurManager::Private::unsetCallback([[maybe_unused]] wl_client* wlClient,
                                         [[maybe_unused]] wl_resource* wlResource,
                                         wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::fromResource(wlSurface)->handle();
    surface->d_ptr->setBlur(QPointer<Blur>());
}

BlurManager::BlurManager(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

BlurManager::~BlurManager() = default;

const struct org_kde_kwin_blur_interface Blur::Private::s_interface = {
    commitCallback,
    setRegionCallback,
    destroyCallback,
};

Blur::Private::Private(Client* client, uint32_t version, uint32_t id, Blur* qptr)
    : Wayland::Resource<Blur>(client, version, id, &org_kde_kwin_blur_interface, &s_interface, qptr)
{
}

Blur::Private::~Private() = default;

void Blur::Private::commitCallback([[maybe_unused]] wl_client* wlClient, wl_resource* wlResource)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->commit();
}

void Blur::Private::commit()
{
    currentRegion = pendingRegion;
}

void Blur::Private::setRegionCallback([[maybe_unused]] wl_client* wlClient,
                                      wl_resource* wlResource,
                                      wl_resource* wlRegion)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    auto region = Wayland::Resource<Region>::fromResource(wlRegion)->handle();
    if (region) {
        priv->pendingRegion = region->region();
    } else {
        priv->pendingRegion = QRegion();
    }
}

Blur::Blur(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}

Blur::~Blur() = default;

QRegion Blur::region()
{
    return d_ptr->currentRegion;
}
}
