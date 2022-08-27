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
#include "xdg_shell_positioner_p.h"

#include "xdg_shell_surface.h"

namespace Wrapland::Server
{

XdgShellPositioner::Private::Private(Client* client,
                                     uint32_t version,
                                     uint32_t id,
                                     XdgShellPositioner* q_ptr)
    : Wayland::Resource<XdgShellPositioner>(client,
                                            version,
                                            id,
                                            &xdg_positioner_interface,
                                            &s_interface,
                                            q_ptr)
{
}

const struct xdg_positioner_interface XdgShellPositioner::Private::s_interface = {
    destroyCallback,
    setSizeCallback,
    setAnchorRectCallback,
    setAnchorCallback,
    setGravityCallback,
    setConstraintAdjustmentCallback,
    setOffsetCallback,
    // TODO(romangg): Update xdg-shell protocol version (currently at 1).
    // NOLINTNEXTLINE(clang-diagnostic-missing-field-initializers)
};

void XdgShellPositioner::Private::setSizeCallback([[maybe_unused]] wl_client* wlClient,
                                                  wl_resource* wlResource,
                                                  int32_t width,
                                                  int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->initialSize = QSize(width, height);
}

void XdgShellPositioner::Private::setAnchorRectCallback([[maybe_unused]] wl_client* wlClient,
                                                        wl_resource* wlResource,
                                                        int32_t pos_x,
                                                        int32_t pos_y,
                                                        int32_t width,
                                                        int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->anchorRect = QRect(pos_x, pos_y, width, height);
}

void XdgShellPositioner::Private::setAnchorCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource,
                                                    uint32_t anchor)
{
    auto priv = get_handle(wlResource)->d_ptr;

    Qt::Edges qtEdges;
    switch (anchor) {
    case XDG_POSITIONER_ANCHOR_TOP:
        qtEdges = Qt::TopEdge;
        break;
    case XDG_POSITIONER_ANCHOR_BOTTOM:
        qtEdges = Qt::BottomEdge;
        break;
    case XDG_POSITIONER_ANCHOR_LEFT:
        qtEdges = Qt::LeftEdge;
        break;
    case XDG_POSITIONER_ANCHOR_TOP_LEFT:
        qtEdges = Qt::TopEdge | Qt::LeftEdge;
        break;
    case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
        qtEdges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case XDG_POSITIONER_ANCHOR_RIGHT:
        qtEdges = Qt::RightEdge;
        break;
    case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
        qtEdges = Qt::TopEdge | Qt::RightEdge;
        break;
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
        qtEdges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case XDG_POSITIONER_ANCHOR_NONE:
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    priv->anchorEdge = qtEdges;
}

void XdgShellPositioner::Private::setGravityCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource,
                                                     uint32_t gravity)
{
    auto priv = get_handle(wlResource)->d_ptr;

    Qt::Edges qtEdges;
    switch (gravity) {
    case XDG_POSITIONER_GRAVITY_TOP:
        qtEdges = Qt::TopEdge;
        break;
    case XDG_POSITIONER_GRAVITY_BOTTOM:
        qtEdges = Qt::BottomEdge;
        break;
    case XDG_POSITIONER_GRAVITY_LEFT:
        qtEdges = Qt::LeftEdge;
        break;
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
        qtEdges = Qt::TopEdge | Qt::LeftEdge;
        break;
    case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
        qtEdges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case XDG_POSITIONER_GRAVITY_RIGHT:
        qtEdges = Qt::RightEdge;
        break;
    case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
        qtEdges = Qt::TopEdge | Qt::RightEdge;
        break;
    case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
        qtEdges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case XDG_POSITIONER_GRAVITY_NONE:
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    priv->gravity = qtEdges;
}

void XdgShellPositioner::Private::setConstraintAdjustmentCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    uint32_t constraint_adjustment)
{
    auto priv = get_handle(wlResource)->d_ptr;

    XdgShellSurface::ConstraintAdjustments constraints;
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X) {
        constraints |= XdgShellSurface::ConstraintAdjustment::SlideX;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y) {
        constraints |= XdgShellSurface::ConstraintAdjustment::SlideY;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X) {
        constraints |= XdgShellSurface::ConstraintAdjustment::FlipX;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y) {
        constraints |= XdgShellSurface::ConstraintAdjustment::FlipY;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X) {
        constraints |= XdgShellSurface::ConstraintAdjustment::ResizeX;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y) {
        constraints |= XdgShellSurface::ConstraintAdjustment::ResizeY;
    }

    priv->constraintAdjustments = constraints;
}

void XdgShellPositioner::Private::setOffsetCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource,
                                                    int32_t pos_x,
                                                    int32_t pos_y)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->anchorOffset = QPoint(pos_x, pos_y);
}

XdgShellPositioner::XdgShellPositioner(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

QSize XdgShellPositioner::initialSize() const
{
    return d_ptr->initialSize;
}

QRect XdgShellPositioner::anchorRect() const
{
    return d_ptr->anchorRect;
}

Qt::Edges XdgShellPositioner::anchorEdge() const
{
    return d_ptr->anchorEdge;
}

Qt::Edges XdgShellPositioner::gravity() const
{
    return d_ptr->gravity;
}

XdgShellSurface::ConstraintAdjustments XdgShellPositioner::constraintAdjustments() const
{
    return d_ptr->constraintAdjustments;
}

QPoint XdgShellPositioner::anchorOffset() const
{
    return d_ptr->anchorOffset;
}

}
