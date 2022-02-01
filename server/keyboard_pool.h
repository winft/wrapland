/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Wrapland::Server
{
class Client;
class Keyboard;
class Surface;
class Seat;

enum class key_state {
    released,
    pressed,
};

struct keyboard_focus {
    Surface* surface{nullptr};
    std::vector<Keyboard*> devices;
    uint32_t serial{0};
    QMetaObject::Connection surface_lost_notifier;
};

struct keyboard_modifiers {
    uint32_t depressed{0};
    uint32_t latched{0};
    uint32_t locked{0};
    uint32_t group{0};
    uint32_t serial{0};

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
    // In chars per second.
    int32_t rate{0};
    // In milliseconds.
    int32_t delay{0};
};

/*
 * Handle keyboard devices associated to a seat.
 *
 * Clients are allowed to mantain multiple keyboards for the same seat,
 * that can receive focus and should be updated together.
 * On seats with keyboard capability, keyboard focus is relevant for
 * selections and text input.
 */
class WRAPLANDSERVER_EXPORT keyboard_pool
{
public:
    explicit keyboard_pool(Seat* seat);
    keyboard_pool(keyboard_pool const&) = delete;
    keyboard_pool& operator=(keyboard_pool const&) = delete;
    keyboard_pool(keyboard_pool&&) noexcept = default;
    keyboard_pool& operator=(keyboard_pool&&) noexcept = default;
    ~keyboard_pool();

    keyboard_focus const& get_focus() const;
    keyboard_modifiers const& get_modifiers() const;
    keyboard_repeat_info const& get_repeat_info() const;

    void key(uint32_t key, key_state state);
    void update_modifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);
    void set_focused_surface(Surface* surface);
    void set_keymap(char const* keymap);
    void set_repeat_info(int32_t charactersPerSecond, int32_t delay);

    std::vector<uint32_t> pressed_keys() const;
    bool update_key(uint32_t key, key_state state);

private:
    friend class Seat;
    void create_device(Client* client, uint32_t version, uint32_t id);

    char const* keymap{nullptr};

    keyboard_focus focus;
    keyboard_modifiers modifiers;
    keyboard_repeat_info keyRepeat;

    std::unordered_map<uint32_t, key_state> states;
    uint32_t lastStateSerial{0};

    std::vector<Keyboard*> devices;
    Seat* seat;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::key_state)
