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
#pragma once

#include "slide.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-slide-server-protocol.h>

namespace Wrapland::Server
{

class Display;

constexpr uint32_t SlideManagerVersion = 1;
using SlideManagerGlobal = Wayland::Global<SlideManager, SlideManagerVersion>;

class SlideManager::Private : public SlideManagerGlobal
{
public:
    Private(Display* display, SlideManager* qptr);

private:
    static void createCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               uint32_t id,
                               wl_resource* wlSurface);
    static void unsetCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlSurface);

    static const struct org_kde_kwin_slide_manager_interface s_interface;
};

class Slide::Private : public Wayland::Resource<Slide>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Slide* qptr);

    Slide::Location pendingLocation;
    Slide::Location currentLocation;
    uint32_t pendingOffset;
    uint32_t currentOffset;

private:
    static void commitCallback(wl_client* wlClient, wl_resource* wlResource);
    static void
    setLocationCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t location);
    static void setOffsetCallback(wl_client* wlClient, wl_resource* wlResource, int32_t offset);

    static const struct org_kde_kwin_slide_interface s_interface;
};
}
