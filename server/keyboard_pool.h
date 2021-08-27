/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <cstdint>
#include <unordered_map>

namespace Wrapland::Server
{
class Client;
class Keyboard;
class Surface;
class DataDevice;
class PrimarySelectionDevice;
class Seat;

enum class button_state;

struct keyboard_focus {
    Surface* surface = nullptr;
    std::vector<Keyboard*> devices;
    QMetaObject::Connection destroyConnection;
    uint32_t serial = 0;
    std::vector<DataDevice*> selections;
    std::vector<PrimarySelectionDevice*> primarySelections;
};

struct keyboard_map {
    int fd = -1;
    std::string content;
    bool xkbcommonCompatible = false;
};

struct keyboard_modifiers {
    uint32_t depressed = 0;
    uint32_t latched = 0;
    uint32_t locked = 0;
    uint32_t group = 0;
    uint32_t serial = 0;

    bool operator==(keyboard_modifiers const& other) const
    {
        return depressed == other.depressed && latched == other.latched && locked == other.locked
            && group == other.group && serial == other.serial;
    }
    bool operator!=(keyboard_modifiers const& other) const
    {
        return !(*this == other);
    }
};

struct keyboard_repeat_info {
    int32_t charactersPerSecond = 0;
    int32_t delay = 0;
};

/*
 * Handle keyboard devices associated to a seat.
 *
 * Clients are allowed to mantain multiple keyboards for the same seat,
 * that can receive focus and should be updated together.
 * On seats with keyboard capability, keyboard focus is relevant for
 * selections and text input.
 */
class keyboard_pool
{
public:
    explicit keyboard_pool(Seat* seat);

    void key_pressed(uint32_t key);
    void key_released(uint32_t key);
    void update_modifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);
    void set_focused_surface(Surface* surface);
    void set_keymap(std::string const& content);
    void set_repeat_info(int32_t charactersPerSecond, int32_t delay);

    std::vector<uint32_t> pressed_keys() const;

    void create_device(Client* client, uint32_t version, uint32_t id);

    keyboard_map keymap;
    keyboard_modifiers modifiers;
    keyboard_focus focus;
    keyboard_repeat_info keyRepeat;

    std::unordered_map<uint32_t, button_state> states;
    uint32_t lastStateSerial = 0;

    bool update_key(uint32_t key, button_state state);
    Seat* seat;
    std::vector<Keyboard*> devices;
};

}
