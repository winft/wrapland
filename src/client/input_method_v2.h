/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "keyboard.h"
#include "text_input_v3.h"

#include <QObject>
#include <QRect>

#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct zwp_input_method_manager_v2;
struct zwp_input_method_v2;
struct zwp_input_popup_surface_v2;
struct zwp_input_method_keyboard_grab_v2;

namespace Wrapland::Client
{
class EventQueue;
class Seat;
class Surface;
class input_method_v2;
class input_method_keyboard_grab_v2;
class input_popup_surface_v2;

/**
 * @short Wrapper for the zwp_input_method_manager_v2 interface.
 *
 * This class provides a convenient wrapper for the zwp_input_method_manager_v2 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the input_method_manager_v2 interface:
 * @code
 * auto c = registry->createInputMethodManagerV2(name, version);
 * @endcode
 *
 * This creates the input_method_manager_v2 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * auto c = new input_method_manager_v2;
 * c->setup(registry->bindInputMethodManagerV2(name, version));
 * @endcode
 *
 * The input_method_manager_v2 can be used as a drop-in replacement for any
 *zwp_input_method_manager_v2 pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT input_method_manager_v2 : public QObject
{
    Q_OBJECT
public:
    explicit input_method_manager_v2(QObject* parent = nullptr);
    ~input_method_manager_v2() override;

    /**
     * Setup this input_method_manager_v2 to manage the @p manager.
     * When using Registry::createInputMethodManagerV2 there is no need to call this
     * method.
     **/
    void setup(zwp_input_method_manager_v2* manager);
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;
    /**
     * Releases the interface.
     * After the interface has been released the input_method_manager_v2 instance is no
     * longer valid and can be setup with another interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this input_method_manager_v2.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this input_method_manager_v2.
     **/
    EventQueue* event_queue();

    /**
     * Creates a input_method_v2 for the @p seat.
     *
     * @param seat The Seat to create the input_method_v2 for
     * @param parent The parent to use for the input_method_v2
     **/
    input_method_v2* get_input_method(Seat* seat, QObject* parent = nullptr);

    /**
     * @returns @c null if not for a zwp_input_method_manager_v2
     **/
    operator zwp_input_method_manager_v2*();
    /**
     * @returns @c null if not for a zwp_input_method_manager_v2
     **/
    operator zwp_input_method_manager_v2*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the input_method_manager_v2 got created by
     * Registry::createInputMethodManagerV2
     **/
    void removed();

private:
    class Private;

    std::unique_ptr<Private> d_ptr;
};

struct input_method_v2_state {
    bool active{false};
    struct {
        text_input_v3_content_hints hints{text_input_v3_content_hint::none};
        text_input_v3_content_purpose purpose{text_input_v3_content_purpose::normal};
    } content;

    struct {
        bool update{false};
        std::string data;
        int32_t cursor_position{0};
        int32_t selection_anchor{0};
        text_input_v3_change_cause change_cause{text_input_v3_change_cause::other};
    } surrounding_text;
};

/**
 * @short Wrapper for the zwp_input_method_v2 interface.
 *
 * This class is a convenient wrapper for the zwp_input_method_v2 interface.
 * To create a input_method_v2 call input_method_manager_v2::get_input_method.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see input_method_manager_v2
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT input_method_v2 : public QObject
{
    Q_OBJECT
public:
    ~input_method_v2() override;

    /**
     * Setup this input_method_v2 to manage the @p input_method.
     * When using input_method_manager_v2::get_input_method there is no need to call
     * this method.
     **/
    void setup(zwp_input_method_v2* input_method);
    /**
     * Releases the zwp_input_method_v2 interface.
     * After the interface has been released the input_method_v2 instance is no
     * longer valid and can be setup with another zwp_input_method_v2 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;

    operator zwp_input_method_v2*();
    operator zwp_input_method_v2*() const;

    void setEventQueue(EventQueue* queue);
    EventQueue* event_queue() const;

    input_method_v2_state const& state() const;

    void commit_string(std::string const& text);
    void set_preedit_string(std::string const& text, int32_t cursor_begin, int32_t cursor_end);
    void delete_surrounding_text(uint32_t before_length, uint32_t after_length);
    void commit();

    input_popup_surface_v2* get_input_popup_surface(Surface* surface, QObject* parent = nullptr);
    input_method_keyboard_grab_v2* grab_keyboard(QObject* parent = nullptr);

Q_SIGNALS:
    void done();
    void unavailable();

private:
    explicit input_method_v2(Seat* seat, QObject* parent = nullptr);
    friend class input_method_manager_v2;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the zwp_input_popup_surface_v2 interface.
 *
 * This class is a convenient wrapper for the zwp_input_popup_surface_v2 interface.
 * To create a input_popup_surface_v2 call input_method_v2::get_input_popup_surface.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see input_method_v2
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT input_popup_surface_v2 : public QObject
{
    Q_OBJECT
public:
    ~input_popup_surface_v2() override;

    /**
     * Setup this input_popup_surface_v2 to manage the @p input_popup_surface.
     * When using input_method_v2::get_input_popup_surface there is no need to call
     * this method.
     **/
    void setup(zwp_input_popup_surface_v2* input_popup_surface);
    /**
     * Releases the zwp_input_popup_surface_v2 interface.
     * After the interface has been released the input_popup_surface_v2 instance is no
     * longer valid and can be setup with another zwp_input_popup_surface_v2 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;

    operator zwp_input_popup_surface_v2*();
    operator zwp_input_popup_surface_v2*() const;

    void setEventQueue(EventQueue* queue);
    EventQueue* event_queue() const;

    QRect text_input_rectangle() const;

Q_SIGNALS:
    void text_input_rectangle_changed();

private:
    explicit input_popup_surface_v2(QObject* parent = nullptr);
    friend class input_method_v2;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the zwp_input_method_keyboard_grab_v2 interface.
 *
 * This class is a convenient wrapper for the zwp_input_method_keyboard_grab_v2 interface.
 * To create a input_method_keyboard_grab_v2 call input_method_v2::grab_keyboard.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see input_method_v2
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT input_method_keyboard_grab_v2 : public QObject
{
    Q_OBJECT
public:
    ~input_method_keyboard_grab_v2() override;

    /**
     * Setup this input_method_keyboard_grab_v2 to manage the @p keyboard_grab.
     * When using input_method_v2::grab_keyboard there is no need to call
     * this method.
     **/
    void setup(zwp_input_method_keyboard_grab_v2* keyboard_grab);
    /**
     * Releases the zwp_input_method_keyboard_grab_v2 interface.
     * After the interface has been released the input_method_keyboard_grab_v2 instance is no
     * longer valid and can be setup with another zwp_input_method_keyboard_grab_v2 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;

    operator zwp_input_method_keyboard_grab_v2*();
    operator zwp_input_method_keyboard_grab_v2*() const;

    void setEventQueue(EventQueue* queue);
    EventQueue* event_queue() const;

    int32_t repeat_rate() const;
    int32_t repeat_delay() const;

Q_SIGNALS:
    void keymap_changed(int fd, quint32 size);
    void key_changed(quint32 key, Wrapland::Client::Keyboard::KeyState state, quint32 time);
    void modifiers_changed(quint32 depressed, quint32 latched, quint32 locked, quint32 group);
    void repeat_changed();

private:
    explicit input_method_keyboard_grab_v2(QObject* parent = nullptr);
    friend class input_method_v2;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
