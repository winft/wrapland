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

#pragma once
#include "keyboard_shortcuts_inhibit.h"
#include "seat.h"
#include "surface.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include "wayland-keyboard-shortcuts-inhibit-server-protocol.h"

namespace Wrapland::Server
{

class KeyboardShortcutsInhibitManagerV1::Private
    : public Wayland::Global<KeyboardShortcutsInhibitManagerV1>
{
public:
    Private(Display* display, KeyboardShortcutsInhibitManagerV1* q);

    static void inhibitShortcutsCallback(wl_client* client,
                                         wl_resource* wlResource,
                                         uint32_t id,
                                         wl_resource* wlSurface,
                                         wl_resource* wlSeat);

    KeyboardShortcutsInhibitorV1* findInhibitor(Surface* surface, Seat* seat) const;
    void removeInhibitor(Surface* surface, Seat* seat);

    QHash<QPair<Surface*, Seat*>, KeyboardShortcutsInhibitorV1*> m_inhibitors;

private:
    static const struct zwp_keyboard_shortcuts_inhibit_manager_v1_interface s_interface;
};

class KeyboardShortcutsInhibitorV1::Private : public Wayland::Resource<KeyboardShortcutsInhibitorV1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Surface* surface,
            Seat* seat,
            KeyboardShortcutsInhibitorV1* q);

    Surface* m_surface;
    Seat* m_seat;
    bool m_active;

private:
    static const struct zwp_keyboard_shortcuts_inhibitor_v1_interface s_interface;
};

}
