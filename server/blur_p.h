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

#include "blur.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QObject>

#include <wayland-blur-server-protocol.h>

namespace Wrapland::Server
{

class Display;

constexpr uint32_t BlurManagerVersion = 1;
using BlurManagerGlobal = Wayland::Global<BlurManager, BlurManagerVersion>;

class BlurManager::Private : public BlurManagerGlobal
{
public:
    Private(Display* display, BlurManager* qptr);
    ~Private() override;

private:
    void
    createBlur(wl_client* wlClient, wl_resource* wlResource, uint32_t id, wl_resource* wlSurface);

    static void createCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               uint32_t id,
                               wl_resource* wlSurface);
    static void unsetCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlSurface);

    static const struct org_kde_kwin_blur_manager_interface s_interface;
};

class Blur::Private : public Wayland::Resource<Blur>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Blur* qptr);
    ~Private() override;

    QRegion pendingRegion;
    QRegion currentRegion;

private:
    void commit();

    static void commitCallback(wl_client* wlClient, wl_resource* wlResource);
    static void
    setRegionCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlRegion);

    static const struct org_kde_kwin_blur_interface s_interface;
};

}
