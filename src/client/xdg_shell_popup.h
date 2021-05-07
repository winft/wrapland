/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>
#include <QRect>
#include <QSize>

#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct xdg_surface;
struct xdg_popup;

namespace Wrapland::Client
{

class EventQueue;
class Seat;

/**
 * A XdgShellPopup is a short-lived, temporary surface that can be
 * used to implement menus. It takes an explicit grab on the surface
 * that will be dismissed when the user dismisses the popup. This can
 * be done by the user clicking outside the surface, using the keyboard,
 * or even locking the screen through closing the lid or a timeout.
 * @since 0.0.525
 **/
class WRAPLANDCLIENT_EXPORT XdgShellPopup : public QObject
{
    Q_OBJECT
public:
    virtual ~XdgShellPopup();

    /**
     * Setup this XdgShellPopup to manage the @p xdgpopupv on associated @p xdgsurface
     * When using XdgShell::createXdgShellPopup there is no need to call this
     * method.
     * @since 0.0.5XDGSTABLE
     **/
    void setup(xdg_surface* xdgsurface, xdg_popup* xdgpopup);

    /**
     * @returns @c true if managing an xdg_popup.
     **/
    bool isValid() const;
    /**
     * Releases the xdg_popup interface.
     * After the interface has been released the XdgShellPopup instance is no
     * longer valid and can be setup with another xdg_popup interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for bound proxies.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for bound proxies.
     **/
    EventQueue* eventQueue();

    /**
     * Requests a grab on this popup
     * @since 0.0.539
     */
    void requestGrab(Seat* seat, quint32 serial);

    /**
     * When a configure event is received, if a client commits the
     * Surface in response to the configure event, then the client
     * must make an ackConfigure request sometime before the commit
     * request, passing along the @p serial of the configure event.
     * @see configureRequested
     * @since 0.0.556
     **/
    void ackConfigure(quint32 serial);

    /**
     * Sets the position of the window contents within the buffer
     * @since 0.0.559
     */
    void setWindowGeometry(const QRect& windowGeometry);

    operator xdg_surface*();
    operator xdg_surface*() const;
    operator xdg_popup*();
    operator xdg_popup*() const;

Q_SIGNALS:
    /**
     * This signal is emitted when a XdgShellPopup is dismissed by the
     * compositor. The user should delete this instance at this point.
     **/
    void popupDone();

    /**
     * Emitted when the server has configured the popup with the final location of @p
     * relativePosition This is emitted for V6 surfaces only
     * @since 0.0.539
     **/
    void configureRequested(const QRect& relativePosition, quint32 serial);

private:
    explicit XdgShellPopup(QObject* parent = nullptr);
    friend class XdgShell;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
