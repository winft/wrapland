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

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-xdg-shell-server-protocol.h>

#include <QTimer>

namespace Wrapland::Server
{
class XdgShellPositioner;

constexpr uint32_t XdgShellVersion = 5;
using XdgShellGlobal = Wayland::Global<XdgShell, XdgShellVersion>;

class XdgShell::Private : public XdgShellGlobal
{
public:
    Private(XdgShell* q_ptr, Display* display);

    void setupTimer(uint32_t serial);

    uint32_t ping(Client* client);

    XdgShellSurface* getSurface(wl_resource* wlSurface);
    XdgShellToplevel* getToplevel(wl_resource* wlToplevel);
    XdgShellPositioner* getPositioner(wl_resource* wlPositioner);

    struct BindResources {
        std::vector<XdgShellSurface*> surfaces;
        std::vector<XdgShellPositioner*> positioners;
    };
    std::map<XdgShellGlobal::bind_t*, BindResources> bindsObjects;

    // ping-serial to timer
    std::map<uint32_t, QTimer*> pingTimers;

protected:
    void prepareUnbind(XdgShellGlobal::bind_t* bind) override;

private:
    static void createPositionerCallback(XdgShellGlobal::bind_t* bind, uint32_t id);
    static void
    getXdgSurfaceCallback(XdgShellGlobal::bind_t* bind, uint32_t id, wl_resource* wlSurface);
    static void pongCallback(XdgShellGlobal::bind_t* bind, uint32_t serial);

    static const struct xdg_wm_base_interface s_interface;
};

}
