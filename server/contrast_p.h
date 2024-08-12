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
#pragma once

#include "contrast.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QObject>
#include <QRegion>

#include <memory>
#include <wayland-contrast-server-protocol.h>

namespace Wrapland::Server
{

class Display;

constexpr uint32_t ContrastManagerVersion = 1;
using ContrastManagerGlobal = Wayland::Global<ContrastManager, ContrastManagerVersion>;

class ContrastManager::Private : public ContrastManagerGlobal
{
public:
    Private(Display* display, ContrastManager* q_ptr);

private:
    static void
    createCallback(ContrastManagerGlobal::bind_t* bind, uint32_t id, wl_resource* wlSurface);
    static void unsetCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlSurface);

    static const struct org_kde_kwin_contrast_manager_interface s_interface;
};

class Contrast::Private : public Wayland::Resource<Contrast>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Contrast* q_ptr);
    ~Private() override;

    QRegion pendingRegion;
    QRegion currentRegion;
    qreal pendingContrast;
    qreal currentContrast;
    qreal pendingIntensity;
    qreal currentIntensity;
    qreal pendingSaturation;
    qreal currentSaturation;

private:
    void commit();

    static void commitCallback(wl_client* wlClient, wl_resource* wlResource);
    static void
    setRegionCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlRegion);
    static void
    setContrastCallback(wl_client* wlClient, wl_resource* wlResource, wl_fixed_t contrast);
    static void
    setIntensityCallback(wl_client* wlClient, wl_resource* wlResource, wl_fixed_t intensity);
    static void
    setSaturationCallback(wl_client* wlClient, wl_resource* wlResource, wl_fixed_t saturation);

    static const struct org_kde_kwin_contrast_interface s_interface;
};

}
