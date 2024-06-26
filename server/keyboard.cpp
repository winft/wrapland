/********************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "keyboard.h"
#include "keyboard_p.h"

#include "client.h"
#include "display.h"
#include "seat.h"
#include "surface.h"
#include "surface_p.h"

#include <QVector>
#include <cstring>

#include <wayland-server.h>

namespace Wrapland::Server
{

Keyboard::Private::Private(Client* client,
                           uint32_t version,
                           uint32_t id,
                           Seat* _seat,
                           Keyboard* q_ptr)
    : Wayland::Resource<Keyboard>(client, version, id, &wl_keyboard_interface, &s_interface, q_ptr)
    , seat(_seat)
    , q_ptr{q_ptr}
{
}

const struct wl_keyboard_interface Keyboard::Private::s_interface {
    destroyCallback,
};

void Keyboard::Private::sendLeave(quint32 serial, Surface* surface)
{
    if (surface && surface->d_ptr->resource) {
        send<wl_keyboard_send_leave>(serial, surface->d_ptr->resource);
    }
}

void Keyboard::Private::sendEnter(quint32 serial, Surface* surface)
{
    wl_array keys;
    wl_array_init(&keys);
    auto const& states = seat->keyboards().pressed_keys();
    for (auto const btn : states) {
        auto key = static_cast<uint32_t*>(wl_array_add(&keys, sizeof(uint32_t)));
        *key = btn;
    }
    send<wl_keyboard_send_enter>(serial, surface->d_ptr->resource, &keys);
    wl_array_release(&keys);

    sendModifiers();
}

void Keyboard::Private::sendKeymap(int fd, quint32 size)
{
    send<wl_keyboard_send_keymap>(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
}

void Keyboard::Private::sendModifiers(quint32 serial,
                                      quint32 depressed,
                                      quint32 latched,
                                      quint32 locked,
                                      quint32 group)
{
    send<wl_keyboard_send_modifiers>(serial, depressed, latched, locked, group);
}

void Keyboard::Private::sendModifiers()
{
    auto const mods = seat->keyboards().get_modifiers();
    sendModifiers(mods.serial, mods.depressed, mods.latched, mods.locked, mods.group);
}

Keyboard::Keyboard(Client* client, uint32_t version, uint32_t id, Seat* seat)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, seat, this))
{
    connect(client, &Client::disconnected, this, [this] { disconnect(d_ptr->destroyConnection); });
}

void Keyboard::setKeymap(char const* content)
{
    auto tmpf = std::tmpfile();

    if (auto rc = std::fputs(content, tmpf); rc < 0) {
        qCWarning(WRAPLAND_SERVER, "Failed to set keyboard keymap with %d.", rc);
        // TODO(romangg): Handle error by closing file here and returning?
    }
    if (std::fseek(tmpf, 0, SEEK_SET) != 0) {
        qCWarning(WRAPLAND_SERVER, "Failed to seek begin of keyboard keymap.");
        // TODO(romangg): Handle error by closing file here and returning?
    }

    d_ptr->sendKeymap(fileno(tmpf), strlen(content));
    d_ptr->keymap = file_wrap(tmpf);
    d_ptr->needs_keymap_update = false;
}

void Keyboard::setFocusedSurface(quint32 serial, Surface* surface)
{
    d_ptr->sendLeave(serial, d_ptr->focusedSurface);
    disconnect(d_ptr->destroyConnection);
    d_ptr->focusedSurface = surface;
    if (!d_ptr->focusedSurface) {
        return;
    }
    d_ptr->destroyConnection = connect(
        d_ptr->focusedSurface, &Surface::resourceDestroyed, this, [this] {
            d_ptr->sendLeave(d_ptr->client->display()->handle->nextSerial(), d_ptr->focusedSurface);
            d_ptr->focusedSurface = nullptr;
        });

    d_ptr->sendEnter(serial, d_ptr->focusedSurface);
    d_ptr->client->flush();
}

void Keyboard::key(uint32_t serial, uint32_t key, key_state state)
{
    Q_ASSERT(d_ptr->focusedSurface);
    d_ptr->send<wl_keyboard_send_key>(serial,
                                      d_ptr->seat->timestamp(),
                                      key,
                                      state == key_state::pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                                                                  : WL_KEYBOARD_KEY_STATE_RELEASED);
}

void Keyboard::updateModifiers(quint32 serial,
                               quint32 depressed,
                               quint32 latched,
                               quint32 locked,
                               quint32 group)
{
    Q_ASSERT(d_ptr->focusedSurface);
    d_ptr->sendModifiers(serial, depressed, latched, locked, group);
}

void Keyboard::repeatInfo(qint32 charactersPerSecond, qint32 delay)
{
    d_ptr->send<wl_keyboard_send_repeat_info, WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION>(
        charactersPerSecond, delay);
}

Surface* Keyboard::focusedSurface() const
{
    return d_ptr->focusedSurface;
}

Client* Keyboard::client() const
{
    return d_ptr->client->handle;
}

}
