/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "text_input_v3.h"

#include "wayland_pointer_p.h"

#include <QObject>

#include <wayland-text-input-v3-client-protocol.h>

namespace Wrapland::Client
{

uint32_t convert_content_hints(text_input_v3_content_hints hints);
uint32_t convert_content_purpose(text_input_v3_content_purpose purpose);
uint32_t convert_change_cause(text_input_v3_change_cause cause);

text_input_v3_content_hints convert_content_hints(uint32_t cause);
text_input_v3_content_purpose convert_content_purpose(uint32_t purpose);
text_input_v3_change_cause convert_change_cause(uint32_t cause);

class Q_DECL_HIDDEN text_input_manager_v3::Private
{
public:
    ~Private() = default;

    void release();
    bool isValid();
    text_input_v3* get_text_input(Seat* seat, QObject* parent = nullptr);

    operator zwp_text_input_manager_v3*()
    {
        return manager_ptr;
    }
    operator zwp_text_input_manager_v3*() const
    {
        return manager_ptr;
    }

    WaylandPointer<zwp_text_input_manager_v3, zwp_text_input_manager_v3_destroy> manager_ptr;

    EventQueue* queue = nullptr;
};

class Q_DECL_HIDDEN text_input_v3::Private
{
public:
    Private(Seat* seat, text_input_v3* q);
    virtual ~Private() = default;

    void setup(zwp_text_input_v3* ti);

    bool isValid() const;

    void enable();
    void disable();
    void reset();

    WaylandPointer<zwp_text_input_v3, zwp_text_input_v3_destroy> text_input_ptr;

    EventQueue* queue{nullptr};
    Seat* seat;
    Surface* entered_surface{nullptr};
    uint32_t serial{0};

    text_input_v3_state current;
    text_input_v3_state pending;

private:
    static void
    enter_callback(void* data, zwp_text_input_v3* zwp_text_input_v3, wl_surface* surface);
    static void
    leave_callback(void* data, zwp_text_input_v3* zwp_text_input_v3, wl_surface* surface);
    static void preedit_string_callback(void* data,
                                        zwp_text_input_v3* zwp_text_input_v3,
                                        char const* text,
                                        int32_t cursor_begin,
                                        int32_t cursor_end);
    static void
    commit_string_callback(void* data, zwp_text_input_v3* zwp_text_input_v3, char const* text);
    static void delete_surrounding_text_callback(void* data,
                                                 zwp_text_input_v3* zwp_text_input_v3,
                                                 uint32_t before_length,
                                                 uint32_t after_length);
    static void done_callback(void* data, zwp_text_input_v3* zwp_text_input_v3, uint32_t serial);

    text_input_v3* q_ptr;

    static zwp_text_input_v3_listener const s_listener;
};

}
