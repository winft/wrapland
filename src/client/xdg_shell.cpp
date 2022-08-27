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
                                 XdgPositioner const& positioner,
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
                                                XdgPositioner const& positioner,
                                                QObject* parent)
{
    Q_ASSERT(isValid());
    auto ss = xdg_wm_base_get_xdg_surface(xdg_shell_base, *surface);
    if (!ss) {
        return nullptr;
    }

    auto p = xdg_wm_base_create_positioner(xdg_shell_base);

    auto anchorRect = positioner.anchorRect();
    xdg_positioner_set_anchor_rect(
        p, anchorRect.x(), anchorRect.y(), anchorRect.width(), anchorRect.height());

    QSize initialSize = positioner.initialSize();
    xdg_positioner_set_size(p, initialSize.width(), initialSize.height());

    QPoint anchorOffset = positioner.anchorOffset();
    if (!anchorOffset.isNull()) {
        xdg_positioner_set_offset(p, anchorOffset.x(), anchorOffset.y());
    }

    uint32_t anchor = XDG_POSITIONER_ANCHOR_NONE;
    if (positioner.anchorEdge().testFlag(Qt::TopEdge)) {
        if (positioner.anchorEdge().testFlag(Qt::LeftEdge)
            && ((positioner.anchorEdge() & ~Qt::LeftEdge) == Qt::TopEdge)) {
            anchor = XDG_POSITIONER_ANCHOR_TOP_LEFT;
        } else if (positioner.anchorEdge().testFlag(Qt::RightEdge)
                   && ((positioner.anchorEdge() & ~Qt::RightEdge) == Qt::TopEdge)) {
            anchor = XDG_POSITIONER_ANCHOR_TOP_RIGHT;
        } else if ((positioner.anchorEdge() & ~Qt::TopEdge) == Qt::Edges()) {
            anchor = XDG_POSITIONER_ANCHOR_TOP;
        }
    } else if (positioner.anchorEdge().testFlag(Qt::BottomEdge)) {
        if (positioner.anchorEdge().testFlag(Qt::LeftEdge)
            && ((positioner.anchorEdge() & ~Qt::LeftEdge) == Qt::BottomEdge)) {
            anchor = XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
        } else if (positioner.anchorEdge().testFlag(Qt::RightEdge)
                   && ((positioner.anchorEdge() & ~Qt::RightEdge) == Qt::BottomEdge)) {
            anchor = XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
        } else if ((positioner.anchorEdge() & ~Qt::BottomEdge) == Qt::Edges()) {
            anchor = XDG_POSITIONER_ANCHOR_BOTTOM;
        }
    } else if (positioner.anchorEdge().testFlag(Qt::RightEdge)
               && ((positioner.anchorEdge() & ~Qt::RightEdge) == Qt::Edges())) {
        anchor = XDG_POSITIONER_ANCHOR_RIGHT;
    } else if (positioner.anchorEdge().testFlag(Qt::LeftEdge)
               && ((positioner.anchorEdge() & ~Qt::LeftEdge) == Qt::Edges())) {
        anchor = XDG_POSITIONER_ANCHOR_LEFT;
    }
    if (anchor != 0) {
        xdg_positioner_set_anchor(p, anchor);
    }

    uint32_t gravity = XDG_POSITIONER_GRAVITY_NONE;
    if (positioner.gravity().testFlag(Qt::TopEdge)) {
        if (positioner.gravity().testFlag(Qt::LeftEdge)
            && ((positioner.gravity() & ~Qt::LeftEdge) == Qt::TopEdge)) {
            gravity = XDG_POSITIONER_GRAVITY_TOP_LEFT;
        } else if (positioner.gravity().testFlag(Qt::RightEdge)
                   && ((positioner.gravity() & ~Qt::RightEdge) == Qt::TopEdge)) {
            gravity = XDG_POSITIONER_GRAVITY_TOP_RIGHT;
        } else if ((positioner.gravity() & ~Qt::TopEdge) == Qt::Edges()) {
            gravity = XDG_POSITIONER_GRAVITY_TOP;
        }
    } else if (positioner.gravity().testFlag(Qt::BottomEdge)) {
        if (positioner.gravity().testFlag(Qt::LeftEdge)
            && ((positioner.gravity() & ~Qt::LeftEdge) == Qt::BottomEdge)) {
            gravity = XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
        } else if (positioner.gravity().testFlag(Qt::RightEdge)
                   && ((positioner.gravity() & ~Qt::RightEdge) == Qt::BottomEdge)) {
            gravity = XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
        } else if ((positioner.gravity() & ~Qt::BottomEdge) == Qt::Edges()) {
            gravity = XDG_POSITIONER_GRAVITY_BOTTOM;
        }
    } else if (positioner.gravity().testFlag(Qt::RightEdge)
               && ((positioner.gravity() & ~Qt::RightEdge) == Qt::Edges())) {
        gravity = XDG_POSITIONER_GRAVITY_RIGHT;
    } else if (positioner.gravity().testFlag(Qt::LeftEdge)
               && ((positioner.gravity() & ~Qt::LeftEdge) == Qt::Edges())) {
        gravity = XDG_POSITIONER_GRAVITY_LEFT;
    }
    if (gravity != 0) {
        xdg_positioner_set_gravity(p, gravity);
    }

    uint32_t constraint = XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE;
    if (positioner.constraints().testFlag(XdgPositioner::Constraint::SlideX)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
    }
    if (positioner.constraints().testFlag(XdgPositioner::Constraint::SlideY)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
    }
    if (positioner.constraints().testFlag(XdgPositioner::Constraint::FlipX)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
    }
    if (positioner.constraints().testFlag(XdgPositioner::Constraint::FlipY)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
    }
    if (positioner.constraints().testFlag(XdgPositioner::Constraint::ResizeX)) {
        constraint |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
    }
    if (positioner.constraints().testFlag(XdgPositioner::Constraint::ResizeY)) {
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
                                      XdgPositioner const& positioner,
                                      QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, *parentSurface, positioner, parent);
}

XdgShellPopup* XdgShell::create_popup(Surface* surface,
                                      XdgShellPopup* parentSurface,
                                      XdgPositioner const& positioner,
                                      QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, *parentSurface, positioner, parent);
}

XdgShellPopup*
XdgShell::create_popup(Surface* surface, XdgPositioner const& positioner, QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, nullptr, positioner, parent);
}

}
