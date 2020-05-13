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
#pragma once

#include "viewporter.h"

#include "wayland/client.h"
#include "wayland/display.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "viewporter.h"

#include <wayland-viewporter-server-protocol.h>

namespace Wrapland::Server
{

class D_isplay;
class Surface;
class Viewport;

class Viewporter::Private : public Wayland::Global<Viewporter>
{
public:
    Private(D_isplay* display, Viewporter* qptr);
    ~Private() override;

private:
    void getViewport(wl_resource* wlResource, uint32_t id, wl_resource* wlSurface);

    static void getViewportCallback(wl_client* wlClient,
                                    wl_resource* wlResource,
                                    uint32_t id,
                                    wl_resource* wlSurface);

    static const struct wp_viewporter_interface s_interface;
};

class Viewport::Private : public Wayland::Resource<Viewport>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Surface* _surface, Viewport* q);
    ~Private() override;

    Surface* surface;

private:
    void setSource(double x, double y, double width, double height);
    void setDestination(int width, int height);

    static void setSourceCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  wl_fixed_t x,
                                  wl_fixed_t y,
                                  wl_fixed_t width,
                                  wl_fixed_t height);
    static void setDestinationCallback(wl_client* wlClient,
                                       wl_resource* wlResource,
                                       int32_t width,
                                       int32_t height);

    static const struct wp_viewport_interface s_interface;
};

}
