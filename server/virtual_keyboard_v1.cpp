/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "virtual_keyboard_v1_p.h"

#include "display.h"
#include "seat_p.h"

namespace Wrapland::Server
{

struct zwp_virtual_keyboard_manager_v1_interface const
    virtual_keyboard_manager_v1::Private::s_interface
    = {
        cb<create_virtual_keyboard_callback>,
};

virtual_keyboard_manager_v1::Private::Private(Display* display, virtual_keyboard_manager_v1* q_ptr)
    : virtual_keyboard_manager_v1_global(q_ptr,
                                         display,
                                         &zwp_virtual_keyboard_manager_v1_interface,
                                         &s_interface)
{
    create();
}

void virtual_keyboard_manager_v1::Private::create_virtual_keyboard_callback(
    virtual_keyboard_manager_v1_bind* bind,
    wl_resource* wlSeat,
    uint32_t id)
{
    auto handle = bind->global()->handle();
    auto seat = SeatGlobal::handle(wlSeat);

    auto vk = new virtual_keyboard_v1(bind->client()->handle(), bind->version(), id);
    Q_EMIT handle->keyboard_created(vk, seat);
}

virtual_keyboard_manager_v1::virtual_keyboard_manager_v1(Display* display)
    : d_ptr(new Private(display, this))
{
}

virtual_keyboard_manager_v1::~virtual_keyboard_manager_v1() = default;

struct zwp_virtual_keyboard_v1_interface const virtual_keyboard_v1::Private::s_interface = {
    keymap_callback,
    key_callback,
    modifiers_callback,
    destroyCallback,
};

virtual_keyboard_v1::Private::Private(Client* client,
                                      uint32_t version,
                                      uint32_t id,
                                      virtual_keyboard_v1* q_ptr)
    : Wayland::Resource<virtual_keyboard_v1>(client,
                                             version,
                                             id,
                                             &zwp_virtual_keyboard_v1_interface,
                                             &s_interface,
                                             q_ptr)
{
}

void virtual_keyboard_v1::Private::keymap_callback(wl_client* /*wlClient*/,
                                                   wl_resource* wlResource,
                                                   uint32_t format,
                                                   int32_t fd,
                                                   uint32_t size)
{
    auto vk = handle(wlResource);

    vk->d_ptr->keymap_set = true;
    Q_EMIT vk->keymap(format, fd, size);
}

void virtual_keyboard_v1::Private::key_callback(wl_client* /*wlClient*/,
                                                wl_resource* wlResource,
                                                uint32_t time,
                                                uint32_t key,
                                                uint32_t state)
{
    auto vk = handle(wlResource);

    if (!vk->d_ptr->check_keymap_set()) {
        return;
    }

    Q_EMIT
    vk->key(time, key, state ? key_state::pressed : key_state::released);
}

void virtual_keyboard_v1::Private::modifiers_callback(wl_client* /*wlClient*/,
                                                      wl_resource* wlResource,
                                                      uint32_t depressed,
                                                      uint32_t latched,
                                                      uint32_t locked,
                                                      uint32_t group)
{
    auto vk = handle(wlResource);

    if (!vk->d_ptr->check_keymap_set()) {
        return;
    }

    Q_EMIT vk->modifiers(depressed, latched, locked, group);
}

bool virtual_keyboard_v1::Private::check_keymap_set()
{
    if (keymap_set) {
        return true;
    }
    postError(ZWP_VIRTUAL_KEYBOARD_V1_ERROR_NO_KEYMAP, "No keymap was set");
    return false;
}

virtual_keyboard_v1::virtual_keyboard_v1(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}

}
