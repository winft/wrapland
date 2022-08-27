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
#include "plasma_shell_p.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_plasma_shell_interface PlasmaShell::Private::s_interface = {
    cb<createSurfaceCallback>,
};

PlasmaShell::Private::Private(Display* display, PlasmaShell* qptr)
    : PlasmaShellGlobal(qptr, display, &org_kde_plasma_shell_interface, &s_interface)
{
    create();
}

PlasmaShell::Private::~Private() = default;

void PlasmaShell::Private::createSurfaceCallback(PlasmaShellBind* bind,
                                                 uint32_t id,
                                                 wl_resource* surface)
{
    auto priv = bind->global()->handle->d_ptr.get();
    priv->createSurface(bind, id, Wayland::Resource<Surface>::get_handle(surface));
}

void PlasmaShell::Private::createSurface(PlasmaShellBind* bind, uint32_t id, Surface* surface)
{
    auto it = std::find_if(surfaces.constBegin(), surfaces.constEnd(), [surface](auto pss) {
        return surface == pss->surface();
    });
    if (it != surfaces.constEnd()) {
        surface->d_ptr->postError(WL_DISPLAY_ERROR_INVALID_OBJECT,
                                  "PlasmaShellSurface already created");
        return;
    }

    auto shellSurface
        = new PlasmaShellSurface(bind->client->handle, bind->version, id, surface, handle);

    surfaces << shellSurface;
    QObject::connect(shellSurface,
                     &PlasmaShellSurface::resourceDestroyed,
                     handle,
                     [this, shellSurface] { surfaces.removeAll(shellSurface); });
    Q_EMIT handle->surfaceCreated(shellSurface);
}

PlasmaShell::PlasmaShell(Display* display)
    : d_ptr(new Private(display, this))
{
}

PlasmaShell::~PlasmaShell() = default;

/*********************************
 * ShellSurfaceInterface
 *********************************/

const struct org_kde_plasma_surface_interface PlasmaShellSurface::Private::s_interface = {
    destroyCallback,
    setOutputCallback,
    setPositionCallback,
    setRoleCallback,
    setPanelBehaviorCallback,
    setSkipTaskbarCallback,
    panelAutoHideHideCallback,
    panelAutoHideShowCallback,
    panelTakesFocusCallback,
    setSkipSwitcherCallback,
};

PlasmaShellSurface::Private::Private(Client* client,
                                     uint32_t version,
                                     uint32_t id,
                                     Surface* surface,
                                     PlasmaShell* shell,
                                     PlasmaShellSurface* qptr)
    : Wayland::Resource<PlasmaShellSurface>(client,
                                            version,
                                            id,
                                            &org_kde_plasma_surface_interface,
                                            &s_interface,
                                            qptr)
    , surface(surface)
    , shell(shell)

{
}

PlasmaShellSurface::Private::~Private() = default;

void PlasmaShellSurface::Private::setOutputCallback([[maybe_unused]] wl_client* wlClient,
                                                    [[maybe_unused]] wl_resource* wlResource,
                                                    [[maybe_unused]] wl_resource* output)
{
    // TODO(unknown author): implement
}

void PlasmaShellSurface::Private::setPositionCallback([[maybe_unused]] wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      int32_t pos_x,
                                                      int32_t pos_y)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->setPosition(QPoint(pos_x, pos_y));
}

void PlasmaShellSurface::Private::setPosition(QPoint const& globalPos)
{
    if (m_globalPos == globalPos && m_positionSet) {
        return;
    }
    m_positionSet = true;
    m_globalPos = globalPos;
    Q_EMIT handle->positionChanged();
}

void PlasmaShellSurface::Private::setRoleCallback([[maybe_unused]] wl_client* wlClient,
                                                  wl_resource* wlResource,
                                                  uint32_t role)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->setRole(role);
}

void PlasmaShellSurface::Private::setRole(uint32_t role)
{
    auto val = Role::Normal;
    switch (role) {
    case ORG_KDE_PLASMA_SURFACE_ROLE_DESKTOP:
        val = Role::Desktop;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_PANEL:
        val = Role::Panel;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_ONSCREENDISPLAY:
        val = Role::OnScreenDisplay;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_NOTIFICATION:
        val = Role::Notification;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_TOOLTIP:
        val = Role::ToolTip;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_CRITICALNOTIFICATION:
        val = Role::CriticalNotification;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_NORMAL:
    default:
        val = Role::Normal;
        break;
    }
    if (val == m_role) {
        return;
    }
    m_role = val;
    Q_EMIT handle->roleChanged();
}

void PlasmaShellSurface::Private::setPanelBehaviorCallback([[maybe_unused]] wl_client* wlClient,
                                                           wl_resource* wlResource,
                                                           uint32_t flag)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->setPanelBehavior(org_kde_plasma_surface_panel_behavior(flag));
}

void PlasmaShellSurface::Private::setSkipTaskbarCallback([[maybe_unused]] wl_client* wlClient,
                                                         wl_resource* wlResource,
                                                         uint32_t skip)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->m_skipTaskbar = static_cast<bool>(skip);
    Q_EMIT priv->handle->skipTaskbarChanged();
}

void PlasmaShellSurface::Private::setSkipSwitcherCallback([[maybe_unused]] wl_client* wlClient,
                                                          wl_resource* wlResource,
                                                          uint32_t skip)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->m_skipSwitcher = static_cast<bool>(skip);
    Q_EMIT priv->handle->skipSwitcherChanged();
}

void PlasmaShellSurface::Private::panelAutoHideHideCallback([[maybe_unused]] wl_client* wlClient,
                                                            wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    if (priv->m_role != Role::Panel
        || (priv->m_panelBehavior != PanelBehavior::AutoHide
            && priv->m_panelBehavior != PanelBehavior::WindowsCanCover)) {
        priv->postError(ORG_KDE_PLASMA_SURFACE_ERROR_PANEL_NOT_AUTO_HIDE, "Not an auto hide panel");
        return;
    }
    Q_EMIT priv->handle->panelAutoHideHideRequested();
}

void PlasmaShellSurface::Private::panelAutoHideShowCallback([[maybe_unused]] wl_client* wlClient,
                                                            wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    if (priv->m_role != Role::Panel || priv->m_panelBehavior != PanelBehavior::AutoHide) {
        priv->postError(ORG_KDE_PLASMA_SURFACE_ERROR_PANEL_NOT_AUTO_HIDE, "Not an auto hide panel");
        return;
    }
    Q_EMIT priv->handle->panelAutoHideShowRequested();
}

void PlasmaShellSurface::Private::panelTakesFocusCallback([[maybe_unused]] wl_client* wlClient,
                                                          wl_resource* wlResource,
                                                          uint32_t takesFocus)
{
    auto priv = get_handle(wlResource)->d_ptr;
    if (priv->panelTakesFocus == takesFocus) {
        return;
    }
    priv->panelTakesFocus = takesFocus;
    Q_EMIT priv->handle->panelTakesFocusChanged();
}

void PlasmaShellSurface::Private::setPanelBehavior(org_kde_plasma_surface_panel_behavior behavior)
{
    PanelBehavior newBehavior = PanelBehavior::AlwaysVisible;
    switch (behavior) {
    case ORG_KDE_PLASMA_SURFACE_PANEL_BEHAVIOR_AUTO_HIDE:
        newBehavior = PanelBehavior::AutoHide;
        break;
    case ORG_KDE_PLASMA_SURFACE_PANEL_BEHAVIOR_WINDOWS_CAN_COVER:
        newBehavior = PanelBehavior::WindowsCanCover;
        break;
    case ORG_KDE_PLASMA_SURFACE_PANEL_BEHAVIOR_WINDOWS_GO_BELOW:
        newBehavior = PanelBehavior::WindowsGoBelow;
        break;
    case ORG_KDE_PLASMA_SURFACE_PANEL_BEHAVIOR_ALWAYS_VISIBLE:
    default:
        break;
    }
    if (m_panelBehavior == newBehavior) {
        return;
    }
    m_panelBehavior = newBehavior;
    Q_EMIT handle->panelBehaviorChanged();
}

PlasmaShellSurface::PlasmaShellSurface(Client* client,
                                       uint32_t version,
                                       uint32_t id,
                                       Surface* surface,
                                       PlasmaShell* shell)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, surface, shell, this))

{
    auto unsetSurface = [this] { d_ptr->surface = nullptr; };
    connect(surface, &Surface::resourceDestroyed, this, unsetSurface);
}

Surface* PlasmaShellSurface::surface() const
{
    return d_ptr->surface;
}

PlasmaShell* PlasmaShellSurface::shell() const
{
    return d_ptr->shell;
}

wl_resource* PlasmaShellSurface::resource() const
{
    return d_ptr->resource;
}

Client* PlasmaShellSurface::client() const
{
    return d_ptr->client->handle;
}

QPoint PlasmaShellSurface::position() const
{
    return d_ptr->m_globalPos;
}

PlasmaShellSurface::Role PlasmaShellSurface::role() const
{
    return d_ptr->m_role;
}

bool PlasmaShellSurface::isPositionSet() const
{
    return d_ptr->m_positionSet;
}

PlasmaShellSurface::PanelBehavior PlasmaShellSurface::panelBehavior() const
{
    return d_ptr->m_panelBehavior;
}

bool PlasmaShellSurface::skipTaskbar() const
{
    return d_ptr->m_skipTaskbar;
}

bool PlasmaShellSurface::skipSwitcher() const
{
    return d_ptr->m_skipSwitcher;
}

void PlasmaShellSurface::hideAutoHidingPanel()
{
    d_ptr->send<org_kde_plasma_surface_send_auto_hidden_panel_hidden>();
}

void PlasmaShellSurface::showAutoHidingPanel()
{
    d_ptr->send<org_kde_plasma_surface_send_auto_hidden_panel_shown>();
}

bool PlasmaShellSurface::panelTakesFocus() const
{
    return d_ptr->panelTakesFocus;
}

PlasmaShellSurface* PlasmaShellSurface::get(wl_resource* native)
{
    if (native == nullptr) {
        return nullptr;
    }
    return Private::get_handle(native);
}

}
