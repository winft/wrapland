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

    XdgShellToplevel* getXdgToplevel(Surface* surface, QObject* parent);

    XdgShellPopup* getXdgPopup(Surface* surface,
                               XdgShellToplevel* parentSurface,
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

XdgShellPopup* XdgShell::Private::getXdgPopup(Surface* surface,
                                              XdgShellToplevel* parentSurface,
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

XdgShellPopup* XdgShell::createPopup(Surface* surface,
                                     XdgShellToplevel* parentSurface,
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

class Q_DECL_HIDDEN XdgShellToplevel::Private
{
public:
    Private(XdgShellToplevel* q);
    virtual ~Private();
    EventQueue* queue = nullptr;
    QSize size;

    void setup(xdg_surface* surface, xdg_toplevel* toplevel);
    void release();
    bool isValid() const;

    operator xdg_surface*()
    {
        return xdgsurface;
    }
    operator xdg_surface*() const
    {
        return xdgsurface;
    }
    operator xdg_toplevel*()
    {
        return xdgtoplevel;
    }
    operator xdg_toplevel*() const
    {
        return xdgtoplevel;
    }

    void setTransientFor(XdgShellToplevel* parent);
    void setTitle(const QString& title);
    void setAppId(const QByteArray& appId);
    void showWindowMenu(Seat* seat, quint32 serial, qint32 x, qint32 y);
    void move(Seat* seat, quint32 serial);
    void resize(Seat* seat, quint32 serial, Qt::Edges edges);
    void ackConfigure(quint32 serial);
    void setMaximized();
    void unsetMaximized();
    void setFullscreen(Output* output);
    void unsetFullscreen();
    void setMinimized();
    void setMaxSize(const QSize& size);
    void setMinSize(const QSize& size);
    void setWindowGeometry(const QRect& windowGeometry);

private:
    WaylandPointer<xdg_toplevel, xdg_toplevel_destroy> xdgtoplevel;
    WaylandPointer<xdg_surface, xdg_surface_destroy> xdgsurface;

    QSize pendingSize;
    States pendingState;

    static void configureCallback(void* data,
                                  struct xdg_toplevel* xdg_toplevel,
                                  int32_t width,
                                  int32_t height,
                                  struct wl_array* state);
    static void closeCallback(void* data, xdg_toplevel* xdg_toplevel);
    static void surfaceConfigureCallback(void* data, xdg_surface* xdg_surface, uint32_t serial);

    static struct xdg_toplevel_listener const s_toplevelListener;
    static struct xdg_surface_listener const s_surfaceListener;

    XdgShellToplevel* q_ptr;
};

XdgShellToplevel::Private::Private(XdgShellToplevel* q)
    : q_ptr(q)
{
}

XdgShellToplevel::Private::~Private() = default;

struct xdg_toplevel_listener const XdgShellToplevel::Private::s_toplevelListener = {
    configureCallback,
    closeCallback,
};

struct xdg_surface_listener const XdgShellToplevel::Private::s_surfaceListener = {
    surfaceConfigureCallback,
};

void XdgShellToplevel::Private::surfaceConfigureCallback(void* data,
                                                         struct xdg_surface* surface,
                                                         uint32_t serial)
{
    Q_UNUSED(surface)
    auto s = static_cast<Private*>(data);
    s->q_ptr->configureRequested(s->pendingSize, s->pendingState, serial);
    if (!s->pendingSize.isNull()) {
        s->q_ptr->setSize(s->pendingSize);
        s->pendingSize = QSize();
    }
    s->pendingState = {};
}

void XdgShellToplevel::Private::configureCallback(void* data,
                                                  struct xdg_toplevel* xdg_toplevel,
                                                  int32_t width,
                                                  int32_t height,
                                                  struct wl_array* state)
{
    Q_UNUSED(xdg_toplevel)
    auto s = static_cast<Private*>(data);
    States states;

    uint32_t* statePtr = static_cast<uint32_t*>(state->data);
    for (size_t i = 0; i < state->size / sizeof(uint32_t); i++) {
        switch (statePtr[i]) {
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
            states = states | XdgShellToplevel::State::Maximized;
            break;
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
            states = states | XdgShellToplevel::State::Fullscreen;
            break;
        case XDG_TOPLEVEL_STATE_RESIZING:
            states = states | XdgShellToplevel::State::Resizing;
            break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
            states = states | XdgShellToplevel::State::Activated;
            break;
        case XDG_TOPLEVEL_STATE_TILED_LEFT:
            states = states | XdgShellToplevel::State::TiledLeft;
            break;
        case XDG_TOPLEVEL_STATE_TILED_RIGHT:
            states = states | XdgShellToplevel::State::TiledRight;
            break;
        case XDG_TOPLEVEL_STATE_TILED_TOP:
            states = states | XdgShellToplevel::State::TiledTop;
            break;
        case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
            states = states | XdgShellToplevel::State::TiledBottom;
            break;
        }
    }
    s->pendingSize = QSize(width, height);
    s->pendingState = states;
}

void XdgShellToplevel::Private::closeCallback(void* data, xdg_toplevel* xdg_toplevel)
{
    auto s = static_cast<XdgShellToplevel::Private*>(data);
    Q_ASSERT(s->xdgtoplevel == xdg_toplevel);
    Q_EMIT s->q_ptr->closeRequested();
}

void XdgShellToplevel::Private::setup(xdg_surface* surface, xdg_toplevel* topLevel)
{
    Q_ASSERT(surface);
    Q_ASSERT(!xdgtoplevel);
    xdgsurface.setup(surface);
    xdgtoplevel.setup(topLevel);
    xdg_surface_add_listener(xdgsurface, &s_surfaceListener, this);
    xdg_toplevel_add_listener(xdgtoplevel, &s_toplevelListener, this);
}

void XdgShellToplevel::Private::release()
{
    xdgtoplevel.release();
    xdgsurface.release();
}

bool XdgShellToplevel::Private::isValid() const
{
    return xdgtoplevel.isValid() && xdgsurface.isValid();
}

void XdgShellToplevel::Private::setTransientFor(XdgShellToplevel* parent)
{
    xdg_toplevel* parentSurface = nullptr;
    if (parent) {
        parentSurface = *parent;
    }
    xdg_toplevel_set_parent(xdgtoplevel, parentSurface);
}

void XdgShellToplevel::Private::setTitle(const QString& title)
{
    xdg_toplevel_set_title(xdgtoplevel, title.toUtf8().constData());
}

void XdgShellToplevel::Private::setAppId(const QByteArray& appId)
{
    xdg_toplevel_set_app_id(xdgtoplevel, appId.constData());
}

void XdgShellToplevel::Private::showWindowMenu(Seat* seat, quint32 serial, qint32 x, qint32 y)
{
    xdg_toplevel_show_window_menu(xdgtoplevel, *seat, serial, x, y);
}

void XdgShellToplevel::Private::move(Seat* seat, quint32 serial)
{
    xdg_toplevel_move(xdgtoplevel, *seat, serial);
}

void XdgShellToplevel::Private::resize(Seat* seat, quint32 serial, Qt::Edges edges)
{
    uint wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
    if (edges.testFlag(Qt::TopEdge)) {
        if (edges.testFlag(Qt::LeftEdge) && ((edges & ~Qt::LeftEdge) == Qt::TopEdge)) {
            wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
        } else if (edges.testFlag(Qt::RightEdge) && ((edges & ~Qt::RightEdge) == Qt::TopEdge)) {
            wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
        } else if ((edges & ~Qt::TopEdge) == Qt::Edges()) {
            wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
        }
    } else if (edges.testFlag(Qt::BottomEdge)) {
        if (edges.testFlag(Qt::LeftEdge) && ((edges & ~Qt::LeftEdge) == Qt::BottomEdge)) {
            wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
        } else if (edges.testFlag(Qt::RightEdge) && ((edges & ~Qt::RightEdge) == Qt::BottomEdge)) {
            wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
        } else if ((edges & ~Qt::BottomEdge) == Qt::Edges()) {
            wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
        }
    } else if (edges.testFlag(Qt::RightEdge) && ((edges & ~Qt::RightEdge) == Qt::Edges())) {
        wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
    } else if (edges.testFlag(Qt::LeftEdge) && ((edges & ~Qt::LeftEdge) == Qt::Edges())) {
        wlEdge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
    }
    xdg_toplevel_resize(xdgtoplevel, *seat, serial, wlEdge);
}

void XdgShellToplevel::Private::ackConfigure(quint32 serial)
{
    xdg_surface_ack_configure(xdgsurface, serial);
}

void XdgShellToplevel::Private::setMaximized()
{
    xdg_toplevel_set_maximized(xdgtoplevel);
}

void XdgShellToplevel::Private::unsetMaximized()
{
    xdg_toplevel_unset_maximized(xdgtoplevel);
}

void XdgShellToplevel::Private::setFullscreen(Output* output)
{
    wl_output* o = nullptr;
    if (output) {
        o = *output;
    }
    xdg_toplevel_set_fullscreen(xdgtoplevel, o);
}

void XdgShellToplevel::Private::unsetFullscreen()
{
    xdg_toplevel_unset_fullscreen(xdgtoplevel);
}

void XdgShellToplevel::Private::setMinimized()
{
    xdg_toplevel_set_minimized(xdgtoplevel);
}

void XdgShellToplevel::Private::setMaxSize(const QSize& size)
{
    xdg_toplevel_set_max_size(xdgtoplevel, size.width(), size.height());
}

void XdgShellToplevel::Private::setMinSize(const QSize& size)
{
    xdg_toplevel_set_min_size(xdgtoplevel, size.width(), size.height());
}

void XdgShellToplevel::Private::setWindowGeometry(const QRect& windowGeometry)
{
    xdg_surface_set_window_geometry(xdgsurface,
                                    windowGeometry.x(),
                                    windowGeometry.y(),
                                    windowGeometry.width(),
                                    windowGeometry.height());
}

XdgShellToplevel::XdgShellToplevel(QObject* parent)
    : QObject(parent)
    , d_ptr{new Private(this)}
{
}

XdgShellToplevel::~XdgShellToplevel()
{
    release();
}

void XdgShellToplevel::setup(xdg_surface* xdgsurface, xdg_toplevel* xdgtoplevel)
{
    d_ptr->setup(xdgsurface, xdgtoplevel);
}

void XdgShellToplevel::release()
{
    d_ptr->release();
}

void XdgShellToplevel::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* XdgShellToplevel::eventQueue()
{
    return d_ptr->queue;
}

XdgShellToplevel::operator xdg_surface*()
{
    return *(d_ptr.get());
}

XdgShellToplevel::operator xdg_surface*() const
{
    return *(d_ptr.get());
}

XdgShellToplevel::operator xdg_toplevel*()
{
    return *(d_ptr.get());
}

XdgShellToplevel::operator xdg_toplevel*() const
{
    return *(d_ptr.get());
}

bool XdgShellToplevel::isValid() const
{
    return d_ptr->isValid();
}

void XdgShellToplevel::setTransientFor(XdgShellToplevel* parent)
{
    d_ptr->setTransientFor(parent);
}

void XdgShellToplevel::setTitle(const QString& title)
{
    d_ptr->setTitle(title);
}

void XdgShellToplevel::setAppId(const QByteArray& appId)
{
    d_ptr->setAppId(appId);
}

void XdgShellToplevel::requestShowWindowMenu(Seat* seat, quint32 serial, const QPoint& pos)
{
    d_ptr->showWindowMenu(seat, serial, pos.x(), pos.y());
}

void XdgShellToplevel::requestMove(Seat* seat, quint32 serial)
{
    d_ptr->move(seat, serial);
}

void XdgShellToplevel::requestResize(Seat* seat, quint32 serial, Qt::Edges edges)
{
    d_ptr->resize(seat, serial, edges);
}

void XdgShellToplevel::ackConfigure(quint32 serial)
{
    d_ptr->ackConfigure(serial);
}

void XdgShellToplevel::setMaximized(bool set)
{
    if (set) {
        d_ptr->setMaximized();
    } else {
        d_ptr->unsetMaximized();
    }
}

void XdgShellToplevel::setFullscreen(bool set, Output* output)
{
    if (set) {
        d_ptr->setFullscreen(output);
    } else {
        d_ptr->unsetFullscreen();
    }
}

void XdgShellToplevel::setMaxSize(const QSize& size)
{
    d_ptr->setMaxSize(size);
}

void XdgShellToplevel::setMinSize(const QSize& size)
{
    d_ptr->setMinSize(size);
}

void XdgShellToplevel::setWindowGeometry(const QRect& windowGeometry)
{
    d_ptr->setWindowGeometry(windowGeometry);
}

void XdgShellToplevel::requestMinimize()
{
    d_ptr->setMinimized();
}

void XdgShellToplevel::setSize(const QSize& size)
{
    if (d_ptr->size == size) {
        return;
    }
    d_ptr->size = size;
    emit sizeChanged(size);
}

QSize XdgShellToplevel::size() const
{
    return d_ptr->size;
}

class Q_DECL_HIDDEN XdgShellPopup::Private
{
public:
    Private(XdgShellPopup* q);
    virtual ~Private();

    EventQueue* queue = nullptr;

    void setup(xdg_surface* s, xdg_popup* p);
    void release();
    bool isValid() const;
    void requestGrab(Seat* seat, quint32 serial);
    void ackConfigure(quint32 serial);
    void setWindowGeometry(const QRect& windowGeometry);

    operator xdg_surface*()
    {
        return xdgsurface;
    }
    operator xdg_surface*() const
    {
        return xdgsurface;
    }
    operator xdg_popup*()
    {
        return xdgpopup;
    }
    operator xdg_popup*() const
    {
        return xdgpopup;
    }

private:
    static void configureCallback(void* data,
                                  xdg_popup* xdg_popup,
                                  int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height);
    static void popupDoneCallback(void* data, xdg_popup* xdg_popup);
    static void surfaceConfigureCallback(void* data, xdg_surface* xdg_surface, uint32_t serial);

    QRect pendingRect;

    static struct xdg_popup_listener const s_popupListener;
    static struct xdg_surface_listener const s_surfaceListener;

    WaylandPointer<xdg_surface, xdg_surface_destroy> xdgsurface;
    WaylandPointer<xdg_popup, xdg_popup_destroy> xdgpopup;

    XdgShellPopup* q_ptr;
};

struct xdg_popup_listener const XdgShellPopup::Private::s_popupListener = {
    configureCallback,
    popupDoneCallback,
};

struct xdg_surface_listener const XdgShellPopup::Private::s_surfaceListener = {
    surfaceConfigureCallback,
};

void XdgShellPopup::Private::configureCallback(void* data,
                                               xdg_popup* xdg_popup,
                                               int32_t x,
                                               int32_t y,
                                               int32_t width,
                                               int32_t height)
{
    Q_UNUSED(xdg_popup)
    auto s = static_cast<Private*>(data);
    s->pendingRect = QRect(x, y, width, height);
}

void XdgShellPopup::Private::surfaceConfigureCallback(void* data,
                                                      struct xdg_surface* surface,
                                                      uint32_t serial)
{
    Q_UNUSED(surface)
    auto s = static_cast<Private*>(data);
    s->q_ptr->configureRequested(s->pendingRect, serial);
    s->pendingRect = QRect();
}

void XdgShellPopup::Private::popupDoneCallback(void* data, xdg_popup* xdg_popup)
{
    auto s = static_cast<XdgShellPopup::Private*>(data);
    Q_ASSERT(s->xdgpopup == xdg_popup);
    Q_EMIT s->q_ptr->popupDone();
}

XdgShellPopup::Private::Private(XdgShellPopup* q)
    : q_ptr(q)
{
}

XdgShellPopup::Private::~Private() = default;

void XdgShellPopup::Private::setup(xdg_surface* s, xdg_popup* p)
{
    Q_ASSERT(p);
    Q_ASSERT(!xdgsurface);
    Q_ASSERT(!xdgpopup);

    xdgsurface.setup(s);
    xdgpopup.setup(p);
    xdg_surface_add_listener(xdgsurface, &s_surfaceListener, this);
    xdg_popup_add_listener(xdgpopup, &s_popupListener, this);
}

void XdgShellPopup::Private::release()
{
    xdgpopup.release();
}

bool XdgShellPopup::Private::isValid() const
{
    return xdgpopup.isValid();
}

void XdgShellPopup::Private::requestGrab(Seat* seat, quint32 serial)
{
    xdg_popup_grab(xdgpopup, *seat, serial);
}

void XdgShellPopup::Private::ackConfigure(quint32 serial)
{
    xdg_surface_ack_configure(xdgsurface, serial);
}

void XdgShellPopup::Private::setWindowGeometry(const QRect& windowGeometry)
{
    xdg_surface_set_window_geometry(xdgsurface,
                                    windowGeometry.x(),
                                    windowGeometry.y(),
                                    windowGeometry.width(),
                                    windowGeometry.height());
}

XdgShellPopup::XdgShellPopup(QObject* parent)
    : QObject(parent)
    , d_ptr{new Private(this)}
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
