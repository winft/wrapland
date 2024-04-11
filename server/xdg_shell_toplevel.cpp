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
#include "xdg_shell_toplevel.h"
#include "xdg_shell_toplevel_p.h"

#include "display.h"
#include "logging.h"
#include "seat_p.h"
#include "wl_output_p.h"
#include "xdg_shell_surface.h"
#include "xdg_shell_surface_p.h"

#include "wayland/global.h"

namespace Wrapland::Server
{

const struct xdg_toplevel_interface XdgShellToplevel::Private::s_interface = {
    destroyCallback,
    setParentCallback,
    setTitleCallback,
    setAppIdCallback,
    showWindowMenuCallback,
    moveCallback,
    resizeCallback,
    setMaxSizeCallback,
    setMinSizeCallback,
    setMaximizedCallback,
    unsetMaximizedCallback,
    setFullscreenCallback,
    unsetFullscreenCallback,
    setMinimizedCallback,
};

XdgShellToplevel::Private::Private(uint32_t version,
                                   uint32_t id,
                                   XdgShellSurface* surface,
                                   XdgShellToplevel* q_ptr)
    : Wayland::Resource<XdgShellToplevel>(surface->d_ptr->client,
                                          version,
                                          id,
                                          &xdg_toplevel_interface,
                                          &s_interface,
                                          q_ptr)
    , shellSurface{surface}
{
}

void XdgShellToplevel::Private::setTitleCallback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 char const* title)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (priv->title == title) {
        return;
    }

    priv->title = title;
    Q_EMIT priv->handle->titleChanged(title);
}

void XdgShellToplevel::Private::setAppIdCallback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 char const* app_id)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (priv->appId == app_id) {
        return;
    }

    priv->appId = app_id;
    Q_EMIT priv->handle->appIdChanged(app_id);
}

void XdgShellToplevel::Private::moveCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             wl_resource* wlSeat,
                                             uint32_t serial)
{
    auto priv = get_handle(wlResource)->d_ptr;
    Q_EMIT priv->handle->moveRequested(SeatGlobal::get_handle(wlSeat), serial);
}

Qt::Edges edgesToQtEdges(xdg_toplevel_resize_edge edges)
{
    Qt::Edges qtEdges;
    switch (edges) {
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP:
        qtEdges = Qt::TopEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM:
        qtEdges = Qt::BottomEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_LEFT:
        qtEdges = Qt::LeftEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT:
        qtEdges = Qt::TopEdge | Qt::LeftEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT:
        qtEdges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT:
        qtEdges = Qt::RightEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT:
        qtEdges = Qt::TopEdge | Qt::RightEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT:
        qtEdges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_NONE:
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    return qtEdges;
}

void XdgShellToplevel::Private::resizeCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               wl_resource* wlSeat,
                                               uint32_t serial,
                                               uint32_t edges)
{
    auto priv = get_handle(wlResource)->d_ptr;
    Q_EMIT priv->handle->resizeRequested(
        SeatGlobal::get_handle(wlSeat),
        serial,
        edgesToQtEdges(static_cast<xdg_toplevel_resize_edge>(edges)));
}

void XdgShellToplevel::Private::ackConfigure(uint32_t serial)
{
    auto& serials = shellSurface->d_ptr->configureSerials;
    if (std::count(serials.cbegin(), serials.cend(), serial) == 0) {
        return;
    }
    for (;;) {
        if (serials.empty()) {
            break;
        }
        auto next_serial = serials.front();
        serials.pop_front();

        Q_EMIT handle->configureAcknowledged(next_serial);
        if (next_serial == serial) {
            break;
        }
    }
}

uint32_t XdgShellToplevel::Private::configure(XdgShellSurface::States states, QSize const& size)
{
    uint32_t const serial = client->display()->handle->nextSerial();

    wl_array configureStates;
    wl_array_init(&configureStates);

    if (states.testFlag(XdgShellSurface::State::Maximized)) {
        auto state = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *state = XDG_TOPLEVEL_STATE_MAXIMIZED;
    }
    if (states.testFlag(XdgShellSurface::State::Fullscreen)) {
        auto state = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *state = XDG_TOPLEVEL_STATE_FULLSCREEN;
    }
    if (states.testFlag(XdgShellSurface::State::Resizing)) {
        auto state = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *state = XDG_TOPLEVEL_STATE_RESIZING;
    }
    if (states.testFlag(XdgShellSurface::State::Activated)) {
        auto state = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *state = XDG_TOPLEVEL_STATE_ACTIVATED;
    }
    if (states.testFlag(XdgShellSurface::State::TiledLeft)) {
        auto state = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *state = XDG_TOPLEVEL_STATE_TILED_LEFT;
    }
    if (states.testFlag(XdgShellSurface::State::TiledRight)) {
        auto state = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *state = XDG_TOPLEVEL_STATE_TILED_RIGHT;
    }
    if (states.testFlag(XdgShellSurface::State::TiledTop)) {
        auto state = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *state = XDG_TOPLEVEL_STATE_TILED_TOP;
    }
    if (states.testFlag(XdgShellSurface::State::TiledBottom)) {
        auto state = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *state = XDG_TOPLEVEL_STATE_TILED_BOTTOM;
    }

    shellSurface->d_ptr->configureSerials.push_back(serial);

    send<xdg_toplevel_send_configure>(
        std::max(size.width(), 0), std::max(size.height(), 0), &configureStates);
    shellSurface->d_ptr->send<xdg_surface_send_configure>(serial);

    client->flush();
    wl_array_release(&configureStates);
    return serial;
}

QSize XdgShellToplevel::Private::minimumSize() const
{
    return m_currentState.minimumSize;
}

QSize XdgShellToplevel::Private::maximumSize() const
{
    return m_currentState.maximiumSize;
}

void XdgShellToplevel::Private::close()
{
    send<xdg_toplevel_send_close>();
    client->flush();
}

void XdgShellToplevel::Private::commit()
{
    bool const minimumSizeChanged = m_pendingState.minimumSizeIsSet;
    bool const maximumSizeChanged = m_pendingState.maximumSizeIsSet;

    if (minimumSizeChanged) {
        m_currentState.minimumSize = m_pendingState.minimumSize;
    }
    if (maximumSizeChanged) {
        m_currentState.maximiumSize = m_pendingState.maximiumSize;
    }

    m_pendingState = ShellSurfaceState{};

    if (minimumSizeChanged) {
        Q_EMIT handle->minSizeChanged(m_currentState.minimumSize);
    }
    if (maximumSizeChanged) {
        Q_EMIT handle->maxSizeChanged(m_currentState.maximiumSize);
    }
}

void XdgShellToplevel::Private::setMaxSizeCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource,
                                                   int32_t width,
                                                   int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (width < 0 || height < 0) {
        priv->postError(XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                        "Invalid xdg-toplevel maximum size");
        return;
    }
    if (width == 0) {
        width = INT32_MAX;
    }
    if (height == 0) {
        height = INT32_MAX;
    }

    priv->m_pendingState.maximiumSize = QSize(width, height);
    priv->m_pendingState.maximumSizeIsSet = true;
}

void XdgShellToplevel::Private::setMinSizeCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource,
                                                   int32_t width,
                                                   int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (width < 0 || height < 0) {
        priv->postError(XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                        "Invalid xdg-toplevel minimum size");
        return;
    }

    priv->m_pendingState.minimumSize = QSize(width, height);
    priv->m_pendingState.minimumSizeIsSet = true;
}

void XdgShellToplevel::Private::setParentCallback([[maybe_unused]] wl_client* wlClient,
                                                  wl_resource* wlResource,
                                                  wl_resource* wlParent)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!wlParent) {
        // setting null is valid API. Clear
        priv->parentSurface = nullptr;
        Q_EMIT priv->handle->transientForChanged();
    } else {
        auto parent = Wayland::Resource<XdgShellToplevel>::get_handle(wlParent);
        if (priv->parentSurface != parent) {
            priv->parentSurface = parent;
            Q_EMIT priv->handle->transientForChanged();
        }
    }
}

void XdgShellToplevel::Private::showWindowMenuCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       wl_resource* wlSeat,
                                                       uint32_t serial,
                                                       int32_t pos_x,
                                                       int32_t pos_y)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto seat = SeatGlobal::get_handle(wlSeat);
    Q_EMIT priv->handle->windowMenuRequested(seat, serial, QPoint(pos_x, pos_y));
}

void XdgShellToplevel::Private::setMaximizedCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    Q_EMIT priv->handle->maximizedChanged(true);
}

void XdgShellToplevel::Private::unsetMaximizedCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    Q_EMIT priv->handle->maximizedChanged(false);
}

void XdgShellToplevel::Private::setFullscreenCallback([[maybe_unused]] wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      wl_resource* wlOutput)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto output = wlOutput ? WlOutputGlobal::get_handle(wlOutput)->output() : nullptr;

    Q_EMIT priv->handle->fullscreenChanged(true, output);
}

void XdgShellToplevel::Private::unsetFullscreenCallback([[maybe_unused]] wl_client* wlClient,
                                                        wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    Q_EMIT priv->handle->fullscreenChanged(false, nullptr);
}

void XdgShellToplevel::Private::setMinimizedCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    Q_EMIT priv->handle->minimizeRequested();
}

XdgShellToplevel::XdgShellToplevel(uint32_t version, uint32_t id, XdgShellSurface* surface)
    : QObject(nullptr)
    , d_ptr(new Private(version, id, surface, this))
{
}

XdgShellToplevel* XdgShellToplevel::transientFor() const
{
    return d_ptr->parentSurface;
}

QSize XdgShellToplevel::minimumSize() const
{
    return d_ptr->minimumSize();
}

QSize XdgShellToplevel::maximumSize() const
{
    return d_ptr->maximumSize();
}

void XdgShellToplevel::configure_bounds(QSize const& bounds)
{
    auto val = bounds.isValid() ? bounds : QSize(0, 0);
    d_ptr->send<xdg_toplevel_send_configure_bounds, XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION>(
        val.width(), val.height());
}

void XdgShellToplevel::unconfigure_bounds()
{
    configure_bounds({});
}

uint32_t XdgShellToplevel::configure(XdgShellSurface::States states, QSize const& size)
{
    return d_ptr->configure(states, size);
}

bool XdgShellToplevel::configurePending() const
{
    return d_ptr->shellSurface->configurePending();
}

void XdgShellToplevel::set_capabilities(std::set<xdg_shell_wm_capability> const& caps) const
{
    wl_array wlcaps;
    wl_array_init(&wlcaps);

    auto get_wlcap = [](auto cap) -> xdg_toplevel_wm_capabilities {
        switch (cap) {
        case xdg_shell_wm_capability::window_menu:
            return XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU;
        case xdg_shell_wm_capability::maximize:
            return XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE;
        case xdg_shell_wm_capability::fullscreen:
            return XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN;
        case xdg_shell_wm_capability::minimize:
            return XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
        }

        // Should never reach. Return some value so it builds in release config.
        assert(false);
        return XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
    };

    for (auto cap : caps) {
        auto state = static_cast<uint32_t*>(wl_array_add(&wlcaps, sizeof(uint32_t)));
        *state = get_wlcap(cap);
    }

    d_ptr->send<xdg_toplevel_send_wm_capabilities, XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION>(
        &wlcaps);
    wl_array_release(&wlcaps);
}

void XdgShellToplevel::close()
{
    d_ptr->close();
}

XdgShellSurface* XdgShellToplevel::surface() const
{
    return d_ptr->shellSurface;
}

Client* XdgShellToplevel::client() const
{
    return d_ptr->client->handle;
}

std::string XdgShellToplevel::title() const
{
    return d_ptr->title;
}

std::string XdgShellToplevel::appId() const
{
    return d_ptr->appId;
}

}
