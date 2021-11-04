/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "keyboard_pool.h"
#include "display.h"
#include "keyboard.h"
#include "keyboard_p.h"
#include "seat.h"
#include "seat_p.h"
#include "utils.h"

#include <algorithm>

namespace Wrapland::Server
{

keyboard_pool::keyboard_pool(Seat* seat)
    : seat{seat}
{
}

keyboard_pool::~keyboard_pool()
{
    QObject::disconnect(focus.surface_lost_notifier);
    for (auto dev : devices) {
        QObject::disconnect(dev, nullptr, seat, nullptr);
    }
}

keyboard_focus const& keyboard_pool::get_focus() const
{
    return focus;
}

keyboard_modifiers const& keyboard_pool::get_modifiers() const
{
    return modifiers;
}

keyboard_repeat_info const& keyboard_pool::get_repeat_info() const
{
    return keyRepeat;
}

void keyboard_pool::create_device(Client* client, uint32_t version, uint32_t id)
{
    auto keyboard = new Keyboard(client, version, id, seat);
    if (!keyboard) {
        return;
    }

    keyboard->repeatInfo(keyRepeat.rate, keyRepeat.delay);

    devices.push_back(keyboard);

    if (focus.surface && focus.surface->client() == keyboard->client()) {
        // this is a keyboard for the currently focused keyboard surface
        if (keymap) {
            keyboard->setKeymap(keymap);
        }
        focus.devices.push_back(keyboard);
        keyboard->setFocusedSurface(focus.serial, focus.surface);
    }

    QObject::connect(keyboard, &Keyboard::resourceDestroyed, seat, [keyboard, this] {
        remove_one(devices, keyboard);
        remove_one(focus.devices, keyboard);
    });

    Q_EMIT seat->keyboardCreated(keyboard);
}

bool keyboard_pool::update_key(uint32_t key, key_state state)
{
    auto it = states.find(key);
    if (it != states.end() && it->second == state) {
        return false;
    }
    states[key] = state;
    return true;
}

void keyboard_pool::key(uint32_t key, key_state state)
{
    lastStateSerial = seat->d_ptr->display()->handle()->nextSerial();
    if (!update_key(key, state)) {
        return;
    }
    if (focus.surface) {
        for (auto kbd : focus.devices) {
            kbd->key(lastStateSerial, key, state);
        }
    }
}

void keyboard_pool::update_modifiers(uint32_t depressed,
                                     uint32_t latched,
                                     uint32_t locked,
                                     uint32_t group)
{
    auto mods = keyboard_modifiers{depressed, latched, locked, group, modifiers.serial};

    if (modifiers == mods) {
        return;
    }

    modifiers = mods;

    auto const serial = seat->d_ptr->display()->handle()->nextSerial();
    modifiers.serial = serial;

    if (focus.surface) {
        for (auto& keyboard : focus.devices) {
            keyboard->updateModifiers(serial, depressed, latched, locked, group);
        }
    }
}

void keyboard_pool::set_focused_surface(Surface* surface)
{
    auto const serial = seat->d_ptr->display()->handle()->nextSerial();

    for (auto kbd : focus.devices) {
        kbd->setFocusedSurface(serial, nullptr);
    }

    if (focus.surface) {
        QObject::disconnect(focus.surface_lost_notifier);
    }

    focus = {};
    focus.devices = interfacesForSurface(surface, devices);

    if (surface) {
        focus.surface = surface;
        focus.serial = serial;
        focus.surface_lost_notifier
            = QObject::connect(surface, &Surface::resourceDestroyed, seat, [this] { focus = {}; });
    }

    for (auto kbd : focus.devices) {
        if (kbd->d_ptr->needs_keymap_update && keymap) {
            kbd->setKeymap(keymap);
        }
        kbd->setFocusedSurface(serial, surface);
    }
}

void keyboard_pool::set_keymap(char const* keymap)
{
    if (this->keymap == keymap) {
        return;
    }

    this->keymap = keymap;

    for (auto device : devices) {
        device->d_ptr->needs_keymap_update = true;
    }
    if (keymap) {
        for (auto device : focus.devices) {
            device->setKeymap(keymap);
        }
    }
}

void keyboard_pool::set_repeat_info(int32_t charactersPerSecond, int32_t delay)
{
    keyRepeat.rate = qMax(charactersPerSecond, 0);
    keyRepeat.delay = qMax(delay, 0);
    for (auto device : devices) {
        device->repeatInfo(keyRepeat.rate, keyRepeat.delay);
    }
}

std::vector<uint32_t> keyboard_pool::pressed_keys() const
{
    std::vector<uint32_t> keys;
    for (auto const& [serial, state] : states) {
        if (state == key_state::pressed) {
            keys.push_back(serial);
        }
    }
    return keys;
}

}
