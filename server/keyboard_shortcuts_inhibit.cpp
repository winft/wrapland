/****************************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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
****************************************************************************/
#include "keyboard_shortcuts_inhibit_p.h"

#include "client.h"
#include "display.h"
#include "seat_p.h"
#include "surface.h"

namespace Wrapland::Server
{

const struct zwp_keyboard_shortcuts_inhibit_manager_v1_interface
    KeyboardShortcutsInhibitManagerV1::Private::s_interface
    = {
        resourceDestroyCallback,
        cb<inhibitShortcutsCallback>,
};

KeyboardShortcutsInhibitManagerV1::Private::Private(Display* display,
                                                    KeyboardShortcutsInhibitManagerV1* q_ptr)
    : Wayland::Global<KeyboardShortcutsInhibitManagerV1>(
          q_ptr,
          display,
          &zwp_keyboard_shortcuts_inhibit_manager_v1_interface,
          &s_interface)
{
    create();
}

void KeyboardShortcutsInhibitManagerV1::Private::inhibitShortcutsCallback(
    KeyboardShortcutsInhibitManagerV1Bind* bind,
    uint32_t id,
    wl_resource* wlSurface,
    wl_resource* wlSeat)
{
    auto priv = bind->global()->handle->d_ptr.get();
    auto seat = SeatGlobal::get_handle(wlSeat);
    auto surface = Wayland::Resource<Surface>::get_handle(wlSurface);

    if (priv->m_inhibitors.contains({surface, seat})) {
        bind->post_error(ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ALREADY_INHIBITED,
                         "the shortcuts are already inhibited for this surface and seat");
        return;
    }

    auto inhibitor
        = new KeyboardShortcutsInhibitorV1(bind->client->handle, bind->version, id, surface, seat);

    QObject::connect(inhibitor,
                     &KeyboardShortcutsInhibitorV1::resourceDestroyed,
                     priv->handle,
                     [priv, surface, seat]() { priv->m_inhibitors.remove({surface, seat}); });

    priv->m_inhibitors[{surface, seat}] = inhibitor;
    Q_EMIT bind->global()->handle->inhibitorCreated(inhibitor);
    inhibitor->setActive(true);
}

KeyboardShortcutsInhibitorV1*
KeyboardShortcutsInhibitManagerV1::Private::findInhibitor(Surface* surface, Seat* seat) const
{
    return m_inhibitors.value({surface, seat}, nullptr);
}

void KeyboardShortcutsInhibitManagerV1::Private::removeInhibitor(Surface* surface, Seat* seat)
{
    m_inhibitors.remove({surface, seat});
}

KeyboardShortcutsInhibitManagerV1::KeyboardShortcutsInhibitManagerV1(Display* display)
    : d_ptr(new Private(display, this))
{
}

KeyboardShortcutsInhibitManagerV1::~KeyboardShortcutsInhibitManagerV1() = default;

KeyboardShortcutsInhibitorV1* KeyboardShortcutsInhibitManagerV1::findInhibitor(Surface* surface,
                                                                               Seat* seat) const
{
    return d_ptr->findInhibitor(surface, seat);
}

void KeyboardShortcutsInhibitManagerV1::removeInhibitor(Surface* surface, Seat* seat)
{
    d_ptr->removeInhibitor(surface, seat);
}

const struct zwp_keyboard_shortcuts_inhibitor_v1_interface
    KeyboardShortcutsInhibitorV1::Private::s_interface
    = {destroyCallback};

KeyboardShortcutsInhibitorV1::Private::Private(Client* client,
                                               uint32_t version,
                                               uint32_t id,
                                               Surface* surface,
                                               Seat* seat,
                                               KeyboardShortcutsInhibitorV1* q_ptr)
    : Wayland::Resource<KeyboardShortcutsInhibitorV1>(
          client,
          version,
          id,
          &zwp_keyboard_shortcuts_inhibitor_v1_interface,
          &s_interface,
          q_ptr)
    , m_surface(surface)
    , m_seat(seat)
{
}

KeyboardShortcutsInhibitorV1::KeyboardShortcutsInhibitorV1(Client* client,
                                                           uint32_t version,
                                                           uint32_t id,
                                                           Surface* surface,
                                                           Seat* seat)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, surface, seat, this))
{
}

KeyboardShortcutsInhibitorV1::~KeyboardShortcutsInhibitorV1() = default;

void KeyboardShortcutsInhibitorV1::setActive(bool active)
{
    if (d_ptr->m_active == active) {
        return;
    }
    d_ptr->m_active = active;
    if (active) {
        d_ptr->send<zwp_keyboard_shortcuts_inhibitor_v1_send_active>();
    } else {
        d_ptr->send<zwp_keyboard_shortcuts_inhibitor_v1_send_inactive>();
    }
}

bool KeyboardShortcutsInhibitorV1::isActive() const
{
    return d_ptr->m_active;
}

Seat* KeyboardShortcutsInhibitorV1::seat() const
{
    return d_ptr->m_seat;
}

Surface* KeyboardShortcutsInhibitorV1::surface() const
{
    return d_ptr->m_surface;
}

}
