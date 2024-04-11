/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "input_method_v2.h"

#include "wayland_pointer_p.h"

#include <QObject>

#include <wayland-input-method-v2-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN input_method_manager_v2::Private
{
public:
    ~Private() = default;

    void release();
    bool isValid();
    input_method_v2* get_input_method(Seat* seat, QObject* parent = nullptr);

    operator zwp_input_method_manager_v2*()
    {
        return manager_ptr;
    }
    operator zwp_input_method_manager_v2*() const
    {
        return manager_ptr;
    }

    WaylandPointer<zwp_input_method_manager_v2, zwp_input_method_manager_v2_destroy> manager_ptr;

    EventQueue* queue = nullptr;
};

class Q_DECL_HIDDEN input_method_v2::Private
{
public:
    Private(Seat* seat, input_method_v2* q);
    virtual ~Private() = default;

    void setup(zwp_input_method_v2* input_method);

    bool isValid() const;

    void enable();
    void disable();
    void reset();

    WaylandPointer<zwp_input_method_v2, zwp_input_method_v2_destroy> input_method_ptr;

    EventQueue* queue{nullptr};
    Seat* seat;
    Surface* entered_surface{nullptr};
    uint32_t serial{0};

    input_method_v2_state current;
    input_method_v2_state pending;

private:
    static void activate_callback(void* data, zwp_input_method_v2* zwp_input_method_v2);
    static void deactivate_callback(void* data, zwp_input_method_v2* zwp_input_method_v2);
    static void surrounding_text_callback(void* data,
                                          zwp_input_method_v2* zwp_input_method_v2,
                                          char const* text,
                                          uint32_t cursor,
                                          uint32_t anchor);
    static void text_change_cause_callback(void* data,
                                           zwp_input_method_v2* zwp_input_method_v2,
                                           uint32_t cause);
    static void content_type_callback(void* data,
                                      zwp_input_method_v2* zwp_input_method_v2,
                                      uint32_t hint,
                                      uint32_t purpose);
    static void done_callback(void* data, zwp_input_method_v2* zwp_input_method_v2);
    static void unavailable_callback(void* data, zwp_input_method_v2* zwp_input_method_v2);

    input_method_v2* q_ptr;

    static zwp_input_method_v2_listener const s_listener;
};

class Q_DECL_HIDDEN input_popup_surface_v2::Private
{
public:
    Private(input_popup_surface_v2* q);
    virtual ~Private() = default;

    void setup(zwp_input_popup_surface_v2* input_popup_surface);

    bool isValid() const;

    void enable();
    void disable();
    void reset();

    QRect text_input_rectangle;

    WaylandPointer<zwp_input_popup_surface_v2, zwp_input_popup_surface_v2_destroy> input_popup_ptr;

    EventQueue* queue{nullptr};

private:
    static void
    text_input_rectangle_callback(void* data,
                                  zwp_input_popup_surface_v2* zwp_input_popup_surface_v2,
                                  int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height);

    input_popup_surface_v2* q_ptr;

    static zwp_input_popup_surface_v2_listener const s_listener;
};

class Q_DECL_HIDDEN input_method_keyboard_grab_v2::Private
{
public:
    Private(input_method_keyboard_grab_v2* q);
    virtual ~Private() = default;

    void setup(zwp_input_method_keyboard_grab_v2* keyboard_grab);

    bool isValid() const;

    void enable();
    void disable();
    void reset();

    struct {
        int32_t rate{0};
        int32_t delay{0};
    } repeat_info;

    WaylandPointer<zwp_input_method_keyboard_grab_v2, zwp_input_method_keyboard_grab_v2_release>
        keyboard_grab_ptr;

    EventQueue* queue{nullptr};

private:
    static void
    keymap_callback(void* data,
                    zwp_input_method_keyboard_grab_v2* zwp_input_method_keyboard_grab_v2,
                    uint32_t format,
                    int fd,
                    uint32_t size);
    static void key_callback(void* data,
                             zwp_input_method_keyboard_grab_v2* zwp_input_method_keyboard_grab_v2,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t key,
                             uint32_t state);
    static void
    modifiers_callback(void* data,
                       zwp_input_method_keyboard_grab_v2* zwp_input_method_keyboard_grab_v2,
                       uint32_t serial,
                       uint32_t depressed,
                       uint32_t latched,
                       uint32_t locked,
                       uint32_t group);
    static void
    repeat_info_callback(void* data,
                         zwp_input_method_keyboard_grab_v2* zwp_input_method_keyboard_grab_v2,
                         int32_t charactersPerSecond,
                         int32_t delay);

    input_method_keyboard_grab_v2* q_ptr;

    static zwp_input_method_keyboard_grab_v2_listener const s_listener;
};

}
