/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdgshell_p.h"

#include "event_queue.h"
#include "output.h"
#include "seat.h"
#include "surface.h"
#include "wayland_pointer_p.h"

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

    XdgShellSurface* getXdgSurface(Surface* surface, QObject* parent);

    XdgShellPopup* getXdgPopup(Surface* surface,
                               XdgShellSurface* parentSurface,
                               const XdgPositioner& positioner,
                               QObject* parent);
    XdgShellPopup* getXdgPopup(Surface* surface,
                               XdgShellPopup* parentSurface,
                               const XdgPositioner& positioner,
                               QObject* parent);

    EventQueue* queue = nullptr;

private:
    XdgShellPopup* internalGetXdgPopup(Surface* surface,
                                       xdg_surface* parentSurface,
                                       const XdgPositioner& positioner,
                                       QObject* parent);
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

XdgShellSurface* XdgShell::Private::getXdgSurface(Surface* surface, QObject* parent)
{
    Q_ASSERT(isValid());
    auto ss = xdg_wm_base_get_xdg_surface(xdg_shell_base, *surface);

    if (!ss) {
        return nullptr;
    }

    auto s = new XdgTopLevelStable(parent);
    auto toplevel = xdg_surface_get_toplevel(ss);
    if (queue) {
        queue->addProxy(ss);
        queue->addProxy(toplevel);
    }
    s->setup(ss, toplevel);
    return s;
}

XdgShellPopup* XdgShell::Private::getXdgPopup(Surface* surface,
                                              XdgShellSurface* parentSurface,
                                              const XdgPositioner& positioner,
                                              QObject* parent)
{
    return internalGetXdgPopup(surface, *parentSurface, positioner, parent);
}

XdgShellPopup* XdgShell::Private::getXdgPopup(Surface* surface,
                                              XdgShellPopup* parentSurface,
                                              const XdgPositioner& positioner,
                                              QObject* parent)
{
    return internalGetXdgPopup(surface, *parentSurface, positioner, parent);
}

XdgShellPopup* XdgShell::Private::internalGetXdgPopup(Surface* surface,
                                                      xdg_surface* parentSurface,
                                                      const XdgPositioner& positioner,
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

    XdgShellPopup* s = new XdgShellPopupStable(parent);
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

XdgShellSurface* XdgShell::createSurface(Surface* surface, QObject* parent)
{
    return d_ptr->getXdgSurface(surface, parent);
}

XdgShellPopup* XdgShell::createPopup(Surface* surface,
                                     XdgShellSurface* parentSurface,
                                     const XdgPositioner& positioner,
                                     QObject* parent)
{
    return d_ptr->getXdgPopup(surface, parentSurface, positioner, parent);
}

XdgShellPopup* XdgShell::createPopup(Surface* surface,
                                     XdgShellPopup* parentSurface,
                                     const XdgPositioner& positioner,
                                     QObject* parent)
{
    return d_ptr->getXdgPopup(surface, parentSurface, positioner, parent);
}

XdgShellSurface::Private::Private(XdgShellSurface* q)
    : q_ptr(q)
{
}

XdgShellSurface::Private::~Private() = default;

XdgShellSurface::XdgShellSurface(Private* p, QObject* parent)
    : QObject(parent)
    , d_ptr(p)
{
}

XdgShellSurface::~XdgShellSurface()
{
    release();
}

void XdgShellSurface::setup(xdg_surface* xdgsurface, xdg_toplevel* xdgtoplevel)
{
    d_ptr->setup(xdgsurface, xdgtoplevel);
}

void XdgShellSurface::release()
{
    d_ptr->release();
}

void XdgShellSurface::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* XdgShellSurface::eventQueue()
{
    return d_ptr->queue;
}

XdgShellSurface::operator xdg_surface*()
{
    return *(d_ptr.get());
}

XdgShellSurface::operator xdg_surface*() const
{
    return *(d_ptr.get());
}

XdgShellSurface::operator xdg_toplevel*()
{
    return *(d_ptr.get());
}

XdgShellSurface::operator xdg_toplevel*() const
{
    return *(d_ptr.get());
}

bool XdgShellSurface::isValid() const
{
    return d_ptr->isValid();
}

void XdgShellSurface::setTransientFor(XdgShellSurface* parent)
{
    d_ptr->setTransientFor(parent);
}

void XdgShellSurface::setTitle(const QString& title)
{
    d_ptr->setTitle(title);
}

void XdgShellSurface::setAppId(const QByteArray& appId)
{
    d_ptr->setAppId(appId);
}

void XdgShellSurface::requestShowWindowMenu(Seat* seat, quint32 serial, const QPoint& pos)
{
    d_ptr->showWindowMenu(seat, serial, pos.x(), pos.y());
}

void XdgShellSurface::requestMove(Seat* seat, quint32 serial)
{
    d_ptr->move(seat, serial);
}

void XdgShellSurface::requestResize(Seat* seat, quint32 serial, Qt::Edges edges)
{
    d_ptr->resize(seat, serial, edges);
}

void XdgShellSurface::ackConfigure(quint32 serial)
{
    d_ptr->ackConfigure(serial);
}

void XdgShellSurface::setMaximized(bool set)
{
    if (set) {
        d_ptr->setMaximized();
    } else {
        d_ptr->unsetMaximized();
    }
}

void XdgShellSurface::setFullscreen(bool set, Output* output)
{
    if (set) {
        d_ptr->setFullscreen(output);
    } else {
        d_ptr->unsetFullscreen();
    }
}

void XdgShellSurface::setMaxSize(const QSize& size)
{
    d_ptr->setMaxSize(size);
}

void XdgShellSurface::setMinSize(const QSize& size)
{
    d_ptr->setMinSize(size);
}

void XdgShellSurface::setWindowGeometry(const QRect& windowGeometry)
{
    d_ptr->setWindowGeometry(windowGeometry);
}

void XdgShellSurface::requestMinimize()
{
    d_ptr->setMinimized();
}

void XdgShellSurface::setSize(const QSize& size)
{
    if (d_ptr->size == size) {
        return;
    }
    d_ptr->size = size;
    emit sizeChanged(size);
}

QSize XdgShellSurface::size() const
{
    return d_ptr->size;
}

XdgShellPopup::Private::~Private() = default;

XdgShellPopup::Private::Private(XdgShellPopup* q)
    : q_ptr(q)
{
}

XdgShellPopup::XdgShellPopup(Private* p, QObject* parent)
    : QObject(parent)
    , d_ptr(p)
{
}

XdgShellPopup::~XdgShellPopup()
{
    release();
}

void XdgShellPopup::setup(xdg_surface* surface, xdg_popup* popup)
{
    d_ptr->setup(surface, popup);
}

void XdgShellPopup::release()
{
    d_ptr->release();
}

void XdgShellPopup::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* XdgShellPopup::eventQueue()
{
    return d_ptr->queue;
}

void XdgShellPopup::requestGrab(Wrapland::Client::Seat* seat, quint32 serial)
{
    d_ptr->requestGrab(seat, serial);
}

void XdgShellPopup::ackConfigure(quint32 serial)
{
    d_ptr->ackConfigure(serial);
}

void XdgShellPopup::setWindowGeometry(const QRect& windowGeometry)
{
    d_ptr->setWindowGeometry(windowGeometry);
}

XdgShellPopup::operator xdg_surface*()
{
    return *(d_ptr.get());
}

XdgShellPopup::operator xdg_surface*() const
{
    return *(d_ptr.get());
}

XdgShellPopup::operator xdg_popup*()
{
    return *(d_ptr.get());
}

XdgShellPopup::operator xdg_popup*() const
{
    return *(d_ptr.get());
}

bool XdgShellPopup::isValid() const
{
    return d_ptr->isValid();
}

XdgPositioner::XdgPositioner(const QSize& initialSize, const QRect& anchor)
    : d_ptr(new Private)
{
    d_ptr->initialSize = initialSize;
    d_ptr->anchorRect = anchor;
}

XdgPositioner::XdgPositioner(const XdgPositioner& other)
    : d_ptr(new Private)
{
    *d_ptr = *other.d_ptr;
}

XdgPositioner::~XdgPositioner()
{
}

void XdgPositioner::setInitialSize(const QSize& size)
{
    d_ptr->initialSize = size;
}

QSize XdgPositioner::initialSize() const
{
    return d_ptr->initialSize;
}

void XdgPositioner::setAnchorRect(const QRect& anchor)
{
    d_ptr->anchorRect = anchor;
}

QRect XdgPositioner::anchorRect() const
{
    return d_ptr->anchorRect;
}

void XdgPositioner::setAnchorEdge(Qt::Edges edge)
{
    d_ptr->anchorEdge = edge;
}

Qt::Edges XdgPositioner::anchorEdge() const
{
    return d_ptr->anchorEdge;
}

void XdgPositioner::setAnchorOffset(const QPoint& offset)
{
    d_ptr->anchorOffset = offset;
}

QPoint XdgPositioner::anchorOffset() const
{
    return d_ptr->anchorOffset;
}

void XdgPositioner::setGravity(Qt::Edges edge)
{
    d_ptr->gravity = edge;
}

Qt::Edges XdgPositioner::gravity() const
{
    return d_ptr->gravity;
}

void XdgPositioner::setConstraints(Constraints constraints)
{
    d_ptr->constraints = constraints;
}

XdgPositioner::Constraints XdgPositioner::constraints() const
{
    return d_ptr->constraints;
}

}
