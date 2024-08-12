/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "virtual_keyboard_v1.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QObject>

#include <wayland-virtual-keyboard-v1-server-protocol.h>

namespace Wrapland::Server
{

class Display;

constexpr uint32_t virtual_keyboard_manager_v1_version = 1;
using virtual_keyboard_manager_v1_global
    = Wayland::Global<virtual_keyboard_manager_v1, virtual_keyboard_manager_v1_version>;

class virtual_keyboard_manager_v1::Private : public virtual_keyboard_manager_v1_global
{
public:
    Private(Display* display, virtual_keyboard_manager_v1* q_ptr);

private:
    static void create_virtual_keyboard_callback(virtual_keyboard_manager_v1_global::bind_t* bind,
                                                 wl_resource* wlSeat,
                                                 uint32_t id);

    static const struct zwp_virtual_keyboard_manager_v1_interface s_interface;
};

class virtual_keyboard_v1::Private : public Wayland::Resource<virtual_keyboard_v1>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, virtual_keyboard_v1* q_ptr);

private:
    static void keymap_callback(wl_client* wlClient,
                                wl_resource* wlResource,
                                uint32_t format,
                                int32_t fd,
                                uint32_t size);
    static void key_callback(wl_client* wlClient,
                             wl_resource* wlResource,
                             uint32_t time,
                             uint32_t key,
                             uint32_t state);

    static void modifiers_callback(wl_client* wlClient,
                                   wl_resource* wlResource,
                                   uint32_t depressed,
                                   uint32_t latched,
                                   uint32_t locked,
                                   uint32_t group);

    bool check_keymap_set();

    bool keymap_set{false};

    static struct zwp_virtual_keyboard_v1_interface const s_interface;
};

}
