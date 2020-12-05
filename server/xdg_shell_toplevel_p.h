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

#include "xdg_shell_surface.h"
#include "xdg_shell_toplevel.h"

#include "wayland/resource.h"

#include <wayland-xdg-shell-server-protocol.h>

#include <QRect>

namespace Wrapland::Server
{

class XdgShellToplevel::Private : public Wayland::Resource<XdgShellToplevel>
{
public:
    Private(uint32_t version, uint32_t id, XdgShellSurface* surface, XdgShellToplevel* q);

    void setWindowGeometry(const QRect& rect);

    QSize minimumSize() const;
    QSize maximumSize() const;

    void close();
    void commit();

    uint32_t configure(XdgShellSurface::States states, const QSize& size);
    void ackConfigure(uint32_t serial);

    XdgShellSurface* shellSurface;
    XdgShellToplevel* parentSurface = nullptr;

    std::string title;
    std::string appId;

private:
    static void moveCallback(wl_client* wlClient,
                             wl_resource* wlResource,
                             wl_resource* wlSeat,
                             uint32_t serial);

    static void resizeCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               wl_resource* wlSeat,
                               uint32_t serial,
                               uint32_t edges);
    static void setTitleCallback(wl_client* wlClient, wl_resource* wlResource, const char* title);
    static void setAppIdCallback(wl_client* wlClient, wl_resource* wlResource, const char* app_id);

    static void
    setParentCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlParent);
    static void showWindowMenuCallback(wl_client* wlClient,
                                       wl_resource* wlResource,
                                       wl_resource* wlSeat,
                                       uint32_t serial,
                                       int32_t x,
                                       int32_t y);
    static void
    setMaxSizeCallback(wl_client* wlClient, wl_resource* wlResource, int32_t width, int32_t height);
    static void
    setMinSizeCallback(wl_client* wlClient, wl_resource* wlResource, int32_t width, int32_t height);
    static void setMaximizedCallback(wl_client* wlClient, wl_resource* wlResource);
    static void unsetMaximizedCallback(wl_client* wlClient, wl_resource* wlResource);
    static void
    setFullscreenCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlOutput);
    static void unsetFullscreenCallback(wl_client* wlClient, wl_resource* wlResource);
    static void setMinimizedCallback(wl_client* wlClient, wl_resource* wlResource);

    static const struct xdg_toplevel_interface s_interface;

    struct ShellSurfaceState {
        QSize minimumSize = QSize(0, 0);
        QSize maximiumSize = QSize(INT32_MAX, INT32_MAX);

        bool minimumSizeIsSet = false;
        bool maximumSizeIsSet = false;
    };

    ShellSurfaceState m_currentState;
    ShellSurfaceState m_pendingState;
};

}
