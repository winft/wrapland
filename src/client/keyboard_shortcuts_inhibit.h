/****************************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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
#pragma once

#include <QObject>
// STD
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct zwp_keyboard_shortcuts_inhibit_manager_v1;
struct zwp_keyboard_shortcuts_inhibitor_v1;

namespace Wrapland::Client
{

class EventQueue;
class Surface;
class KeyboardShortcutsInhibitorV1;
class Seat;

/**
 * @short Wrapper for the zwp_keyboard_shortcuts_inhibit_manager_v1 interface.
 *
 * This class provides a convenient wrapper for the zwp_keyboard_shortcuts_inhibit_manager_v1
 *interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the KeyboardShortcutsInhibitManagerV1 interface:
 * @code
 * KeyboardShortcutsInhibitManagerV1 *c = registry->createKeyboardShortcutsInhibitManagerV1(name,
 *version);
 * @endcode
 *
 * This creates the KeyboardShortcutsInhibitManagerV1 and sets it up directly. As an alternative
 *this can also be done in a more low level way:
 * @code
 * KeyboardShortcutsInhibitManagerV1 *c = new KeyboardShortcutsInhibitManagerV1;
 * c->setup(registry->bindKeyboardShortcutsInhibitManagerV1(name, version));
 * @endcode
 *
 * The KeyboardShortcutsInhibitManagerV1 can be used as a drop-in replacement for any
 *zwp_keyboard_shortcuts_inhibit_manager_v1 pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.0.541
 **/
class WRAPLANDCLIENT_EXPORT KeyboardShortcutsInhibitManagerV1 : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new KeyboardShortcutsInhibitManagerV1.
     * Note: after constructing the KeyboardShortcutsInhibitManagerV1 it is not yet valid and one
     *needs to call setup. In order to get a ready to use KeyboardShortcutsInhibitManagerV1 prefer
     *using Registry::createKeyboardShortcutsInhibitManagerV1.
     **/
    explicit KeyboardShortcutsInhibitManagerV1(QObject* parent = nullptr);
    virtual ~KeyboardShortcutsInhibitManagerV1();

    /**
     * Setup this KeyboardShortcutsInhibitManagerV1 to manage the @p idleinhibitmanager.
     * When using Registry::createKeyboardShortcutsInhibitManagerV1 there is no need to call this
     * method.
     **/
    void setup(zwp_keyboard_shortcuts_inhibit_manager_v1* idleinhibitmanager);
    /**
     * @returns @c true if managing a zwp_keyboard_shortcuts_inhibit_manager_v1.
     **/
    bool isValid() const;
    /**
     * Releases the zwp_keyboard_shortcuts_inhibit_manager_v1 interface.
     * After the interface has been released the KeyboardShortcutsInhibitManagerV1 instance is no
     * longer valid and can be setup with another zwp_keyboard_shortcuts_inhibit_manager_v1
     *interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this KeyboardShortcutsInhibitManagerV1.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this
     *KeyboardShortcutsInhibitManagerV1.
     **/
    EventQueue* eventQueue();

    /**
     * Creates an KeyboardShortcutsInhibitorV1 for the given @p surface.
     * While the KeyboardShortcutsInhibitorV1 exists the @p surface is marked to inhibit idle.
     * @param surface The Surface which should have idle inhibited
     * @param seat The Seat which should have idle inhibited
     * @param parent The parent object for the KeyboardShortcutsInhibitorV1
     * @returns The created KeyboardShortcutsInhibitorV1
     **/
    KeyboardShortcutsInhibitorV1*
    inhibitShortcuts(Surface* surface, Seat* seat, QObject* parent = nullptr);

    operator zwp_keyboard_shortcuts_inhibit_manager_v1*();
    operator zwp_keyboard_shortcuts_inhibit_manager_v1*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the KeyboardShortcutsInhibitManagerV1 got created by
     * Registry::createKeyboardShortcutsInhibitManagerV1
     **/
    void removed();
    void inhibitorCreated();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * An KeyboardShortcutsInhibitorV1 prevents the Output that the associated Surface is visible on
 *from being set to a state where it is not visually usable due to lack of user interaction (e.g.
 *blanked, dimmed, locked, set to power save, etc.)  Any screensaver processes are also blocked from
 *displaying.
 *
 * If the Surface is destroyed, unmapped, becomes occluded, loses visibility, or otherwise
 * becomes not visually relevant for the user, the KeyboardShortcutsInhibitorV1 will not be honored
 *by the compositor; if the Surface subsequently regains visibility the inhibitor takes effect once
 *again. Likewise, the KeyboardShortcutsInhibitorV1 isn't honored if the system was already idled at
 *the time the KeyboardShortcutsInhibitorV1 was established, although if the system later de-idles
 *and re-idles the KeyboardShortcutsInhibitorV1 will take effect.
 *
 * @see KeyboardShortcutsInhibitManagerV1
 * @see Surface
 * @since 0.0.541
 **/
class WRAPLANDCLIENT_EXPORT KeyboardShortcutsInhibitorV1 : public QObject
{
    Q_OBJECT
public:
    virtual ~KeyboardShortcutsInhibitorV1();

    /**
     * Setup this KeyboardShortcutsInhibitorV1 to manage the @p idleinhibitor.
     * When using KeyboardShortcutsInhibitManagerV1::createKeyboardShortcutsInhibitorV1 there is no
     *need to call this method.
     **/
    void setup(zwp_keyboard_shortcuts_inhibitor_v1* idleinhibitor);
    /**
     * @returns @c true if managing a zwp_keyboard_shortcuts_inhibitor_v1.
     **/
    bool isValid() const;
    /**
     * Releases the zwp_keyboard_shortcuts_inhibitor_v1 interface.
     * After the interface has been released the KeyboardShortcutsInhibitorV1 instance is no
     * longer valid and can be setup with another zwp_keyboard_shortcuts_inhibitor_v1 interface.
     **/
    void release();
    bool isActive();

    operator zwp_keyboard_shortcuts_inhibitor_v1*();
    operator zwp_keyboard_shortcuts_inhibitor_v1*() const;

Q_SIGNALS:
    void inhibitorActive();
    void inhibitorInactive();

private:
    friend class KeyboardShortcutsInhibitManagerV1;
    explicit KeyboardShortcutsInhibitorV1(QObject* parent = nullptr);
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
