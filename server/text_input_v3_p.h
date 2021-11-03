/*
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>
    SPDX-FileCopyrightText: 2021 dcz <gihuac.dcz@porcupinefactory.org>
    SPDX-FileCopyrightText: 2021 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "text_input_v3.h"

#include "wayland/global.h"

#include <wayland-text-input-unstable-v3-server-protocol.h>

namespace Wrapland::Server
{

uint32_t convert_content_hints(text_input_v3_content_hints hints);
uint32_t convert_content_purpose(text_input_v3_content_purpose purpose);
uint32_t convert_change_cause(text_input_v3_change_cause cause);

constexpr uint32_t text_input_manager_v3_version = 1;
using text_input_manager_v3_global
    = Wayland::Global<text_input_manager_v3, text_input_manager_v3_version>;
using text_input_manager_v3_bind = Wayland::Bind<text_input_manager_v3_global>;

class text_input_manager_v3::Private : public text_input_manager_v3_global
{
public:
    Private(Display* display, text_input_manager_v3* q);

private:
    static void
    get_text_input_callback(text_input_manager_v3_bind* bind, uint32_t id, wl_resource* wlSeat);

    static struct zwp_text_input_manager_v3_interface const s_interface;
};

class text_input_v3::Private : public Wayland::Resource<text_input_v3>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, text_input_v3* q_ptr);

    void send_enter(Surface* surface);
    void send_leave(Surface* surface);

    Seat* seat{nullptr};
    Surface* entered_surface{nullptr};
    uint32_t serial{0};

    text_input_v3_state current;
    text_input_v3_state pending;

    text_input_v3* q_ptr;

private:
    static void enable_callback(wl_client* wlClient, wl_resource* wlResource);
    static void disable_callback(wl_client* wlClient, wl_resource* wlResource);

    static void set_surrounding_text_callback(wl_client* wlClient,
                                              wl_resource* wlResource,
                                              const char* text,
                                              int32_t cursor,
                                              int32_t anchor);
    static void set_content_type_callback(wl_client* wlClient,
                                          wl_resource* wlResource,
                                          uint32_t hint,
                                          uint32_t purpose);
    static void set_cursor_rectangle_callback(wl_client* wlClient,
                                              wl_resource* wlResource,
                                              int32_t x,
                                              int32_t y,
                                              int32_t width,
                                              int32_t height);
    static void set_text_change_cause_callback(wl_client* wlClient,
                                               wl_resource* wlResource,
                                               uint32_t change_cause);
    static void set_commit_callback(wl_client* wlClient, wl_resource* wlResource);

    static struct zwp_text_input_v3_interface const s_interface;
};
}
