/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_shell_popup.h"

#include "event_queue.h"
#include "seat.h"
#include "wayland_pointer_p.h"
#include "xdg_shell_positioner.h"

#include <wayland-xdg-shell-client-protocol.h>

namespace Wrapland::Client
{

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
    void setWindowGeometry(QRect const& windowGeometry);

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

void XdgShellPopup::Private::setWindowGeometry(QRect const& windowGeometry)
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

void XdgShellPopup::setWindowGeometry(QRect const& windowGeometry)
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

}
