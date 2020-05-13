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
#include "contrast.h"
#include "contrast_p.h"

#include "wayland/client.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "client.h"
#include "display.h"
#include "region.h"
#include "surface.h"
#include "surface_p.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_kwin_contrast_manager_interface ContrastManager::Private::s_interface = {
    createCallback,
    unsetCallback,
};

ContrastManager::Private::Private(D_isplay* display, ContrastManager* q)
    : ContrastManagerGlobal(q, display, &org_kde_kwin_contrast_manager_interface, &s_interface)
{
    create();
}

ContrastManager::Private::~Private() = default;

void ContrastManager::Private::createCallback(wl_client* wlClient,
                                              wl_resource* wlResource,
                                              uint32_t id,
                                              wl_resource* wlSurface)
{
    auto manager = handle(wlResource);
    manager->d_ptr->createContrast(wlClient, wlResource, id, wlSurface);
}

void ContrastManager::Private::createContrast([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              uint32_t id,
                                              wl_resource* wlSurface)
{
    auto bind = handle(wlResource)->d_ptr->getBind(wlResource);
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);

    auto contrast = new Contrast(bind->client()->handle(), bind->version(), id);
    if (!contrast->d_ptr->resource()) {
        wl_resource_post_no_memory(wlResource);
        delete contrast;
        return;
    }

    surface->d_ptr->setContrast(QPointer<Contrast>(contrast));
}

void ContrastManager::Private::unsetCallback([[maybe_unused]] wl_client* wlClient,
                                             [[maybe_unused]] wl_resource* wlResource,
                                             wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);
    surface->d_ptr->setContrast(QPointer<Contrast>());
}

ContrastManager::ContrastManager(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

ContrastManager::~ContrastManager() = default;

const struct org_kde_kwin_contrast_interface Contrast::Private::s_interface = {
    commitCallback,
    setRegionCallback,
    setContrastCallback,
    setIntensityCallback,
    setSaturationCallback,
    destroyCallback,
};

Contrast::Private::Private(Client* client, uint32_t version, uint32_t id, Contrast* q)
    : Wayland::Resource<Contrast>(client,
                                  version,
                                  id,
                                  &org_kde_kwin_contrast_interface,
                                  &s_interface,
                                  q)
{
}

Contrast::Private::~Private() = default;

void Contrast::Private::commitCallback([[maybe_unused]] wl_client* wlClient,
                                       wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->commit();
}

void Contrast::Private::commit()
{
    currentRegion = pendingRegion;
    currentContrast = pendingContrast;
    currentIntensity = pendingIntensity;
    currentSaturation = pendingSaturation;
}

void Contrast::Private::setRegionCallback([[maybe_unused]] wl_client* wlClient,
                                          wl_resource* wlResource,
                                          wl_resource* wlRegion)
{
    auto priv = handle(wlResource)->d_ptr;
    auto region = Wayland::Resource<Region>::handle(wlRegion);

    priv->pendingRegion = region ? region->region() : QRegion();
}

void Contrast::Private::setContrastCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            wl_fixed_t contrast)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pendingContrast = wl_fixed_to_double(contrast);
}

void Contrast::Private::setIntensityCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             wl_fixed_t intensity)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pendingIntensity = wl_fixed_to_double(intensity);
}

void Contrast::Private::setSaturationCallback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              wl_fixed_t saturation)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pendingSaturation = wl_fixed_to_double(saturation);
}

Contrast::Contrast(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}

Contrast::~Contrast() = default;

QRegion Contrast::region() const
{
    return d_ptr->currentRegion;
}

qreal Contrast::contrast() const
{
    return d_ptr->currentContrast;
}

qreal Contrast::intensity() const
{
    return d_ptr->currentIntensity;
}

qreal Contrast::saturation() const
{
    return d_ptr->currentSaturation;
}
}
