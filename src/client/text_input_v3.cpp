/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "text_input_v3_p.h"

#include "event_queue.h"
#include "seat.h"
#include "surface.h"

namespace Wrapland::Client
{

uint32_t convert_change_cause(text_input_v3_change_cause cause)
{
    switch (cause) {
    case text_input_v3_change_cause::input_method:
        return ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
    case text_input_v3_change_cause::other:
    default:
        return ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER;
    }
}

uint32_t convert_content_hints(text_input_v3_content_hints hints)
{
    uint32_t ret = 0;

    if (hints & text_input_v3_content_hint::completion) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION;
    }
    if (hints & text_input_v3_content_hint::spellcheck) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK;
    }
    if (hints & text_input_v3_content_hint::auto_capitalization) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION;
    }
    if (hints & text_input_v3_content_hint::lowercase) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_LOWERCASE;
    }
    if (hints & text_input_v3_content_hint::uppercase) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_UPPERCASE;
    }
    if (hints & text_input_v3_content_hint::titlecase) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_TITLECASE;
    }
    if (hints & text_input_v3_content_hint::hidden_text) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT;
    }
    if (hints & text_input_v3_content_hint::sensitive_data) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA;
    }
    if (hints & text_input_v3_content_hint::latin) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_LATIN;
    }
    if (hints & text_input_v3_content_hint::multiline) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_MULTILINE;
    }
    return ret;
}

uint32_t convert_content_purpose(text_input_v3_content_purpose purpose)
{
    switch (purpose) {
    case text_input_v3_content_purpose::alpha:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_ALPHA;
    case text_input_v3_content_purpose::digits:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DIGITS;
    case text_input_v3_content_purpose::number:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER;
    case text_input_v3_content_purpose::phone:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE;
    case text_input_v3_content_purpose::url:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL;
    case text_input_v3_content_purpose::email:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL;
    case text_input_v3_content_purpose::name:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME;
    case text_input_v3_content_purpose::password:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD;
    case text_input_v3_content_purpose::date:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATE;
    case text_input_v3_content_purpose::time:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TIME;
    case text_input_v3_content_purpose::datetime:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATETIME;
    case text_input_v3_content_purpose::terminal:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL;
    case text_input_v3_content_purpose::normal:
    default:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    }
}

text_input_v3_content_hints convert_content_hints(uint32_t hints)
{
    auto ret = text_input_v3_content_hints();

    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION) {
        ret |= text_input_v3_content_hint::completion;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK) {
        ret |= text_input_v3_content_hint::spellcheck;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION) {
        ret |= text_input_v3_content_hint::auto_capitalization;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_LOWERCASE) {
        ret |= text_input_v3_content_hint::lowercase;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_UPPERCASE) {
        ret |= text_input_v3_content_hint::uppercase;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_TITLECASE) {
        ret |= text_input_v3_content_hint::titlecase;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT) {
        ret |= text_input_v3_content_hint::hidden_text;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA) {
        ret |= text_input_v3_content_hint::sensitive_data;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_LATIN) {
        ret |= text_input_v3_content_hint::latin;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_MULTILINE) {
        ret |= text_input_v3_content_hint::multiline;
    }
    return ret;
}

text_input_v3_content_purpose convert_content_purpose(uint32_t purpose)
{
    switch (purpose) {
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_ALPHA:
        return text_input_v3_content_purpose::alpha;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DIGITS:
        return text_input_v3_content_purpose::digits;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER:
        return text_input_v3_content_purpose::number;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE:
        return text_input_v3_content_purpose::phone;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL:
        return text_input_v3_content_purpose::url;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL:
        return text_input_v3_content_purpose::email;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME:
        return text_input_v3_content_purpose::name;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD:
        return text_input_v3_content_purpose::password;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATE:
        return text_input_v3_content_purpose::date;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TIME:
        return text_input_v3_content_purpose::time;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATETIME:
        return text_input_v3_content_purpose::datetime;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL:
        return text_input_v3_content_purpose::terminal;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL:
    default:
        return text_input_v3_content_purpose::normal;
    }
}

text_input_v3_change_cause convert_change_cause(uint32_t cause)
{
    switch (cause) {
    case ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD:
        return text_input_v3_change_cause::input_method;
    case ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER:
    default:
        return text_input_v3_change_cause::other;
    }
}

text_input_manager_v3::text_input_manager_v3(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private)
{
}

text_input_manager_v3::~text_input_manager_v3() = default;

void text_input_manager_v3::setup(zwp_text_input_manager_v3* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!d_ptr->manager_ptr);
    d_ptr->manager_ptr.setup(manager);
}

void text_input_manager_v3::release()
{
    d_ptr->release();
}

void text_input_manager_v3::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* text_input_manager_v3::event_queue()
{
    return d_ptr->queue;
}

text_input_manager_v3::operator zwp_text_input_manager_v3*()
{
    return *(d_ptr.get());
}

text_input_manager_v3::operator zwp_text_input_manager_v3*() const
{
    return *(d_ptr.get());
}

bool text_input_manager_v3::isValid() const
{
    return d_ptr->isValid();
}

text_input_v3* text_input_manager_v3::get_text_input(Seat* seat, QObject* parent)
{
    return d_ptr->get_text_input(seat, parent);
}

void text_input_manager_v3::Private::release()
{
    manager_ptr.release();
}

bool text_input_manager_v3::Private::isValid()
{
    return manager_ptr.isValid();
}

text_input_v3* text_input_manager_v3::Private::get_text_input(Seat* seat, QObject* parent)
{
    Q_ASSERT(isValid());
    auto ti = new text_input_v3(seat, parent);
    auto wlti = zwp_text_input_manager_v3_get_text_input(manager_ptr, *seat);
    if (queue) {
        queue->addProxy(wlti);
    }
    ti->setup(wlti);
    return ti;
}

text_input_v3::Private::Private(Seat* seat, text_input_v3* q)
    : seat{seat}
    , q_ptr{q}
{
}

const zwp_text_input_v3_listener text_input_v3::Private::s_listener = {
    enter_callback,
    leave_callback,
    preedit_string_callback,
    commit_string_callback,
    delete_surrounding_text_callback,
    done_callback,
};

void text_input_v3::Private::enter_callback(void* data,
                                            zwp_text_input_v3* zwp_text_input_v3,
                                            wl_surface* surface)
{
    auto priv = reinterpret_cast<text_input_v3::Private*>(data);
    Q_ASSERT(priv->text_input_ptr == zwp_text_input_v3);
    priv->entered_surface = Surface::get(surface);
    connect(priv->entered_surface, &Surface::destroyed, priv->q_ptr, [priv] {
        priv->entered_surface = nullptr;
    });
    Q_EMIT priv->q_ptr->entered();
}

void text_input_v3::Private::leave_callback(void* data,
                                            zwp_text_input_v3* zwp_text_input_v3,
                                            wl_surface* surface)
{
    Q_UNUSED(surface)
    auto priv = reinterpret_cast<text_input_v3::Private*>(data);
    Q_ASSERT(priv->text_input_ptr == zwp_text_input_v3);
    disconnect(priv->entered_surface, nullptr, priv->q_ptr, nullptr);
    priv->entered_surface = nullptr;
    Q_EMIT priv->q_ptr->left();
}

void text_input_v3::Private::preedit_string_callback(void* data,
                                                     zwp_text_input_v3* zwp_text_input_v3,
                                                     char const* text,
                                                     int32_t cursor_begin,
                                                     int32_t cursor_end)
{
    auto priv = reinterpret_cast<text_input_v3::Private*>(data);
    assert(priv->text_input_ptr == zwp_text_input_v3);
    priv->pending.preedit_string.update = true;
    priv->pending.preedit_string.data = text;
    priv->pending.preedit_string.cursor_begin = cursor_begin;
    priv->pending.preedit_string.cursor_end = cursor_end;
}

void text_input_v3::Private::commit_string_callback(void* data,
                                                    zwp_text_input_v3* zwp_text_input_v3,
                                                    char const* text)
{
    auto priv = reinterpret_cast<text_input_v3::Private*>(data);
    assert(priv->text_input_ptr == zwp_text_input_v3);
    priv->pending.commit_string.update = true;
    priv->pending.commit_string.data = text;
}

void text_input_v3::Private::delete_surrounding_text_callback(void* data,
                                                              zwp_text_input_v3* zwp_text_input_v3,
                                                              uint32_t before_length,
                                                              uint32_t after_length)
{
    auto priv = reinterpret_cast<text_input_v3::Private*>(data);
    assert(priv->text_input_ptr == zwp_text_input_v3);
    priv->pending.delete_surrounding_text.update = true;
    priv->pending.delete_surrounding_text.before_length = before_length;
    priv->pending.delete_surrounding_text.after_length = after_length;
}

void text_input_v3::Private::done_callback(void* data,
                                           zwp_text_input_v3* zwp_text_input_v3,
                                           uint32_t serial)
{
    auto priv = reinterpret_cast<text_input_v3::Private*>(data);
    Q_ASSERT(priv->text_input_ptr == zwp_text_input_v3);
    if (priv->serial == serial) {
        priv->current = priv->pending;
        Q_EMIT priv->q_ptr->done();
    }
}

void text_input_v3::Private::setup(zwp_text_input_v3* text_input)
{
    Q_ASSERT(text_input);
    Q_ASSERT(!text_input_ptr);
    text_input_ptr.setup(text_input);
    zwp_text_input_v3_add_listener(text_input, &s_listener, this);
}

bool text_input_v3::Private::isValid() const
{
    return text_input_ptr.isValid();
}

void text_input_v3::Private::enable()
{
    zwp_text_input_v3_enable(text_input_ptr);
}

void text_input_v3::Private::disable()
{
    zwp_text_input_v3_disable(text_input_ptr);
}

text_input_v3::text_input_v3(Seat* seat, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(seat, this))
{
}

text_input_v3::~text_input_v3()
{
    release();
}

void text_input_v3::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* text_input_v3::event_queue() const
{
    return d_ptr->queue;
}

bool text_input_v3::isValid() const
{
    return d_ptr->isValid();
}

Surface* text_input_v3::entered_surface() const
{
    return d_ptr->entered_surface;
}

text_input_v3_state const& text_input_v3::state() const
{
    return d_ptr->current;
}

void text_input_v3::enable()
{
    d_ptr->enable();
}

void text_input_v3::disable()
{
    d_ptr->disable();
}

void text_input_v3::set_surrounding_text(std::string const& text,
                                         uint32_t cursor,
                                         uint32_t anchor,
                                         text_input_v3_change_cause cause)
{
    zwp_text_input_v3_set_surrounding_text(d_ptr->text_input_ptr, text.c_str(), cursor, anchor);
    zwp_text_input_v3_set_text_change_cause(d_ptr->text_input_ptr, convert_change_cause(cause));
}

void text_input_v3::set_content_type(text_input_v3_content_hints hints,
                                     text_input_v3_content_purpose purpose)
{
    auto wlHints = convert_content_hints(hints);
    auto wlPurpose = convert_content_purpose(purpose);
    zwp_text_input_v3_set_content_type(d_ptr->text_input_ptr, wlHints, wlPurpose);
}

void text_input_v3::set_cursor_rectangle(QRect const& rect)
{
    zwp_text_input_v3_set_cursor_rectangle(
        d_ptr->text_input_ptr, rect.x(), rect.y(), rect.width(), rect.height());
}

void text_input_v3::commit()
{
    d_ptr->serial++;
    zwp_text_input_v3_commit(d_ptr->text_input_ptr);
}

void text_input_v3::setup(zwp_text_input_v3* text_input)
{
    d_ptr->setup(text_input);
}

void text_input_v3::release()
{
    d_ptr->text_input_ptr.release();
}

text_input_v3::operator zwp_text_input_v3*()
{
    return d_ptr->text_input_ptr;
}

text_input_v3::operator zwp_text_input_v3*() const
{
    return d_ptr->text_input_ptr;
}

}
