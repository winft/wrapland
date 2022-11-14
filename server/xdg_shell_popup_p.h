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

#include "xdg_shell_popup.h"
#include "xdg_shell_positioner_p.h"
#include "xdg_shell_surface.h"

#include "wayland/resource.h"

#include <wayland-xdg-shell-server-protocol.h>

#include <QRect>

namespace Wrapland::Server
{

using positioner_getter_t = std::function<XdgShellPositioner*(wl_resource*)>;

class XdgShellPopup::Private : public Wayland::Resource<XdgShellPopup>
{
public:
    Private(uint32_t version,
            uint32_t id,
            XdgShellSurface* surface,
            XdgShellSurface* parent,
            positioner_getter_t positioner_getter,
            XdgShellPopup* q_ptr);

    uint32_t configure(QRect const& rect);
    void ackConfigure(uint32_t serial);

    void popupDone();

    XdgShellSurface* shellSurface;
    XdgShellSurface* parent;

    xdg_shell_positioner positioner;
    positioner_getter_t positioner_getter;

private:
    static void grabCallback(wl_client* wlClient,
                             wl_resource* wlResource,
                             wl_resource* wlSeat,
                             uint32_t serial);
    static void reposition_callback(wl_client* wlClient,
                                    wl_resource* wlResource,
                                    wl_resource* wlPositioner,
                                    uint32_t token);

    static const struct xdg_popup_interface s_interface;
};

}
