/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_shell_positioner.h"

#include "wayland_pointer_p.h"

#include <wayland-xdg-shell-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN xdg_shell_positioner::Private
{
public:
    virtual ~Private() = default;

    void setup(::xdg_positioner* positioner);
    bool isValid() const;

    xdg_shell_positioner_data data;
    EventQueue* queue{nullptr};

    WaylandPointer<::xdg_positioner, xdg_positioner_destroy> positioner_ptr;
};

void xdg_shell_positioner::Private::setup(::xdg_positioner* positioner)
{
    Q_ASSERT(positioner);
    Q_ASSERT(!positioner_ptr);
    positioner_ptr.setup(positioner);
}

bool xdg_shell_positioner::Private::isValid() const
{
    return positioner_ptr.isValid();
}

xdg_shell_positioner::xdg_shell_positioner(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private)
{
}

xdg_shell_positioner::~xdg_shell_positioner() = default;

void xdg_shell_positioner::setup(::xdg_positioner* positioner)
{
    d_ptr->setup(positioner);
}

void xdg_shell_positioner::release()
{
    d_ptr->positioner_ptr.release();
}

void xdg_shell_positioner::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* xdg_shell_positioner::eventQueue()
{
    return d_ptr->queue;
}

xdg_shell_positioner::operator ::xdg_positioner*()
{
    return d_ptr->positioner_ptr;
}

xdg_shell_positioner::operator ::xdg_positioner*() const
{
    return d_ptr->positioner_ptr;
}

bool xdg_shell_positioner::isValid() const
{
    return d_ptr->isValid();
}

xdg_shell_positioner_data const& xdg_shell_positioner::get_data() const
{
    return d_ptr->data;
}

void xdg_shell_positioner::set_data(xdg_shell_positioner_data data)
{
    d_ptr->data = data;

    auto anchorRect = data.anchor.rect;
    xdg_positioner_set_anchor_rect(d_ptr->positioner_ptr,
                                   anchorRect.x(),
                                   anchorRect.y(),
                                   anchorRect.width(),
                                   anchorRect.height());

    xdg_positioner_set_size(d_ptr->positioner_ptr, data.size.width(), data.size.height());

    if (auto anchorOffset = data.anchor.offset; !anchorOffset.isNull()) {
        xdg_positioner_set_offset(d_ptr->positioner_ptr, anchorOffset.x(), anchorOffset.y());
    }

    auto const edge_data = data.anchor.edge;
    uint32_t anchor = XDG_POSITIONER_ANCHOR_NONE;

    if (edge_data.testFlag(Qt::TopEdge)) {
        if (edge_data.testFlag(Qt::LeftEdge) && ((edge_data & ~Qt::LeftEdge) == Qt::TopEdge)) {
            anchor = XDG_POSITIONER_ANCHOR_TOP_LEFT;
        } else if (edge_data.testFlag(Qt::RightEdge)
                   && ((edge_data & ~Qt::RightEdge) == Qt::TopEdge)) {
            anchor = XDG_POSITIONER_ANCHOR_TOP_RIGHT;
        } else if ((edge_data & ~Qt::TopEdge) == Qt::Edges()) {
            anchor = XDG_POSITIONER_ANCHOR_TOP;
        }
    } else if (edge_data.testFlag(Qt::BottomEdge)) {
        if (edge_data.testFlag(Qt::LeftEdge) && ((edge_data & ~Qt::LeftEdge) == Qt::BottomEdge)) {
            anchor = XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
        } else if (edge_data.testFlag(Qt::RightEdge)
                   && ((edge_data & ~Qt::RightEdge) == Qt::BottomEdge)) {
            anchor = XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
        } else if ((edge_data & ~Qt::BottomEdge) == Qt::Edges()) {
            anchor = XDG_POSITIONER_ANCHOR_BOTTOM;
        }
    } else if (edge_data.testFlag(Qt::RightEdge) && ((edge_data & ~Qt::RightEdge) == Qt::Edges())) {
        anchor = XDG_POSITIONER_ANCHOR_RIGHT;
    } else if (edge_data.testFlag(Qt::LeftEdge) && ((edge_data & ~Qt::LeftEdge) == Qt::Edges())) {
        anchor = XDG_POSITIONER_ANCHOR_LEFT;
    }
    if (anchor != 0) {
        xdg_positioner_set_anchor(d_ptr->positioner_ptr, anchor);
    }

    auto const gravity_data = data.gravity;
    uint32_t gravity = XDG_POSITIONER_GRAVITY_NONE;

    if (gravity_data.testFlag(Qt::TopEdge)) {
        if (gravity_data.testFlag(Qt::LeftEdge)
            && ((gravity_data & ~Qt::LeftEdge) == Qt::TopEdge)) {
            gravity = XDG_POSITIONER_GRAVITY_TOP_LEFT;
        } else if (gravity_data.testFlag(Qt::RightEdge)
                   && ((gravity_data & ~Qt::RightEdge) == Qt::TopEdge)) {
            gravity = XDG_POSITIONER_GRAVITY_TOP_RIGHT;
        } else if ((gravity_data & ~Qt::TopEdge) == Qt::Edges()) {
            gravity = XDG_POSITIONER_GRAVITY_TOP;
        }
    } else if (gravity_data.testFlag(Qt::BottomEdge)) {
        if (gravity_data.testFlag(Qt::LeftEdge)
            && ((gravity_data & ~Qt::LeftEdge) == Qt::BottomEdge)) {
            gravity = XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
        } else if (gravity_data.testFlag(Qt::RightEdge)
                   && ((gravity_data & ~Qt::RightEdge) == Qt::BottomEdge)) {
            gravity = XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
        } else if ((gravity_data & ~Qt::BottomEdge) == Qt::Edges()) {
            gravity = XDG_POSITIONER_GRAVITY_BOTTOM;
        }
    } else if (gravity_data.testFlag(Qt::RightEdge)
               && ((gravity_data & ~Qt::RightEdge) == Qt::Edges())) {
        gravity = XDG_POSITIONER_GRAVITY_RIGHT;
    } else if (gravity_data.testFlag(Qt::LeftEdge)
               && ((gravity_data & ~Qt::LeftEdge) == Qt::Edges())) {
        gravity = XDG_POSITIONER_GRAVITY_LEFT;
    }
    if (gravity != 0) {
        xdg_positioner_set_gravity(d_ptr->positioner_ptr, gravity);
    }

    auto const constraint_data = data.constraint_adjustments;
    uint32_t constraint = XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE;

    if (constraint_data.testFlag(xdg_shell_constraint_adjustment::slide_x)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
    }
    if (constraint_data.testFlag(xdg_shell_constraint_adjustment::slide_y)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
    }
    if (constraint_data.testFlag(xdg_shell_constraint_adjustment::flip_x)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
    }
    if (constraint_data.testFlag(xdg_shell_constraint_adjustment::flip_y)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
    }
    if (constraint_data.testFlag(xdg_shell_constraint_adjustment::resize_x)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
    }
    if (constraint_data.testFlag(xdg_shell_constraint_adjustment::resize_y)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
    }
    if (constraint != 0) {
        xdg_positioner_set_constraint_adjustment(d_ptr->positioner_ptr, constraint);
    }
}

}
