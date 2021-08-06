/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct zwp_text_input_manager_v3;
struct zwp_text_input_v3;

namespace Wrapland::Client
{
class EventQueue;
class Seat;
class Surface;
class text_input_v3;

enum class text_input_v3_content_hint : uint32_t {
    none = 0,
    completion = 1 << 0,
    spellcheck = 1 << 1,
    auto_capitalization = 1 << 2,
    lowercase = 1 << 3,
    uppercase = 1 << 4,
    titlecase = 1 << 5,
    hidden_text = 1 << 6,
    sensitive_data = 1 << 7,
    latin = 1 << 8,
    multiline = 1 << 9,
};
Q_DECLARE_FLAGS(text_input_v3_content_hints, text_input_v3_content_hint)

enum class text_input_v3_content_purpose : uint32_t {
    normal,
    alpha,
    digits,
    number,
    phone,
    url,
    email,
    name,
    password,
    date,
    time,
    datetime,
    terminal,
};

enum class text_input_v3_change_cause {
    input_method,
    other,
};

/**
 * @short Wrapper for the zwp_text_input_manager_v3 interface.
 *
 * This class provides a convenient wrapper for the zwp_text_input_manager_v3 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the text_input_manager_v3 interface:
 * @code
 * auto c = registry->createTextInputManagerV3(name, version);
 * @endcode
 *
 * This creates the text_input_manager_v3 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * auto c = new text_input_manager_v3;
 * c->setup(registry->bindTextInputManagerV3(name, version));
 * @endcode
 *
 * The text_input_manager_v3 can be used as a drop-in replacement for any zwp_text_input_manager_v3
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT text_input_manager_v3 : public QObject
{
    Q_OBJECT
public:
    explicit text_input_manager_v3(QObject* parent = nullptr);
    ~text_input_manager_v3() override;

    /**
     * Setup this text_input_manager_v3 to manage the @p manager.
     * When using Registry::createTextInputManagerV3 there is no need to call this
     * method.
     **/
    void setup(zwp_text_input_manager_v3* manager);
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;
    /**
     * Releases the interface.
     * After the interface has been released the text_input_manager_v3 instance is no
     * longer valid and can be setup with another interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this text_input_manager_v3.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this text_input_manager_v3.
     **/
    EventQueue* event_queue();

    /**
     * Creates a TextInput for the @p seat.
     *
     * @param seat The Seat to create the TextInput for
     * @param parent The parent to use for the TextInput
     **/
    text_input_v3* get_text_input(Seat* seat, QObject* parent = nullptr);

    /**
     * @returns @c null if not for a zwp_text_input_manager_v3
     **/
    operator zwp_text_input_manager_v3*();
    /**
     * @returns @c null if not for a zwp_text_input_manager_v3
     **/
    operator zwp_text_input_manager_v3*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the text_input_manager_v3 got created by
     * Registry::createTextInputManagerV3
     **/
    void removed();

private:
    class Private;

    std::unique_ptr<Private> d_ptr;
};

struct text_input_v3_state {
    struct {
        bool update{false};
        std::string data;
        uint32_t cursor_begin{0};
        uint32_t cursor_end{0};
    } preedit_string;

    struct {
        bool update{false};
        std::string data;
    } commit_string;

    struct {
        bool update{false};
        uint32_t before_length{0};
        uint32_t after_length{0};
    } delete_surrounding_text;
};

/**
 * @short Wrapper for the zwp_text_input_v3 interface.
 *
 * This class is a convenient wrapper for the zwp_text_input_v3 interface.
 * To create a text_input_v3 call text_input_manager_v3::createTextInput.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see text_input_manager_v3
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT text_input_v3 : public QObject
{
    Q_OBJECT
public:
    ~text_input_v3() override;

    /**
     * Setup this text_input_v3 to manage the @p text_input.
     * When using TextInputManagerUnstableV2::createTextInput there is no need to call
     * this method.
     **/
    void setup(zwp_text_input_v3* text_input);
    /**
     * Releases the zwp_text_input_v3 interface.
     * After the interface has been released the text_input_v3 instance is no
     * longer valid and can be setup with another zwp_text_input_v3 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;

    operator zwp_text_input_v3*();
    operator zwp_text_input_v3*() const;

    void setEventQueue(EventQueue* queue);
    EventQueue* event_queue() const;

    void enable();
    void disable();

    Surface* entered_surface() const;
    text_input_v3_state const& state() const;

    void set_surrounding_text(std::string const& text,
                              uint32_t cursor,
                              uint32_t anchor,
                              text_input_v3_change_cause cause);
    void set_content_type(text_input_v3_content_hints hints, text_input_v3_content_purpose purpose);
    void set_cursor_rectangle(QRect const& rect);

    void commit();

Q_SIGNALS:
    void entered();
    void left();
    void done();

private:
    explicit text_input_v3(Seat* seat, QObject* parent = nullptr);
    friend class text_input_manager_v3;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Client::text_input_v3_content_hint)
Q_DECLARE_METATYPE(Wrapland::Client::text_input_v3_content_purpose)
Q_DECLARE_METATYPE(Wrapland::Client::text_input_v3_content_hints)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::text_input_v3_content_hints)
