/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_shell.h"

#include "xdg_shell_popup.h"
#include "xdg_shell_positioner.h"
#include "xdg_shell_toplevel.h"

#include "event_queue.h"
#include "seat.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <wayland-xdg-shell-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN XdgShell::Private
{
public:
    virtual ~Private();

    void setup(xdg_wm_base* shell);
    void release();
    bool isValid() const;

    operator xdg_wm_base*()
    {
        return xdg_shell_base;
    }
    operator xdg_wm_base*() const
    {
        return xdg_shell_base;
    }

    XdgShellToplevel* getXdgToplevel(Surface* surface, QObject* parent);
    XdgShellPopup* get_xdg_popup(Surface* surface,
                                 xdg_surface* parentSurface,
                                 xdg_shell_positioner_data const& positioner_data,
                                 QObject* parent);

    EventQueue* queue = nullptr;

private:
    static void pingCallback(void* data, struct xdg_wm_base* shell, uint32_t serial);

    WaylandPointer<xdg_wm_base, xdg_wm_base_destroy> xdg_shell_base;
    static const struct xdg_wm_base_listener s_shellListener;
};

XdgShell::Private::~Private() = default;

struct xdg_wm_base_listener const XdgShell::Private::s_shellListener = {
    pingCallback,
};

void XdgShell::Private::pingCallback(void* data, struct xdg_wm_base* shell, uint32_t serial)
{
    Q_UNUSED(data)
    xdg_wm_base_pong(shell, serial);
}

void XdgShell::Private::setup(xdg_wm_base* shell)
{
    Q_ASSERT(shell);
    Q_ASSERT(!xdg_shell_base);
    xdg_shell_base.setup(shell);
    xdg_wm_base_add_listener(shell, &s_shellListener, this);
}

void XdgShell::Private::release()
{
    xdg_shell_base.release();
}

bool XdgShell::Private::isValid() const
{
    return xdg_shell_base.isValid();
}

XdgShellToplevel* XdgShell::Private::getXdgToplevel(Surface* surface, QObject* parent)
{
    Q_ASSERT(isValid());
    auto ss = xdg_wm_base_get_xdg_surface(xdg_shell_base, *surface);

    if (!ss) {
        return nullptr;
    }

    auto s = new XdgShellToplevel(parent);
    auto toplevel = xdg_surface_get_toplevel(ss);
    if (queue) {
        queue->addProxy(ss);
        queue->addProxy(toplevel);
    }
    s->setup(ss, toplevel);
    return s;
}

XdgShellPopup* XdgShell::Private::get_xdg_popup(Surface* surface,
                                                xdg_surface* parentSurface,
                                                xdg_shell_positioner_data const& positioner_data,
                                                QObject* parent)
{
    Q_ASSERT(isValid());
    auto ss = xdg_wm_base_get_xdg_surface(xdg_shell_base, *surface);
    if (!ss) {
        return nullptr;
    }

    auto p = xdg_wm_base_create_positioner(xdg_shell_base);

    auto anchorRect = positioner_data.anchor.rect;
    xdg_positioner_set_anchor_rect(
        p, anchorRect.x(), anchorRect.y(), anchorRect.width(), anchorRect.height());

    xdg_positioner_set_size(p, positioner_data.size.width(), positioner_data.size.height());

    if (auto anchorOffset = positioner_data.anchor.offset; !anchorOffset.isNull()) {
        xdg_positioner_set_offset(p, anchorOffset.x(), anchorOffset.y());
    }

    auto const edge_data = positioner_data.anchor.edge;
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
        xdg_positioner_set_anchor(p, anchor);
    }

    auto const gravity_data = positioner_data.gravity;
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
        xdg_positioner_set_gravity(p, gravity);
    }

    auto const constraint_data = positioner_data.constraint_adjustments;
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
        xdg_positioner_set_constraint_adjustment(p, constraint);
    }

    XdgShellPopup* s = new XdgShellPopup(parent);
    auto popup = xdg_surface_get_popup(ss, parentSurface, p);
    if (queue) {
        // deliberately not adding the positioner because the positioner has no events sent to it
        queue->addProxy(ss);
        queue->addProxy(popup);
    }
    s->setup(ss, popup);

    xdg_positioner_destroy(p);

    return s;
}

XdgShell::XdgShell(QObject* parent)
    : QObject(parent)
    , d_ptr{new Private}
{
}

XdgShell::~XdgShell()
{
    release();
}

void XdgShell::setup(xdg_wm_base* xdg_wm_base)
{
    d_ptr->setup(xdg_wm_base);
}

void XdgShell::release()
{
    d_ptr->release();
}

void XdgShell::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* XdgShell::eventQueue()
{
    return d_ptr->queue;
}

XdgShell::operator xdg_wm_base*()
{
    return *(d_ptr.get());
}

XdgShell::operator xdg_wm_base*() const
{
    return *(d_ptr.get());
}

bool XdgShell::isValid() const
{
    return d_ptr->isValid();
}

XdgShellToplevel* XdgShell::create_toplevel(Surface* surface, QObject* parent)
{
    return d_ptr->getXdgToplevel(surface, parent);
}

XdgShellPopup* XdgShell::create_popup(Surface* surface,
                                      XdgShellToplevel* parentSurface,
                                      xdg_shell_positioner_data const& positioner_data,
                                      QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, *parentSurface, positioner_data, parent);
}

XdgShellPopup* XdgShell::create_popup(Surface* surface,
                                      XdgShellPopup* parentSurface,
                                      xdg_shell_positioner_data const& positioner_data,
                                      QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, *parentSurface, positioner_data, parent);
}

XdgShellPopup* XdgShell::create_popup(Surface* surface,
                                      xdg_shell_positioner_data const& positioner_data,
                                      QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, nullptr, positioner_data, parent);
}

}
