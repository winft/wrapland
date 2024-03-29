/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_SEAT_H
#define WAYLAND_SEAT_H

#include <QObject>
// STD
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct wl_seat;
struct wl_touch;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class Keyboard;
class Pointer;
class Touch;

/**
 * @short Wrapper for the wl_seat interface.
 *
 * This class provides a convenient wrapper for the wl_seat interface.
 * It's main purpose is to provide the interfaces for Keyboard, Pointer and Touch.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the Seat interface:
 * @code
 * Seat *s = registry->createSeat(name, version);
 * @endcode
 *
 * This creates the Seat and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * Seat *s = new Seat;
 * s->setup(registry->bindSeat(name, version));
 * @endcode
 *
 * The Seat can be used as a drop-in replacement for any wl_seat
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @see Keyboard
 * @see Pointer
 **/
class WRAPLANDCLIENT_EXPORT Seat : public QObject
{
    Q_OBJECT
public:
    explicit Seat(QObject* parent = nullptr);
    virtual ~Seat();

    /**
     * @returns @c true if managing a wl_seat.
     **/
    bool isValid() const;
    /**
     * Setup this Seat to manage the @p seat.
     * When using Registry::createSeat there is no need to call this
     * method.
     **/
    void setup(wl_seat* seat);
    /**
     * Releases the wl_seat interface.
     * After the interface has been released the Seat instance is no
     * longer valid and can be setup with another wl_seat interface.
     *
     * Right before the interface is released the signal interfaceAboutToBeReleased is emitted.
     * @see interfaceAboutToBeReleased
     **/
    void release();

    /**
     * Sets the @p queue to use for creating Keyboard, Pointer and Touch.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating Keyboard, Pointer and Touch.
     **/
    EventQueue* eventQueue();

    bool hasKeyboard() const;
    bool hasPointer() const;
    bool hasTouch() const;
    QString name() const;
    operator wl_seat*();
    operator wl_seat*() const;

    /**
     * Creates a Keyboard.
     *
     * This method may only be called if the Seat has a keyboard.
     *
     * @param parent The parent to pass to the created Keyboard.
     * @returns The created Keyboard.
     **/
    Keyboard* createKeyboard(QObject* parent = nullptr);
    /**
     * Creates a Pointer.
     *
     * This method may only be called if the Seat has a pointer.
     *
     * @param parent The parent to pass to the created Pointer.
     * @returns The created Pointer.
     **/
    Pointer* createPointer(QObject* parent = nullptr);
    /**
     * Creates a Touch.
     *
     * This method may only be called if the Seat has touch support.
     *
     * @param parent The parent to pass to the created Touch.
     * @returns The created Touch.
     **/
    Touch* createTouch(QObject* parent = nullptr);

Q_SIGNALS:
    void hasKeyboardChanged(bool);
    void hasPointerChanged(bool);
    void hasTouchChanged(bool);
    void nameChanged(QString const& name);

    /**
     * This signal is emitted right before the interface is going to be released.
     **/
    void interfaceAboutToBeReleased();

    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the Compositor got created by
     * Registry::createSeat
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
