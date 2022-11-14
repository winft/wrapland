/****************************************************************************
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
****************************************************************************/
#pragma once

#include "xdg_shell_positioner.h"
#include "xdg_shell_surface.h"

#include "wayland/resource.h"

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>

#include <wayland-xdg-shell-server-protocol.h>

namespace Wrapland::Server
{

// TODO(romangg): The QObject dependency is only necessary because of the resourceDestroyed signal.
//                Refactor Resource, such that it is not necessary anymore.
class XdgShellPositioner : public QObject
{
    Q_OBJECT
public:
    XdgShellPositioner(Client* client, uint32_t version, uint32_t id);

    xdg_shell_positioner const& get_data() const;

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class XdgShell;

    class Private;
    Private* d_ptr;
};

class XdgShellPositioner::Private : public Wayland::Resource<XdgShellPositioner>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, XdgShellPositioner* q_ptr);

    xdg_shell_positioner data;

private:
    static void
    setSizeCallback(wl_client* wlClient, wl_resource* wlResource, int32_t width, int32_t height);
    static void setAnchorRectCallback(wl_client* wlClient,
                                      wl_resource* wlResource,
                                      int32_t pos_x,
                                      int32_t pos_y,
                                      int32_t width,
                                      int32_t height);
    static void setAnchorCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t anchor);
    static void setGravityCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t gravity);
    static void setConstraintAdjustmentCallback(wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t constraint_adjustment);
    static void
    setOffsetCallback(wl_client* wlClient, wl_resource* wlResource, int32_t pos_x, int32_t pos_y);
    static void set_reactive_callback(wl_client* wlClient, wl_resource* wlResource);
    static void set_parent_size_callback(wl_client* wlClient,
                                         wl_resource* wlResource,
                                         int32_t parent_width,
                                         int32_t parent_height);
    static void
    set_parent_configure_callback(wl_client* wlClient, wl_resource* wlResource, uint32_t serial);

    static const struct xdg_positioner_interface s_interface;
};

}
