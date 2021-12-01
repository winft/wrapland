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
#include "xdg_shell_popup.h"
#include "xdg_shell_popup_p.h"

#include "display.h"
#include "seat_p.h"
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
    // TODO(romangg): Update xdg-shell protocol version (currently at 1).
    // NOLINTNEXTLINE(clang-diagnostic-missing-field-initializers)
};

XdgShellPopup::Private::Private(uint32_t version,
                                uint32_t id,
                                XdgShellSurface* surface,
                                XdgShellSurface* parent,
                                XdgShellPopup* q)
    : Wayland::Resource<XdgShellPopup>(surface->d_ptr->client(),
                                       version,
                                       id,
                                       &xdg_popup_interface,
                                       &s_interface,
                                       q)
    , shellSurface{surface}
    , parent{parent}
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

        uint32_t i = serials.front();
        serials.pop_front();

        Q_EMIT handle()->configureAcknowledged(i);
        if (i == serial) {
            break;
        }
    }
}

void XdgShellPopup::Private::grabCallback([[maybe_unused]] wl_client* wlClient,
                                          wl_resource* wlResource,
                                          wl_resource* wlSeat,
                                          uint32_t serial)
{
    auto priv = handle(wlResource)->d_ptr;
    auto seat = SeatGlobal::handle(wlSeat);

    priv->handle()->grabRequested(seat, serial);
}

uint32_t XdgShellPopup::Private::configure(const QRect& rect)
{
    const uint32_t serial = client()->display()->handle()->nextSerial();
    shellSurface->d_ptr->configureSerials.push_back(serial);

    send<xdg_popup_send_configure>(rect.x(), rect.y(), rect.width(), rect.height());
    shellSurface->d_ptr->send<xdg_surface_send_configure>(serial);
    client()->flush();

    return serial;
}

void XdgShellPopup::Private::popupDone()
{
    // TODO(unknown author): dismiss all child popups
    send<xdg_popup_send_popup_done>();
    client()->flush();
}

XdgShellPopup::XdgShellPopup(uint32_t version,
                             uint32_t id,
                             XdgShellSurface* surface,
                             XdgShellSurface* parent)
    : QObject(nullptr)
    , d_ptr(new Private(version, id, surface, parent, this))
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

QSize XdgShellPopup::initialSize() const
{
    return d_ptr->initialSize;
}

QPoint XdgShellPopup::transientOffset() const
{
    QRect rect = anchorRect();
    const QPoint center = rect.isEmpty() ? rect.topLeft() : rect.center();

    // Compensate for QRect::right + 1.
    rect = rect.adjusted(0, 0, 1, 1);

    switch (anchorEdge()) {
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

QRect XdgShellPopup::anchorRect() const
{
    return d_ptr->anchorRect;
}

Qt::Edges XdgShellPopup::anchorEdge() const
{
    return d_ptr->anchorEdge;
}

Qt::Edges XdgShellPopup::gravity() const
{
    return d_ptr->gravity;
}

QPoint XdgShellPopup::anchorOffset() const
{
    return d_ptr->anchorOffset;
}

XdgShellSurface::ConstraintAdjustments XdgShellPopup::constraintAdjustments() const
{
    return d_ptr->constraintAdjustments;
}

void XdgShellPopup::popupDone()
{
    return d_ptr->popupDone();
}

uint32_t XdgShellPopup::configure(const QRect& rect)
{
    return d_ptr->configure(rect);
}

Client* XdgShellPopup::client() const
{
    return d_ptr->client()->handle();
}

}
