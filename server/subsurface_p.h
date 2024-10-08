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
#pragma once

#include "subcompositor.h"
#include "surface_p.h"

#include "wayland/resource.h"

#include <QPoint>

#include <wayland-server.h>

namespace Wrapland::Server
{

class Subsurface::Private : public Wayland::Resource<Subsurface>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Surface* surface,
            Surface* parent,
            Subsurface* q_ptr);

    /**
     * Initializes the subsurface in relation to its parent. Needs to be after the subsurface
     * is created already since we access its data from outside.
     */
    void init();

    void applyCached(bool force);
    void commit();

    QPoint pos = QPoint(0, 0);
    QPoint scheduledPos;
    bool scheduledPosChange = false;
    Mode mode = Mode::Synchronized;

    Surface* surface = nullptr;
    Surface* parent = nullptr;
    SurfaceState cached;

private:
    void setMode(Mode mode);
    void setPosition(QPoint const& pos);
    void placeAbove(Surface* sibling);
    void placeBelow(Surface* sibling);

    static void
    setPositionCallback(wl_client* wlClient, wl_resource* wlResource, int32_t pos_x, int32_t pos_y);
    static void
    placeAboveCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlSibling);
    static void
    placeBelowCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlSibling);
    static void setSyncCallback(wl_client* wlClient, wl_resource* wlResource);
    static void setDeSyncCallback(wl_client* wlClient, wl_resource* wlResource);

    static const struct wl_subsurface_interface s_interface;
};

}
