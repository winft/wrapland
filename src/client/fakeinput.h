/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
*********************************************************************/
#ifndef WRAPLAND_FAKEINPUT_H
#define WRAPLAND_FAKEINPUT_H

#include <QObject>
//STD
#include <memory>
#include <Wrapland/Client/wraplandclient_export.h>

struct org_kde_kwin_fake_input;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class FakeInputTimeout;
class Seat;

/**
 * @short Wrapper for the org_kde_kwin_fake_input interface.
 *
 * FakeInput allows to fake input events into the Wayland server. This is a privileged
 * Wayland interface and the Wayland server is allowed to ignore all events.
 *
 * This class provides a convenient wrapper for the org_kde_kwin_fake_input interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the FakeInput interface:
 * @code
 * FakeInput *m = registry->createFakeInput(name, version);
 * @endcode
 *
 * This creates the FakeInput and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * FakeInput *m = new FakeInput;
 * m->setup(registry->bindFakeInput(name, version));
 * @endcode
 *
 * The FakeInput can be used as a drop-in replacement for any org_kde_kwin_fake_input
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT FakeInput : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new FakeInput.
     * Note: after constructing the FakeInput it is not yet valid and one needs
     * to call setup. In order to get a ready to use FakeInput prefer using
     * Registry::createFakeInput.
     **/
    explicit FakeInput(QObject *parent = nullptr);
    virtual ~FakeInput();

    /**
     * @returns @c true if managing a org_kde_kwin_fake_input.
     **/
    bool isValid() const;
    /**
     * Setup this FakeInput to manage the @p manager.
     * When using Registry::createFakeInput there is no need to call this
     * method.
     **/
    void setup(org_kde_kwin_fake_input *manager);
    /**
     * Releases the org_kde_kwin_fake_input interface.
     * After the interface has been released the FakeInput instance is no
     * longer valid and can be setup with another org_kde_kwin_fake_input interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for bound proxies.
     **/
    void setEventQueue(EventQueue *queue);
    /**
     * @returns The event queue to use for bound proxies.
     **/
    EventQueue *eventQueue();

    /**
     * Authenticate with the Wayland server in order to request sending fake input events.
     * The Wayland server might ignore all requests without a prior authentication.
     *
     * The Wayland server might use the provided @p applicationName and @p reason to ask
     * the user whether this request should get authenticated.
     *
     * There is no way for the client to figure out whether the authentication was granted
     * or denied. The client should assume that it wasn't granted.
     *
     * @param applicationName A human readable description of the application
     * @param reason A human readable explanation why this application wants to send fake requests
     **/
    void authenticate(const QString &applicationName, const QString &reason);
    /**
     * Request a relative pointer motion of @p delta pixels.
     **/
    void requestPointerMove(const QSizeF &delta);
    /**
     * Request an absolute pointer motion to @p pos position.
     *
     * @since 0.0.554
     **/
    void requestPointerMoveAbsolute(const QPointF &pos);
    /**
     * Convenience overload.
     * @see requestPointerButtonPress(quint32)
     **/
    void requestPointerButtonPress(Qt::MouseButton button);
    /**
     * Request a pointer button press.
     * @param linuxButton The button code as defined in linux/input-event-codes.h
     **/
    void requestPointerButtonPress(quint32 linuxButton);
    /**
     * Convenience overload.
     * @see requestPointerButtonRelease(quint32)
     **/
    void requestPointerButtonRelease(Qt::MouseButton button);
    /**
     * Request a pointer button release.
     * @param linuxButton The button code as defined in linux/input-event-codes.h
     **/
    void requestPointerButtonRelease(quint32 linuxButton);
    /**
     * Convenience overload.
     * @see requestPointerButtonClick(quint32)
     **/
    void requestPointerButtonClick(Qt::MouseButton button);
    /**
     * Requests a pointer button click, that is a press directly followed by a release.
     * @param linuxButton The button code as defined in linux/input-event-codes.h
     **/
    void requestPointerButtonClick(quint32 linuxButton);
    /**
     * Request a scroll of the pointer @p axis with @p delta.
     **/
    void requestPointerAxis(Qt::Orientation axis, qreal delta);
    /**
     * Request a touch down at @p pos in global coordinates.
     *
     * If this is the first touch down it starts a touch sequence.
     * @param id The id to identify the touch point
     * @param pos The global position of the touch point
     * @see requestTouchMotion
     * @see requestTouchUp
     * @since 0.0.523
     **/
    void requestTouchDown(quint32 id, const QPointF &pos);
    /**
     * Request a move of the touch point identified by @p id to new global @p pos.
     * @param id The id to identify the touch point
     * @param pos The global position of the touch point
     * @see requestTouchDown
     * @since 0.0.523
     **/
    void requestTouchMotion(quint32 id, const QPointF &pos);
    /**
     * Requests a touch up of the touch point identified by @p id.
     * @param id The id to identify the touch point
     * @since 0.0.523
     **/
    void requestTouchUp(quint32 id);
    /**
     * Requests to cancel the current touch event sequence.
     * @since 0.0.523
     **/
    void requestTouchCancel();
    /**
     * Requests a touch frame. This allows to manipulate multiple touch points in one
     * event and notify that the set of touch events for the current frame are finished.
     * @since 0.0.523
     **/
    void requestTouchFrame();

    /**
     * Request a keyboard key press.
     * @param linuxButton The button code as defined in linux/input-event-codes.h
     *
     * @since 0.0.563
     **/
    void requestKeyboardKeyPress(quint32 linuxKey);
    /**
     * Request a keyboard button release.
     * @param linuxButton The button code as defined in linux/input-event-codes.h
     *
     * @since 0.0.563
     **/
    void requestKeyboardKeyRelease(quint32 linuxKey);

    operator org_kde_kwin_fake_input*();
    operator org_kde_kwin_fake_input*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the Compositor got created by
     * Registry::createFakeInput
     *
     * @since 5.5
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
