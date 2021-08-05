/*
    SPDX-FileCopyrightText: 2021 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "input_method_v2.h"

#include "display.h"
#include "keyboard_p.h"
#include "seat_p.h"
#include "surface_p.h"
#include "text_input_v3_p.h"

#include <wayland-input-method-unstable-v2-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t input_method_manager_v2_version = 1;
using input_method_manager_v2_global
    = Wayland::Global<input_method_manager_v2, input_method_manager_v2_version>;
using input_method_manager_v2_bind = Wayland::Bind<input_method_manager_v2_global>;

class input_method_manager_v2::Private : public input_method_manager_v2_global
{
private:
    static void
    get_input_method_callback(input_method_manager_v2_bind* bind, wl_resource* wlSeat, uint32_t id);

    static struct zwp_input_method_manager_v2_interface const s_interface;

public:
    Private(Display* display, input_method_manager_v2* q);
};

class input_method_v2::Private : public Wayland::Resource<input_method_v2>
{
private:
    static struct zwp_input_method_v2_interface const s_interface;

public:
    Private(Client* client, uint32_t version, uint32_t id, input_method_v2* q);

    Seat* seat{nullptr};
    uint32_t serial{0};

    input_method_v2_state current;
    input_method_v2_state pending;

    input_method_v2* q_ptr;

private:
    static void commit_string_callback([[maybe_unused]] wl_client* wlClient,
                                       wl_resource* wlResource,
                                       char const* text);
    static void preedit_string_callback([[maybe_unused]] wl_client* wlClient,
                                        wl_resource* wlResource,
                                        char const* text,
                                        int32_t cursor_begin,
                                        int32_t cursor_end);
    static void delete_surrounding_text_callback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 uint32_t beforeBytes,
                                                 uint32_t afterBytes);
    static void
    commit_callback([[maybe_unused]] wl_client* wlClient, wl_resource* wlResource, uint32_t serial);
    static void get_input_popup_surface_callback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 uint32_t id,
                                                 wl_resource* wlSurface);
    static void grab_keyboard_callback([[maybe_unused]] wl_client* wlClient,
                                       wl_resource* wlResource,
                                       uint32_t id);
};

class input_method_keyboard_grab_v2::Private
    : public Wayland::Resource<input_method_keyboard_grab_v2>
{
private:
    static struct zwp_input_method_keyboard_grab_v2_interface const s_interface;

public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Seat* seat,
            input_method_keyboard_grab_v2* q);

    Seat* seat;
    file_wrap keymap;
};

class input_method_popup_surface_v2::Private
    : public Wayland::Resource<input_method_popup_surface_v2>
{
private:
    static struct zwp_input_popup_surface_v2_interface const s_interface;

public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Surface* surface,
            input_method_popup_surface_v2* q);

    Surface* surface;
};

}
