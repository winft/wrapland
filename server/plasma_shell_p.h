/********************************************************************
Copyright 2020 Faveraux Adrien <ad1rie3@hotmail.fr>

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

#include <QObject>
#include <memory>

#include "plasma_shell.h"

#include "display.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "surface.h"
#include "surface_p.h"

#include <wayland-plasma-shell-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t PlasmaShellVersion = 8;
using PlasmaShellGlobal = Wayland::Global<PlasmaShell, PlasmaShellVersion>;
using PlasmaShellBind = Wayland::Bind<PlasmaShellGlobal>;

class PlasmaShell::Private : public PlasmaShellGlobal
{
public:
    Private(Display* display, PlasmaShell* qptr);
    ~Private() override;

    QList<PlasmaShellSurface*> surfaces;

private:
    static void createSurfaceCallback(PlasmaShellBind* bind, uint32_t id, wl_resource* surface);
    void createSurface(PlasmaShellBind* bind, uint32_t id, Surface* surface);

    static const struct org_kde_plasma_shell_interface s_interface;
};

class PlasmaShellSurface::Private : public Wayland::Resource<PlasmaShellSurface>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Surface* surface,
            PlasmaShell* shell,
            PlasmaShellSurface* qptr);
    ~Private() override;

    Surface* surface;
    PlasmaShell* shell;
    QPoint m_globalPos;
    Role m_role = Role::Normal;
    bool m_positionSet = false;
    PanelBehavior m_panelBehavior = PanelBehavior::AlwaysVisible;
    bool m_skipTaskbar = false;
    bool m_skipSwitcher = false;
    bool panelTakesFocus = false;
    bool open_under_cursor{false};

private:
    static void setOutputCallback(wl_client* client, wl_resource* resource, wl_resource* output);
    static void
    setPositionCallback(wl_client* client, wl_resource* resource, int32_t pos_x, int32_t pos_y);
    static void setRoleCallback(wl_client* client, wl_resource* resource, uint32_t role);
    static void setPanelBehaviorCallback(wl_client* client, wl_resource* resource, uint32_t flag);
    static void setSkipTaskbarCallback(wl_client* client, wl_resource* resource, uint32_t skip);
    static void setSkipSwitcherCallback(wl_client* client, wl_resource* resource, uint32_t skip);
    static void panelAutoHideHideCallback(wl_client* client, wl_resource* resource);
    static void panelAutoHideShowCallback(wl_client* client, wl_resource* resource);
    static void
    panelTakesFocusCallback(wl_client* client, wl_resource* resource, uint32_t takesFocus);
    static void open_under_cursor_callback(wl_client* client, wl_resource* resource);

    void setPosition(QPoint const& globalPos);
    void setRole(uint32_t role);
    void setPanelBehavior(org_kde_plasma_surface_panel_behavior behavior);

    static const struct org_kde_plasma_surface_interface s_interface;
};

}
