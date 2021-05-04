/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>
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

class XdgShellPopupStable::Private : public XdgShellPopup::Private
{
public:
    Private(XdgShellPopup* q);

    void setup(xdg_surface* s, xdg_popup* p) override;
    void release() override;
    bool isValid() const override;
    void requestGrab(Seat* seat, quint32 serial) override;
    void ackConfigure(quint32 serial) override;
    void setWindowGeometry(const QRect& windowGeometry) override;

    operator xdg_surface*() override
    {
        return xdgsurface;
    }
    operator xdg_surface*() const override
    {
        return xdgsurface;
    }
    operator xdg_popup*() override
    {
        return xdgpopup;
    }
    operator xdg_popup*() const override
    {
        return xdgpopup;
    }
    WaylandPointer<xdg_surface, xdg_surface_destroy> xdgsurface;
    WaylandPointer<xdg_popup, xdg_popup_destroy> xdgpopup;

    QRect pendingRect;

private:
    static void configureCallback(void* data,
                                  xdg_popup* xdg_popup,
                                  int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height);
    static void popupDoneCallback(void* data, xdg_popup* xdg_popup);
    static void surfaceConfigureCallback(void* data, xdg_surface* xdg_surface, uint32_t serial);

    static const struct xdg_popup_listener s_popupListener;
    static const struct xdg_surface_listener s_surfaceListener;
};

const struct xdg_popup_listener XdgShellPopupStable::Private::s_popupListener = {
    configureCallback,
    popupDoneCallback,
};

const struct xdg_surface_listener XdgShellPopupStable::Private::s_surfaceListener = {
    surfaceConfigureCallback,
};

void XdgShellPopupStable::Private::configureCallback(void* data,
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

void XdgShellPopupStable::Private::surfaceConfigureCallback(void* data,
                                                            struct xdg_surface* surface,
                                                            uint32_t serial)
{
    Q_UNUSED(surface)
    auto s = static_cast<Private*>(data);
    s->q_ptr->configureRequested(s->pendingRect, serial);
    s->pendingRect = QRect();
}

void XdgShellPopupStable::Private::popupDoneCallback(void* data, xdg_popup* xdg_popup)
{
    auto s = static_cast<XdgShellPopupStable::Private*>(data);
    Q_ASSERT(s->xdgpopup == xdg_popup);
    Q_EMIT s->q_ptr->popupDone();
}

XdgShellPopupStable::Private::Private(XdgShellPopup* q)
    : XdgShellPopup::Private(q)
{
}

void XdgShellPopupStable::Private::setup(xdg_surface* s, xdg_popup* p)
{
    Q_ASSERT(p);
    Q_ASSERT(!xdgsurface);
    Q_ASSERT(!xdgpopup);

    xdgsurface.setup(s);
    xdgpopup.setup(p);
    xdg_surface_add_listener(xdgsurface, &s_surfaceListener, this);
    xdg_popup_add_listener(xdgpopup, &s_popupListener, this);
}

void XdgShellPopupStable::Private::release()
{
    xdgpopup.release();
}

bool XdgShellPopupStable::Private::isValid() const
{
    return xdgpopup.isValid();
}

void XdgShellPopupStable::Private::requestGrab(Seat* seat, quint32 serial)
{
    xdg_popup_grab(xdgpopup, *seat, serial);
}

void XdgShellPopupStable::Private::ackConfigure(quint32 serial)
{
    xdg_surface_ack_configure(xdgsurface, serial);
}

void XdgShellPopupStable::Private::setWindowGeometry(const QRect& windowGeometry)
{
    xdg_surface_set_window_geometry(xdgsurface,
                                    windowGeometry.x(),
                                    windowGeometry.y(),
                                    windowGeometry.width(),
                                    windowGeometry.height());
}

XdgShellPopupStable::XdgShellPopupStable(QObject* parent)
    : XdgShellPopup(new Private(this), parent)
{
}

XdgShellPopupStable::~XdgShellPopupStable() = default;

}
