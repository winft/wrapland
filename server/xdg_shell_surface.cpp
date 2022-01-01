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
#include "xdg_shell_surface.h"
#include "xdg_shell_surface_p.h"

#include "surface_p.h"
#include "xdg_shell.h"
#include "xdg_shell_p.h"
#include "xdg_shell_popup.h"
#include "xdg_shell_popup_p.h"
#include "xdg_shell_positioner_p.h"
#include "xdg_shell_toplevel.h"
#include "xdg_shell_toplevel_p.h"

#include "wayland/resource.h"

namespace Wrapland::Server
{

const struct xdg_surface_interface XdgShellSurface::Private::s_interface = {
    destroyCallback,
    getTopLevelCallback,
    getPopupCallback,
    setWindowGeometryCallback,
    ackConfigureCallback,
};

XdgShellSurface::Private::Private(Client* client,
                                  uint32_t version,
                                  uint32_t id,
                                  XdgShell* shell,
                                  Surface* surface,
                                  XdgShellSurface* q)
    : Wayland::Resource<XdgShellSurface>(client,
                                         version,
                                         id,
                                         &xdg_surface_interface,
                                         &s_interface,
                                         q)
    , m_shell(shell)
    , m_surface(surface)
{
}

void XdgShellSurface::Private::getTopLevelCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource,
                                                   uint32_t id)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!priv->check_creation_error()) {
        return;
    }
    auto topLevel = new XdgShellToplevel(priv->version, id, priv->handle);
    priv->toplevel = topLevel;

    priv->m_surface->d_ptr->shellSurface = priv->handle;
    QObject::connect(topLevel,
                     &XdgShellToplevel::resourceDestroyed,
                     priv->m_surface,
                     [surface = priv->m_surface] { surface->d_ptr->shellSurface = nullptr; });

    Q_EMIT priv->m_shell->toplevelCreated(topLevel);
}

void XdgShellSurface::Private::getPopupCallback([[maybe_unused]] wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t id,
                                                wl_resource* wlParent,
                                                wl_resource* wlPositioner)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!priv->check_creation_error()) {
        return;
    }

    auto positioner = priv->m_shell->d_ptr->getPositioner(wlPositioner);
    if (!positioner) {
        priv->postError(XDG_WM_BASE_ERROR_INVALID_POSITIONER, "Invalid positioner");
        return;
    }

    // TODO(romangg): Allow to set parent surface via side-channel (see protocol description).
    auto parent = wlParent ? priv->m_shell->d_ptr->getSurface(wlParent) : nullptr;
    if (wlParent && !parent) {
        priv->postError(XDG_WM_BASE_ERROR_INVALID_POPUP_PARENT, "Invalid popup parent");
        return;
    }

    auto popup = new XdgShellPopup(priv->version, id, priv->handle, parent);

    popup->d_ptr->parent = parent;
    popup->d_ptr->initialSize = positioner->initialSize();
    popup->d_ptr->anchorRect = positioner->anchorRect();
    popup->d_ptr->anchorEdge = positioner->anchorEdge();
    popup->d_ptr->gravity = positioner->gravity();
    popup->d_ptr->constraintAdjustments = positioner->constraintAdjustments();
    popup->d_ptr->anchorOffset = positioner->anchorOffset();

    priv->popup = popup;

    priv->m_surface->d_ptr->shellSurface = priv->handle;
    QObject::connect(popup,
                     &XdgShellPopup::resourceDestroyed,
                     priv->m_surface,
                     [surface = priv->m_surface] { surface->d_ptr->shellSurface = nullptr; });

    Q_EMIT priv->m_shell->popupCreated(popup);
}

void XdgShellSurface::Private::setWindowGeometryCallback([[maybe_unused]] wl_client* wlClient,
                                                         wl_resource* wlResource,
                                                         int32_t x,
                                                         int32_t y,
                                                         int32_t width,
                                                         int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!priv->toplevel && !priv->popup) {
        priv->postError(XDG_SURFACE_ERROR_NOT_CONSTRUCTED, "No role object constructed.");
        return;
    }

    if (width < 0 || height < 0) {
        priv->postError(XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                        "Tried to set invalid xdg-surface geometry");
        return;
    }

    priv->pending_state.window_geometry = QRect(x, y, width, height);
    priv->pending_state.window_geometry_set = true;
}

void XdgShellSurface::Private::ackConfigureCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource,
                                                    uint32_t serial)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!priv->toplevel && !priv->popup) {
        priv->postError(XDG_SURFACE_ERROR_NOT_CONSTRUCTED, "No role object constructed.");
        return;
    }

    if (priv->toplevel) {
        priv->toplevel->d_ptr->ackConfigure(serial);
    } else if (priv->popup) {
        priv->popup->d_ptr->ackConfigure(serial);
    }
}

bool XdgShellSurface::Private::check_creation_error()
{
    if (m_surface->d_ptr->has_role()) {
        postError(XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "Surface already has a role.");
        return false;
    }
    if (m_surface->d_ptr->had_buffer_attached) {
        postError(XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                  "Creation after a buffer was already attached.");
        return false;
    }
    return true;
}

XdgShellSurface::XdgShellSurface(Client* client,
                                 uint32_t version,
                                 uint32_t id,
                                 XdgShell* shell,
                                 Surface* surface)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, shell, surface, this))
{
}

bool XdgShellSurface::configurePending() const
{
    return !d_ptr->configureSerials.empty();
}

Surface* XdgShellSurface::surface() const
{
    return d_ptr->m_surface;
}

void XdgShellSurface::commit()
{
    auto const geo_set = d_ptr->pending_state.window_geometry_set;

    if (geo_set) {
        d_ptr->current_state.window_geometry = d_ptr->pending_state.window_geometry;
        d_ptr->current_state.window_geometry_set = true;
    }

    d_ptr->pending_state = Private::state{};

    if (d_ptr->toplevel) {
        d_ptr->toplevel->d_ptr->commit();
    }

    if (geo_set) {
        Q_EMIT window_geometry_changed(d_ptr->current_state.window_geometry);
    }
}

QRect XdgShellSurface::window_geometry() const
{
    auto const bounds_geo = surface()->expanse();

    if (!d_ptr->current_state.window_geometry_set) {
        return bounds_geo;
    }

    return d_ptr->current_state.window_geometry.intersected(bounds_geo);
}

QMargins XdgShellSurface::window_margins() const
{
    auto const window_geo = window_geometry();

    QMargins margins;

    margins.setLeft(window_geo.left());
    margins.setTop(window_geo.top());

    auto const surface_size = surface()->size();

    margins.setRight(surface_size.width() - window_geo.right() - 1);
    margins.setBottom(surface_size.height() - window_geo.bottom() - 1);

    return margins;
}

}
