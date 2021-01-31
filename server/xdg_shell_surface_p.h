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

#include "xdg_shell.h"
#include "xdg_shell_surface.h"

#include "wayland/resource.h"

#include <wayland-xdg-shell-server-protocol.h>

#include <QSize>

#include <deque>

namespace Wrapland::Server
{

class XdgShellSurface::Private : public Wayland::Resource<XdgShellSurface>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            XdgShell* shell,
            Surface* surface,
            XdgShellSurface* q);

    // Implement here?
    void close();

    uint32_t configure(States states, const QSize& size);
    QRect windowGeometry() const;
    QSize minimumSize() const;
    QSize maximumSize() const;

    XdgShell* m_shell;
    Surface* m_surface;

    // Effectively a union, only one of these should be populated. A surface can't have two roles.
    XdgShellToplevel* toplevel = nullptr;
    XdgShellPopup* popup = nullptr;

    std::deque<uint32_t> configureSerials;

    struct state {
        QRect window_geometry;
        bool window_geometry_set{false};
    };

    state current_state;
    state pending_state;

private:
    static void getTopLevelCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);
    static void getPopupCallback(wl_client* wlClient,
                                 wl_resource* wlResource,
                                 uint32_t id,
                                 wl_resource* wlParent,
                                 wl_resource* wlPositioner);
    static void ackConfigureCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t serial);
    static void setWindowGeometryCallback(wl_client* wlClient,
                                          wl_resource* wlResource,
                                          int32_t x,
                                          int32_t y,
                                          int32_t width,
                                          int32_t height);

    bool checkAlreadyConstructed();

    static const struct xdg_surface_interface s_interface;
};

}
