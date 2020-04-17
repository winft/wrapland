/****************************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
#include "xdgshell_p.h"
#include "event_queue.h"
#include "wayland_pointer_p.h"
#include "seat.h"
#include "surface.h"
#include "output.h"
#include "../compat/wayland-xdg-shell-v5-client-protocol.h"

namespace Wrapland
{
namespace Client
{

XdgShell::Private::~Private() = default;

XdgShell::XdgShell(Private *p, QObject *parent)
    : QObject(parent)
    , d(p)
{
}

XdgShell::~XdgShell()
{
    release();
}

void XdgShell::setup(xdg_shell *xdgshellv5)
{
    d->setupV5(xdgshellv5);
}

void XdgShell::setup(zxdg_shell_v6 *xdgshellv6)
{
    d->setupV6(xdgshellv6);
}

void XdgShell::setup(xdg_wm_base *xdg_wm_base)
{
    d->setup(xdg_wm_base);
}

void XdgShell::release()
{
    d->release();
}

void XdgShell::setEventQueue(EventQueue *queue)
{
    d->queue = queue;
}

EventQueue *XdgShell::eventQueue()
{
    return d->queue;
}

XdgShell::operator xdg_shell*() {
    return *(d.get());
}

XdgShell::operator xdg_shell*() const {
    return *(d.get());
}

XdgShell::operator zxdg_shell_v6*() {
    return *(d.get());
}

XdgShell::operator zxdg_shell_v6*() const {
    return *(d.get());
}

XdgShell::operator xdg_wm_base*() {
    return *(d.get());
}

XdgShell::operator xdg_wm_base*() const {
    return *(d.get());
}


bool XdgShell::isValid() const
{
    return d->isValid();
}

XdgShellSurface *XdgShell::createSurface(Surface *surface, QObject *parent)
{
    return d->getXdgSurface(surface, parent);
}

XdgShellPopup *XdgShell::createPopup(Surface *surface, Surface *parentSurface, Seat *seat, quint32 serial, const QPoint &parentPos, QObject *parent)
{
    return d->getXdgPopup(surface, parentSurface, seat, serial, parentPos, parent);
}

XdgShellPopup *XdgShell::createPopup(Surface *surface, XdgShellSurface *parentSurface, const XdgPositioner &positioner, QObject *parent)
{
    return d->getXdgPopup(surface, parentSurface, positioner, parent);
}

XdgShellPopup *XdgShell::createPopup(Surface *surface, XdgShellPopup *parentSurface, const XdgPositioner &positioner, QObject *parent)
{
    return d->getXdgPopup(surface, parentSurface, positioner, parent);
}

XdgShellSurface::Private::Private(XdgShellSurface *q)
    : q(q)
{
}

XdgShellSurface::Private::~Private() = default;

XdgShellSurface::XdgShellSurface(Private *p, QObject *parent)
    : QObject(parent)
    , d(p)
{
}

XdgShellSurface::~XdgShellSurface()
{
    release();
}

void XdgShellSurface::setup(xdg_surface *xdgsurfacev5)
{
    d->setupV5(xdgsurfacev5);
}

void XdgShellSurface::setup(zxdg_surface_v6 *xdgsurfacev6, zxdg_toplevel_v6 *xdgtoplevelv6)
{
    d->setupV6(xdgsurfacev6, xdgtoplevelv6);
}

void XdgShellSurface::setup(xdg_surface *xdgsurface, xdg_toplevel *xdgtoplevel)
{
    d->setup(xdgsurface, xdgtoplevel);
}


void XdgShellSurface::release()
{
    d->release();
}

void XdgShellSurface::setEventQueue(EventQueue *queue)
{
    d->queue = queue;
}

EventQueue *XdgShellSurface::eventQueue()
{
    return d->queue;
}

XdgShellSurface::operator xdg_surface*() {
    return *(d.get());
}

XdgShellSurface::operator xdg_surface*() const {
    return *(d.get());
}

XdgShellSurface::operator xdg_toplevel*() {
    return *(d.get());
}

XdgShellSurface::operator xdg_toplevel*() const {
    return *(d.get());
}

XdgShellSurface::operator zxdg_surface_v6*() {
    return *(d.get());
}

XdgShellSurface::operator zxdg_surface_v6*() const {
    return *(d.get());
}

XdgShellSurface::operator zxdg_toplevel_v6*() {
    return *(d.get());
}

XdgShellSurface::operator zxdg_toplevel_v6*() const {
    return *(d.get());
}

bool XdgShellSurface::isValid() const
{
    return d->isValid();
}

void XdgShellSurface::setTransientFor(XdgShellSurface *parent)
{
    d->setTransientFor(parent);
}

void XdgShellSurface::setTitle(const QString &title)
{
    d->setTitle(title);
}

void XdgShellSurface::setAppId(const QByteArray &appId)
{
    d->setAppId(appId);
}

void XdgShellSurface::requestShowWindowMenu(Seat *seat, quint32 serial, const QPoint &pos)
{
    d->showWindowMenu(seat, serial, pos.x(), pos.y());
}

void XdgShellSurface::requestMove(Seat *seat, quint32 serial)
{
    d->move(seat, serial);
}

void XdgShellSurface::requestResize(Seat *seat, quint32 serial, Qt::Edges edges)
{
    d->resize(seat, serial, edges);
}

void XdgShellSurface::ackConfigure(quint32 serial)
{
    d->ackConfigure(serial);
}

void XdgShellSurface::setMaximized(bool set)
{
    if (set) {
        d->setMaximized();
    } else {
        d->unsetMaximized();
    }
}

void XdgShellSurface::setFullscreen(bool set, Output *output)
{
    if (set) {
        d->setFullscreen(output);
    } else {
        d->unsetFullscreen();
    }
}

void XdgShellSurface::setMaxSize(const QSize &size)
{
    d->setMaxSize(size);
}

void XdgShellSurface::setMinSize(const QSize &size)
{
    d->setMinSize(size);
}

void XdgShellSurface::setWindowGeometry(const QRect &windowGeometry)
{
    d->setWindowGeometry(windowGeometry);
}

void XdgShellSurface::requestMinimize()
{
    d->setMinimized();
}

void XdgShellSurface::setSize(const QSize &size)
{
    if (d->size == size) {
        return;
    }
    d->size = size;
    emit sizeChanged(size);
}

QSize XdgShellSurface::size() const
{
    return d->size;
}

XdgShellPopup::Private::~Private() = default;


XdgShellPopup::Private::Private(XdgShellPopup *q)
    : q(q)
{
}

XdgShellPopup::XdgShellPopup(Private *p, QObject *parent)
    : QObject(parent)
    , d(p)
{
}

XdgShellPopup::~XdgShellPopup()
{
    release();
}

void XdgShellPopup::setup(xdg_popup *xdgpopupv5)
{
    d->setupV5(xdgpopupv5);
}

void XdgShellPopup::setup(zxdg_surface_v6 *xdgsurfacev6, zxdg_popup_v6 *xdgpopupv6)
{
    d->setupV6(xdgsurfacev6, xdgpopupv6);
}

void XdgShellPopup::setup(xdg_surface *surface, xdg_popup *popup)
{
    d->setup(surface, popup);
}

void XdgShellPopup::release()
{
    d->release();
}

void XdgShellPopup::setEventQueue(EventQueue *queue)
{
    d->queue = queue;
}

EventQueue *XdgShellPopup::eventQueue()
{
    return d->queue;
}

void XdgShellPopup::requestGrab(Wrapland::Client::Seat* seat, quint32 serial)
{
    d->requestGrab(seat, serial);
}

void XdgShellPopup::ackConfigure(quint32 serial)
{
    d->ackConfigure(serial);
}

void XdgShellPopup::setWindowGeometry(const QRect &windowGeometry)
{
    d->setWindowGeometry(windowGeometry);
}

XdgShellPopup::operator xdg_surface*() {
    return *(d.get());
}

XdgShellPopup::operator xdg_surface*() const {
    return *(d.get());
}

XdgShellPopup::operator xdg_popup*() {
    return *(d.get());
}

XdgShellPopup::operator xdg_popup*() const {
    return *(d.get());
}

XdgShellPopup::operator zxdg_surface_v6*() {
    return *(d.get());
}

XdgShellPopup::operator zxdg_surface_v6*() const {
    return *(d.get());
}

XdgShellPopup::operator zxdg_popup_v6*() {
    return *(d.get());
}

XdgShellPopup::operator zxdg_popup_v6*() const {
    return *(d.get());
}

bool XdgShellPopup::isValid() const
{
    return d->isValid();
}

XdgPositioner::XdgPositioner(const QSize& initialSize, const QRect& anchor)
:d (new Private)
{
    d->initialSize = initialSize;
    d->anchorRect = anchor;
}


XdgPositioner::XdgPositioner(const XdgPositioner &other)
:d (new Private)
{
    *d = *other.d;
}

XdgPositioner::~XdgPositioner()
{
}

void XdgPositioner::setInitialSize(const QSize& size)
{
    d->initialSize = size;
}

QSize XdgPositioner::initialSize() const
{
    return d->initialSize;
}

void XdgPositioner::setAnchorRect(const QRect& anchor)
{
    d->anchorRect = anchor;
}

QRect XdgPositioner::anchorRect() const
{
    return d->anchorRect;
}

void XdgPositioner::setAnchorEdge(Qt::Edges edge)
{
    d->anchorEdge = edge;
}

Qt::Edges XdgPositioner::anchorEdge() const
{
    return d->anchorEdge;
}

void XdgPositioner::setAnchorOffset(const QPoint& offset)
{
    d->anchorOffset = offset;
}

QPoint XdgPositioner::anchorOffset() const
{
    return d->anchorOffset;
}

void XdgPositioner::setGravity(Qt::Edges edge)
{
    d->gravity = edge;
}

Qt::Edges XdgPositioner::gravity() const
{
    return d->gravity;
}

void XdgPositioner::setConstraints(Constraints constraints)
{
    d->constraints = constraints;
}

XdgPositioner::Constraints XdgPositioner::constraints() const
{
    return d->constraints;
}


}
}

