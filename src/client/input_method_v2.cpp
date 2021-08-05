/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "input_method_v2_p.h"

#include "text_input_v3_p.h"

#include "event_queue.h"
#include "seat.h"
#include "surface.h"

namespace Wrapland::Client
{

input_method_manager_v2::input_method_manager_v2(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private)
{
}

input_method_manager_v2::~input_method_manager_v2() = default;

void input_method_manager_v2::setup(zwp_input_method_manager_v2* manager)
{
    assert(manager);
    assert(!d_ptr->manager_ptr);
    d_ptr->manager_ptr.setup(manager);
}

void input_method_manager_v2::release()
{
    d_ptr->release();
}

void input_method_manager_v2::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* input_method_manager_v2::event_queue()
{
    return d_ptr->queue;
}

input_method_manager_v2::operator zwp_input_method_manager_v2*()
{
    return *(d_ptr.get());
}

input_method_manager_v2::operator zwp_input_method_manager_v2*() const
{
    return *(d_ptr.get());
}

bool input_method_manager_v2::isValid() const
{
    return d_ptr->isValid();
}

input_method_v2* input_method_manager_v2::get_input_method(Seat* seat, QObject* parent)
{
    return d_ptr->get_input_method(seat, parent);
}

void input_method_manager_v2::Private::release()
{
    manager_ptr.release();
}

bool input_method_manager_v2::Private::isValid()
{
    return manager_ptr.isValid();
}

input_method_v2* input_method_manager_v2::Private::get_input_method(Seat* seat, QObject* parent)
{
    assert(isValid());
    auto im = new input_method_v2(seat, parent);
    auto wlim = zwp_input_method_manager_v2_get_input_method(manager_ptr, *seat);
    if (queue) {
        queue->addProxy(wlim);
    }
    im->setup(wlim);
    return im;
}

input_method_v2::Private::Private(Seat* seat, input_method_v2* q)
    : seat{seat}
    , q_ptr{q}
{
}

zwp_input_method_v2_listener const input_method_v2::Private::s_listener = {
    activate_callback,
    deactivate_callback,
    surrounding_text_callback,
    text_change_cause_callback,
    content_type_callback,
    done_callback,
    unavailable_callback,
};

void input_method_v2::Private::activate_callback(void* data,
                                                 zwp_input_method_v2* zwp_input_method_v2)
{
    auto priv = reinterpret_cast<input_method_v2::Private*>(data);
    assert(priv->input_method_ptr == zwp_input_method_v2);
    priv->pending = {};
    priv->pending.active = true;
}

void input_method_v2::Private::deactivate_callback(void* data,
                                                   zwp_input_method_v2* zwp_input_method_v2)
{
    auto priv = reinterpret_cast<input_method_v2::Private*>(data);
    assert(priv->input_method_ptr == zwp_input_method_v2);
    priv->pending.active = false;
}

void input_method_v2::Private::surrounding_text_callback(void* data,
                                                         zwp_input_method_v2* zwp_input_method_v2,
                                                         char const* text,
                                                         uint32_t cursor,
                                                         uint32_t anchor)
{
    auto priv = reinterpret_cast<input_method_v2::Private*>(data);
    assert(priv->input_method_ptr == zwp_input_method_v2);
    priv->pending.surrounding_text.update = true;
    priv->pending.surrounding_text.data = text;
    priv->pending.surrounding_text.cursor_position = cursor;
    priv->pending.surrounding_text.selection_anchor = anchor;
}

void input_method_v2::Private::text_change_cause_callback(void* data,
                                                          zwp_input_method_v2* zwp_input_method_v2,
                                                          uint32_t cause)
{
    auto priv = reinterpret_cast<input_method_v2::Private*>(data);
    assert(priv->input_method_ptr == zwp_input_method_v2);
    priv->pending.surrounding_text.change_cause = convert_change_cause(cause);
}

void input_method_v2::Private::content_type_callback(void* data,
                                                     zwp_input_method_v2* zwp_input_method_v2,
                                                     uint32_t hint,
                                                     uint32_t purpose)
{
    auto priv = reinterpret_cast<input_method_v2::Private*>(data);
    assert(priv->input_method_ptr == zwp_input_method_v2);
    priv->pending.content.hints = convert_content_hints(hint);
    priv->pending.content.purpose = convert_content_purpose(purpose);
}

void input_method_v2::Private::done_callback(void* data, zwp_input_method_v2* zwp_input_method_v2)
{
    auto priv = reinterpret_cast<input_method_v2::Private*>(data);
    assert(priv->input_method_ptr == zwp_input_method_v2);
    priv->serial++;
    priv->current = priv->pending;
    Q_EMIT priv->q_ptr->done();
}

void input_method_v2::Private::unavailable_callback(void* data,
                                                    zwp_input_method_v2* zwp_input_method_v2)
{
    auto priv = reinterpret_cast<input_method_v2::Private*>(data);
    assert(priv->input_method_ptr == zwp_input_method_v2);
    Q_EMIT priv->q_ptr->unavailable();
}

void input_method_v2::Private::setup(zwp_input_method_v2* input_method)
{
    assert(input_method);
    assert(!input_method_ptr);
    input_method_ptr.setup(input_method);
    zwp_input_method_v2_add_listener(input_method, &s_listener, this);
}

bool input_method_v2::Private::isValid() const
{
    return input_method_ptr.isValid();
}

input_method_v2::input_method_v2(Seat* seat, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(seat, this))
{
}

input_method_v2::~input_method_v2()
{
    release();
}

void input_method_v2::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* input_method_v2::event_queue() const
{
    return d_ptr->queue;
}

bool input_method_v2::isValid() const
{
    return d_ptr->isValid();
}

input_method_v2_state const& input_method_v2::state() const
{
    return d_ptr->current;
}

void input_method_v2::setup(zwp_input_method_v2* input_method)
{
    d_ptr->setup(input_method);
}

void input_method_v2::release()
{
    d_ptr->input_method_ptr.release();
}

input_method_v2::operator zwp_input_method_v2*()
{
    return d_ptr->input_method_ptr;
}

input_method_v2::operator zwp_input_method_v2*() const
{
    return d_ptr->input_method_ptr;
}

void input_method_v2::commit_string(std::string const& text)
{
    zwp_input_method_v2_commit_string(d_ptr->input_method_ptr, text.c_str());
}

void input_method_v2::set_preedit_string(std::string const& text,
                                         int32_t cursor_begin,
                                         int32_t cursor_end)
{
    zwp_input_method_v2_set_preedit_string(
        d_ptr->input_method_ptr, text.c_str(), cursor_begin, cursor_end);
}

void input_method_v2::delete_surrounding_text(uint32_t before_length, uint32_t after_length)
{
    zwp_input_method_v2_delete_surrounding_text(
        d_ptr->input_method_ptr, before_length, after_length);
}

void input_method_v2::commit()
{
    zwp_input_method_v2_commit(d_ptr->input_method_ptr, d_ptr->serial);
}

input_popup_surface_v2* input_method_v2::get_input_popup_surface(Surface* surface, QObject* parent)
{
    assert(isValid());
    auto ips = new input_popup_surface_v2(parent);
    auto wlips = zwp_input_method_v2_get_input_popup_surface(d_ptr->input_method_ptr, *surface);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(wlips);
    }
    ips->setup(wlips);
    return ips;
}

input_method_keyboard_grab_v2* input_method_v2::grab_keyboard(QObject* parent)
{
    assert(isValid());
    auto kg = new input_method_keyboard_grab_v2(parent);
    auto wlkg = zwp_input_method_v2_grab_keyboard(d_ptr->input_method_ptr);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(wlkg);
    }
    kg->setup(wlkg);
    return kg;
}

input_popup_surface_v2::Private::Private(input_popup_surface_v2* q)
    : q_ptr{q}
{
}

zwp_input_popup_surface_v2_listener const input_popup_surface_v2::Private::s_listener = {
    text_input_rectangle_callback,
};

void input_popup_surface_v2::Private::text_input_rectangle_callback(
    void* data,
    zwp_input_popup_surface_v2* zwp_input_popup_surface_v2,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height)
{
    auto priv = reinterpret_cast<input_popup_surface_v2::Private*>(data);
    assert(priv->input_popup_ptr == zwp_input_popup_surface_v2);
    priv->text_input_rectangle = QRect(x, y, width, height);
    Q_EMIT priv->q_ptr->text_input_rectangle_changed();
}

void input_popup_surface_v2::Private::setup(zwp_input_popup_surface_v2* input_popup_surface)
{
    assert(input_popup_surface);
    assert(!input_popup_ptr);
    input_popup_ptr.setup(input_popup_surface);
    zwp_input_popup_surface_v2_add_listener(input_popup_surface, &s_listener, this);
}

bool input_popup_surface_v2::Private::isValid() const
{
    return input_popup_ptr.isValid();
}

input_popup_surface_v2::input_popup_surface_v2(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

input_popup_surface_v2::~input_popup_surface_v2()
{
    release();
}

void input_popup_surface_v2::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* input_popup_surface_v2::event_queue() const
{
    return d_ptr->queue;
}

bool input_popup_surface_v2::isValid() const
{
    return d_ptr->isValid();
}

void input_popup_surface_v2::setup(zwp_input_popup_surface_v2* input_popup_surface)
{
    d_ptr->setup(input_popup_surface);
}

void input_popup_surface_v2::release()
{
    d_ptr->input_popup_ptr.release();
}

input_popup_surface_v2::operator zwp_input_popup_surface_v2*()
{
    return d_ptr->input_popup_ptr;
}

input_popup_surface_v2::operator zwp_input_popup_surface_v2*() const
{
    return d_ptr->input_popup_ptr;
}

QRect input_popup_surface_v2::text_input_rectangle() const
{
    return d_ptr->text_input_rectangle;
}

input_method_keyboard_grab_v2::Private::Private(input_method_keyboard_grab_v2* q)
    : q_ptr{q}
{
}

zwp_input_method_keyboard_grab_v2_listener const input_method_keyboard_grab_v2::Private::s_listener
    = {
        keymap_callback,
        key_callback,
        modifiers_callback,
        repeat_info_callback,
};

void input_method_keyboard_grab_v2::Private::keymap_callback(
    void* data,
    zwp_input_method_keyboard_grab_v2* zwp_input_method_keyboard_grab_v2,
    uint32_t format,
    int fd,
    uint32_t size)
{
    auto priv = reinterpret_cast<input_method_keyboard_grab_v2::Private*>(data);
    assert(priv->keyboard_grab_ptr == zwp_input_method_keyboard_grab_v2);

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        return;
    }
    Q_EMIT priv->q_ptr->keymap_changed(fd, size);
}

void input_method_keyboard_grab_v2::Private::key_callback(
    void* data,
    zwp_input_method_keyboard_grab_v2* zwp_input_method_keyboard_grab_v2,
    [[maybe_unused]] uint32_t serial,
    uint32_t time,
    uint32_t key,
    uint32_t state)
{
    auto priv = reinterpret_cast<input_method_keyboard_grab_v2::Private*>(data);
    assert(priv->keyboard_grab_ptr == zwp_input_method_keyboard_grab_v2);

    auto to_state = [state] {
        if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
            return Keyboard::KeyState::Released;
        } else {
            return Keyboard::KeyState::Pressed;
        }
    };
    Q_EMIT priv->q_ptr->key_changed(key, to_state(), time);
}

void input_method_keyboard_grab_v2::Private::modifiers_callback(
    void* data,
    zwp_input_method_keyboard_grab_v2* zwp_input_method_keyboard_grab_v2,
    [[maybe_unused]] uint32_t serial,
    uint32_t depressed,
    uint32_t latched,
    uint32_t locked,
    uint32_t group)
{
    auto priv = reinterpret_cast<input_method_keyboard_grab_v2::Private*>(data);
    assert(priv->keyboard_grab_ptr == zwp_input_method_keyboard_grab_v2);

    Q_EMIT priv->q_ptr->modifiers_changed(depressed, latched, locked, group);
}

void input_method_keyboard_grab_v2::Private::repeat_info_callback(
    void* data,
    zwp_input_method_keyboard_grab_v2* zwp_input_method_keyboard_grab_v2,
    int32_t rate,
    int32_t delay)
{
    auto priv = reinterpret_cast<input_method_keyboard_grab_v2::Private*>(data);
    assert(priv->keyboard_grab_ptr == zwp_input_method_keyboard_grab_v2);

    priv->repeat_info.rate = std::max(rate, 0);
    priv->repeat_info.delay = std::max(delay, 0);
    Q_EMIT priv->q_ptr->repeat_changed();
}

void input_method_keyboard_grab_v2::Private::setup(zwp_input_method_keyboard_grab_v2* keyboard_grab)
{
    assert(keyboard_grab);
    assert(!keyboard_grab_ptr);
    keyboard_grab_ptr.setup(keyboard_grab);
    zwp_input_method_keyboard_grab_v2_add_listener(keyboard_grab, &s_listener, this);
}

bool input_method_keyboard_grab_v2::Private::isValid() const
{
    return keyboard_grab_ptr.isValid();
}

input_method_keyboard_grab_v2::input_method_keyboard_grab_v2(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

input_method_keyboard_grab_v2::~input_method_keyboard_grab_v2()
{
    release();
}

void input_method_keyboard_grab_v2::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* input_method_keyboard_grab_v2::event_queue() const
{
    return d_ptr->queue;
}

bool input_method_keyboard_grab_v2::isValid() const
{
    return d_ptr->isValid();
}

void input_method_keyboard_grab_v2::setup(zwp_input_method_keyboard_grab_v2* keyboard_grab)
{
    d_ptr->setup(keyboard_grab);
}

void input_method_keyboard_grab_v2::release()
{
    d_ptr->keyboard_grab_ptr.release();
}

input_method_keyboard_grab_v2::operator zwp_input_method_keyboard_grab_v2*()
{
    return d_ptr->keyboard_grab_ptr;
}

input_method_keyboard_grab_v2::operator zwp_input_method_keyboard_grab_v2*() const
{
    return d_ptr->keyboard_grab_ptr;
}

int32_t input_method_keyboard_grab_v2::repeat_rate() const
{
    return d_ptr->repeat_info.rate;
}

int32_t input_method_keyboard_grab_v2::repeat_delay() const
{
    return d_ptr->repeat_info.delay;
}

}
