/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>
#include <Wrapland/Client/wraplandclient_export.h>
#include <chrono>
#include <memory>

struct zwp_virtual_keyboard_manager_v1;
struct zwp_virtual_keyboard_v1;

namespace Wrapland::Client
{
class EventQueue;
class Seat;
class Surface;
class virtual_keyboard_v1;

enum class key_state {
    released,
    pressed,
};

/**
 * @short Wrapper for the zwp_virtual_keyboard_manager_v1 interface.
 *
 * This class provides a convenient wrapper for the zwp_virtual_keyboard_manager_v1 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the virtual_keyboard_manager_v1 interface:
 * @code
 * auto c = registry->createVirtualKeyboardManagerV1(name, version);
 * @endcode
 *
 * This creates the virtual_keyboard_manager_v1 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * auto c = new virtual_keyboard_manager_v1;
 * c->setup(registry->bindVirtualKeyboardManagerV1(name, version));
 * @endcode
 *
 * The virtual_keyboard_manager_v1 can be used as a drop-in replacement for any
 *zwp_virtual_keyboard_manager_v1 pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT virtual_keyboard_manager_v1 : public QObject
{
    Q_OBJECT
public:
    explicit virtual_keyboard_manager_v1(QObject* parent = nullptr);
    ~virtual_keyboard_manager_v1() override;

    /**
     * Setup this virtual_keyboard_manager_v1 to manage the @p manager.
     * When using Registry::createVirtualKeyboardManagerV1 there is no need to call this
     * method.
     **/
    void setup(zwp_virtual_keyboard_manager_v1* manager);
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;
    /**
     * Releases the interface.
     * After the interface has been released the virtual_keyboard_manager_v1 instance is no
     * longer valid and can be setup with another interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this virtual_keyboard_manager_v1.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this virtual_keyboard_manager_v1.
     **/
    EventQueue* event_queue();

    /**
     * Creates a virtual_keyboard for the @p seat.
     *
     * @param seat The Seat to create the virtual_keyboard for
     * @param parent The parent to use for the virtual_keyboard
     **/
    virtual_keyboard_v1* create_virtual_keyboard(Seat* seat, QObject* parent = nullptr);

    /**
     * @returns @c null if not for a zwp_virtual_keyboard_manager_v1
     **/
    operator zwp_virtual_keyboard_manager_v1*();
    /**
     * @returns @c null if not for a zwp_virtual_keyboard_manager_v1
     **/
    operator zwp_virtual_keyboard_manager_v1*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the virtual_keyboard_manager_v1 got created by
     * Registry::createVirtualKeyboardManagerV1
     **/
    void removed();

private:
    class Private;

    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the zwp_virtual_keyboard_v1 interface.
 *
 * This class is a convenient wrapper for the zwp_virtual_keyboard_v1 interface.
 * To create a virtual_keyboard_v1 call virtual_keyboard_manager_v1::create_virtual_keyboard.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see virtual_keyboard_manager_v1
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT virtual_keyboard_v1 : public QObject
{
    Q_OBJECT
public:
    ~virtual_keyboard_v1() override;

    /**
     * Setup this virtual_keyboard_v1 to manage the @p virtual_keyboard.
     * When using VirtualKeyboardManagerV1::create_virtual_keyboard there is no need to call
     * this method.
     **/
    void setup(zwp_virtual_keyboard_v1* virtual_keyboard);
    /**
     * Releases the zwp_virtual_keyboard_v1 interface.
     * After the interface has been released the virtual_keyboard_v1 instance is no
     * longer valid and can be setup with another zwp_virtual_keyboard_v1 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;

    operator zwp_virtual_keyboard_v1*();
    operator zwp_virtual_keyboard_v1*() const;

    void setEventQueue(EventQueue* queue);
    EventQueue* event_queue() const;

    void keymap(std::string const& content);
    void key(std::chrono::milliseconds time, uint32_t key, key_state state);
    void modifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

private:
    explicit virtual_keyboard_v1(Seat* seat, QObject* parent = nullptr);
    friend class virtual_keyboard_manager_v1;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
