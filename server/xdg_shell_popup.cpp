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
#include "xdg_shell_popup_p.h"

#include "display.h"
#include "seat_p.h"
#include "xdg_shell_p.h"
#include "xdg_shell_surface_p.h"

#include "wayland/global.h"

#include <wayland-xdg-shell-server-protocol.h>

#include <QRect>

#include <algorithm>

namespace Wrapland::Server
{

const struct xdg_popup_interface XdgShellPopup::Private::s_interface = {
    destroyCallback,
    grabCallback,
    reposition_callback,
};

XdgShellPopup::Private::Private(uint32_t version,
                                uint32_t id,
                                XdgShellSurface* surface,
                                XdgShellSurface* parent,
                                positioner_getter_t positioner_getter,
                                XdgShellPopup* q_ptr)
    : Wayland::Resource<XdgShellPopup>(surface->d_ptr->client,
                                       version,
                                       id,
                                       &xdg_popup_interface,
                                       &s_interface,
                                       q_ptr)
    , shellSurface{surface}
    , parent{parent}
    , positioner_getter{std::move(positioner_getter)}
{
}

void XdgShellPopup::Private::ackConfigure(uint32_t serial)
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

void XdgShellPopup::Private::grabCallback([[maybe_unused]] wl_client* wlClient,
                                          wl_resource* wlResource,
                                          wl_resource* wlSeat,
                                          uint32_t serial)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto seat = SeatGlobal::get_handle(wlSeat);

    priv->handle->grabRequested(seat, serial);
}

void XdgShellPopup::Private::reposition_callback(wl_client* /*wlClient*/,
                                                 wl_resource* wlResource,
                                                 wl_resource* wlPositioner,
                                                 uint32_t token)
{
    auto priv = get_handle(wlResource)->d_ptr;

    auto positioner = priv->positioner_getter(wlPositioner);
    if (!positioner) {
        priv->postError(XDG_WM_BASE_ERROR_INVALID_POSITIONER, "Invalid positioner");
        return;
    }

    priv->positioner = positioner->get_data();
    Q_EMIT priv->handle->reposition(token);
}

uint32_t XdgShellPopup::Private::configure(QRect const& rect)
{
    uint32_t const serial = client->display()->handle->nextSerial();
    shellSurface->d_ptr->configureSerials.push_back(serial);

    send<xdg_popup_send_configure>(rect.x(), rect.y(), rect.width(), rect.height());
    shellSurface->d_ptr->send<xdg_surface_send_configure>(serial);
    client->flush();

    return serial;
}

void XdgShellPopup::Private::popupDone()
{
    // TODO(unknown author): dismiss all child popups
    send<xdg_popup_send_popup_done>();
    client->flush();
}

XdgShellPopup::XdgShellPopup(uint32_t version,
                             uint32_t id,
                             XdgShellSurface* surface,
                             XdgShellSurface* parent)
    : QObject(nullptr)
    , d_ptr(new Private(
          version,
          id,
          surface,
          parent,
          [surface](auto res) { return surface->d_ptr->m_shell->d_ptr->getPositioner(res); },
          this))
{
}

XdgShellSurface* XdgShellPopup::surface() const
{
    return d_ptr->shellSurface;
}

XdgShellSurface* XdgShellPopup::transientFor() const
{
    return d_ptr->parent;
}

QPoint XdgShellPopup::transientOffset() const
{
    auto rect = get_positioner().anchor.rect;
    QPoint const center = rect.isEmpty() ? rect.topLeft() : rect.center();

    // Compensate for QRect::right + 1.
    rect = rect.adjusted(0, 0, 1, 1);

    switch (get_positioner().anchor.edge) {
    case Qt::TopEdge | Qt::LeftEdge:
        return rect.topLeft();
    case Qt::TopEdge:
        return {center.x(), rect.y()};
    case Qt::TopEdge | Qt::RightEdge:
        return rect.topRight();
    case Qt::RightEdge:
        return {rect.right(), center.y()};
    case Qt::BottomEdge | Qt::RightEdge:
        return rect.bottomRight();
    case Qt::BottomEdge:
        return {center.x(), rect.bottom()};
    case Qt::BottomEdge | Qt::LeftEdge:
        return rect.bottomLeft();
    case Qt::LeftEdge:
        return {rect.left(), center.y()};
    default:
        return center;
    }
    Q_UNREACHABLE();
}

xdg_shell_positioner const& XdgShellPopup::get_positioner() const
{
    return d_ptr->positioner;
}

void XdgShellPopup::popupDone()
{
    d_ptr->popupDone();
}

uint32_t XdgShellPopup::configure(QRect const& rect)
{
    return d_ptr->configure(rect);
}

void XdgShellPopup::repositioned(uint32_t token)
{
    assert(d_ptr->version >= XDG_POPUP_REPOSITIONED_SINCE_VERSION);
    d_ptr->send<xdg_popup_send_repositioned>(token);
}

Client* XdgShellPopup::client() const
{
    return d_ptr->client->handle;
}

}
