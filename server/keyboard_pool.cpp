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

void keyboard_pool::create_device(Client* client, uint32_t version, uint32_t id)
{
    auto keyboard = new Keyboard(client, version, id, seat);
    if (!keyboard) {
        return;
    }

    keyboard->repeatInfo(keyRepeat.charactersPerSecond, keyRepeat.delay);

    if (keymap.xkbcommonCompatible) {
        keyboard->setKeymap(keymap.content);
    }

    devices.push_back(keyboard);

    if (focus.surface && focus.surface->client() == keyboard->client()) {
        // this is a keyboard for the currently focused keyboard surface
        focus.devices.push_back(keyboard);
        keyboard->setFocusedSurface(focus.serial, focus.surface);
    }

    QObject::connect(keyboard, &Keyboard::resourceDestroyed, seat, [keyboard, this] {
        remove_one(devices, keyboard);
        remove_one(focus.devices, keyboard);
    });

    Q_EMIT seat->keyboardCreated(keyboard);
}

enum class button_state {
    released,
    pressed,
};

bool keyboard_pool::update_key(uint32_t key, button_state state)
{
    auto it = states.find(key);
    if (it != states.end() && it->second == state) {
        return false;
    }
    states[key] = state;
    return true;
}

void keyboard_pool::key_pressed(uint32_t key)
{
    lastStateSerial = seat->d_ptr->display()->handle()->nextSerial();
    if (!update_key(key, button_state::pressed)) {
        return;
    }
    if (focus.surface) {
        for (auto kbd : focus.devices) {
            kbd->keyPressed(lastStateSerial, key);
        }
    }
}

void keyboard_pool::key_released(uint32_t key)
{
    lastStateSerial = seat->d_ptr->display()->handle()->nextSerial();
    if (!update_key(key, button_state::released)) {
        return;
    }
    if (focus.surface) {
        for (auto kbd : focus.devices) {
            kbd->keyReleased(lastStateSerial, key);
        }
    }
}

void keyboard_pool::update_modifiers(uint32_t depressed,
                                     uint32_t latched,
                                     uint32_t locked,
                                     uint32_t group)
{
    bool changed = false;

    auto mods = keyboard_modifiers{depressed, latched, locked, group, modifiers.serial};
    if (modifiers != mods) {
        modifiers = mods;
        changed = true;
    }

    if (!changed) {
        return;
    }

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
        QObject::disconnect(focus.destroyConnection);
    }

    focus = {};
    focus.devices = interfacesForSurface(surface, devices);

    if (surface) {
        focus.surface = surface;
        focus.serial = serial;
        focus.destroyConnection
            = QObject::connect(surface, &Surface::resourceDestroyed, seat, [this] { focus = {}; });
    }

    for (auto kbd : focus.devices) {
        kbd->setFocusedSurface(serial, surface);
    }
}

void keyboard_pool::set_keymap(std::string const& content)
{
    keymap.xkbcommonCompatible = true;
    keymap.content = content;
    for (auto device : devices) {
        device->setKeymap(content);
    }
}

void keyboard_pool::set_repeat_info(int32_t charactersPerSecond, int32_t delay)
{
    keyRepeat.charactersPerSecond = qMax(charactersPerSecond, 0);
    keyRepeat.delay = qMax(delay, 0);
    for (auto device : devices) {
        device->repeatInfo(keyRepeat.charactersPerSecond, keyRepeat.delay);
    }
}

std::vector<uint32_t> keyboard_pool::pressed_keys() const
{
    std::vector<uint32_t> keys;
    for (auto const& [serial, state] : states) {
        if (state == button_state::pressed) {
            keys.push_back(serial);
        }
    }
    return keys;
}

}
