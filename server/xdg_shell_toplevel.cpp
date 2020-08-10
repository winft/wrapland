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
                                   XdgShellToplevel* q)
    : Wayland::Resource<XdgShellToplevel>(surface->d_ptr->client(),
                                          version,
                                          id,
                                          &xdg_toplevel_interface,
                                          &s_interface,
                                          q)
    , shellSurface{surface}
{
}

void XdgShellToplevel::Private::setTitleCallback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 const char* title)
{
    auto priv = handle(wlResource)->d_ptr;

    if (priv->title == title) {
        return;
    }

    priv->title = title;
    Q_EMIT priv->handle()->titleChanged(title);
}

void XdgShellToplevel::Private::setAppIdCallback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 const char* app_id)
{
    auto priv = handle(wlResource)->d_ptr;

    if (priv->appId == app_id) {
        return;
    }

    priv->appId = app_id;
    Q_EMIT priv->handle()->appIdChanged(app_id);
}

void XdgShellToplevel::Private::moveCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             wl_resource* wlSeat,
                                             uint32_t serial)
{
    auto priv = handle(wlResource)->d_ptr;
    Q_EMIT priv->handle()->moveRequested(SeatGlobal::handle(wlSeat), serial);
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
    auto priv = handle(wlResource)->d_ptr;
    Q_EMIT priv->handle()->resizeRequested(
        SeatGlobal::handle(wlSeat), serial, edgesToQtEdges(xdg_toplevel_resize_edge(edges)));
}

void XdgShellToplevel::Private::ackConfigure(uint32_t serial)
{
    auto& serials = shellSurface->d_ptr->configureSerials;
    if (std::count(serials.cbegin(), serials.cend(), serial) == 0) {
        return;
    }
    while (!serials.empty()) {
        uint32_t i = serials.front();
        serials.pop_front();

        Q_EMIT handle()->configureAcknowledged(i);
        if (i == serial) {
            break;
        }
    }
}

uint32_t XdgShellToplevel::Private::configure(XdgShellSurface::States states, const QSize& size)
{
    const uint32_t serial = client()->display()->handle()->nextSerial();

    wl_array configureStates;
    wl_array_init(&configureStates);

    if (states.testFlag(XdgShellSurface::State::Maximized)) {
        auto s = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *s = XDG_TOPLEVEL_STATE_MAXIMIZED;
    }
    if (states.testFlag(XdgShellSurface::State::Fullscreen)) {
        auto s = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *s = XDG_TOPLEVEL_STATE_FULLSCREEN;
    }
    if (states.testFlag(XdgShellSurface::State::Resizing)) {
        auto s = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *s = XDG_TOPLEVEL_STATE_RESIZING;
    }
    if (states.testFlag(XdgShellSurface::State::Activated)) {
        auto s = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
        *s = XDG_TOPLEVEL_STATE_ACTIVATED;
    }

    shellSurface->d_ptr->configureSerials.push_back(serial);

    send<xdg_toplevel_send_configure>(size.width(), size.height(), &configureStates);
    shellSurface->d_ptr->send<xdg_surface_send_configure>(serial);

    client()->flush();
    wl_array_release(&configureStates);
    return serial;
}

QRect XdgShellToplevel::Private::windowGeometry() const
{
    return m_currentState.windowGeometry;
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
    client()->flush();
}

void XdgShellToplevel::Private::commit()
{
    const bool windowGeometryChanged = m_pendingState.windowGeometryIsSet;
    const bool minimumSizeChanged = m_pendingState.minimumSizeIsSet;
    const bool maximumSizeChanged = m_pendingState.maximumSizeIsSet;

    if (windowGeometryChanged) {
        m_currentState.windowGeometry = m_pendingState.windowGeometry;
    }
    if (minimumSizeChanged) {
        m_currentState.minimumSize = m_pendingState.minimumSize;
    }
    if (maximumSizeChanged) {
        m_currentState.maximiumSize = m_pendingState.maximiumSize;
    }

    m_pendingState = ShellSurfaceState{};

    if (windowGeometryChanged) {
        Q_EMIT handle()->windowGeometryChanged(m_currentState.windowGeometry);
    }
    if (minimumSizeChanged) {
        Q_EMIT handle()->minSizeChanged(m_currentState.minimumSize);
    }
    if (maximumSizeChanged) {
        Q_EMIT handle()->maxSizeChanged(m_currentState.maximiumSize);
    }
}

void XdgShellToplevel::Private::setMaxSizeCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource,
                                                   int32_t width,
                                                   int32_t height)
{
    auto priv = handle(wlResource)->d_ptr;

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
    auto priv = handle(wlResource)->d_ptr;

    if (width < 0 || height < 0) {
        priv->postError(XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                        "Invalid xdg-toplevel minimum size");
        return;
    }

    priv->m_pendingState.minimumSize = QSize(width, height);
    priv->m_pendingState.minimumSizeIsSet = true;
}

void XdgShellToplevel::Private::setWindowGeometry(const QRect& rect)
{
    m_pendingState.windowGeometry = rect;
    m_pendingState.windowGeometryIsSet = true;
}

void XdgShellToplevel::Private::setParentCallback([[maybe_unused]] wl_client* wlClient,
                                                  wl_resource* wlResource,
                                                  wl_resource* wlParent)
{
    auto priv = handle(wlResource)->d_ptr;

    if (!wlParent) {
        // setting null is valid API. Clear
        priv->parentSurface = nullptr;
        Q_EMIT priv->handle()->transientForChanged();
    } else {
        auto parent = Wayland::Resource<XdgShellToplevel>::handle(wlParent);
        if (priv->parentSurface != parent) {
            priv->parentSurface = parent;
            Q_EMIT priv->handle()->transientForChanged();
        }
    }
}

void XdgShellToplevel::Private::showWindowMenuCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       wl_resource* wlSeat,
                                                       uint32_t serial,
                                                       int32_t x,
                                                       int32_t y)
{
    auto priv = handle(wlResource)->d_ptr;
    auto seat = SeatGlobal::handle(wlSeat);
    Q_EMIT priv->handle()->windowMenuRequested(seat, serial, QPoint(x, y));
}

void XdgShellToplevel::Private::setMaximizedCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    Q_EMIT priv->handle()->maximizedChanged(true);
}

void XdgShellToplevel::Private::unsetMaximizedCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    Q_EMIT priv->handle()->maximizedChanged(false);
}

void XdgShellToplevel::Private::setFullscreenCallback([[maybe_unused]] wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      wl_resource* wlOutput)
{
    auto priv = handle(wlResource)->d_ptr;
    auto output = wlOutput ? WlOutputGlobal::handle(wlOutput) : nullptr;
    Q_EMIT priv->handle()->fullscreenChanged(true, output);
}

void XdgShellToplevel::Private::unsetFullscreenCallback([[maybe_unused]] wl_client* wlClient,
                                                        wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    Q_EMIT priv->handle()->fullscreenChanged(false, nullptr);
}

void XdgShellToplevel::Private::setMinimizedCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    Q_EMIT priv->handle()->minimizeRequested();
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

QRect XdgShellToplevel::windowGeometry() const
{
    return d_ptr->windowGeometry();
}

QSize XdgShellToplevel::minimumSize() const
{
    return d_ptr->minimumSize();
}

QSize XdgShellToplevel::maximumSize() const
{
    return d_ptr->maximumSize();
}

uint32_t XdgShellToplevel::configure(XdgShellSurface::States states, const QSize& size)
{
    return d_ptr->configure(states, size);
}

bool XdgShellToplevel::configurePending() const
{
    return d_ptr->shellSurface->configurePending();
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
    return d_ptr->client()->handle();
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
