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

XdgShell::Private::~Private() = default;

XdgShell::XdgShell(Private* p, QObject* parent)
    : QObject(parent)
    , d_ptr(p)
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
