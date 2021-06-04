/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_shell_toplevel.h"

#include "event_queue.h"
#include "output.h"
#include "seat.h"
#include "wayland_pointer_p.h"

#include <wayland-xdg-shell-client-protocol.h>

namespace Wrapland::Client
{

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

}