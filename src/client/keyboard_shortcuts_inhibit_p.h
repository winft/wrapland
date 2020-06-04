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

#include <QObject>
// STD
#include <memory>

#include "event_queue.h"
#include "keyboard_shortcuts_inhibit.h"
#include "seat.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <wayland-keyboard-shortcuts-inhibit-client-protocol.h>

struct zwp_keyboard_shortcuts_inhibit_manager_v1;
struct zwp_keyboard_shortcuts_inhibitor_v1;

namespace Wrapland::Client
{

class Q_DECL_HIDDEN KeyboardShortcutsInhibitManagerV1::Private
{
public:
    Private() = default;

    void setup(zwp_keyboard_shortcuts_inhibit_manager_v1* arg);

    WaylandPointer<zwp_keyboard_shortcuts_inhibit_manager_v1,
                   zwp_keyboard_shortcuts_inhibit_manager_v1_destroy>
        idleinhibitmanager;
    EventQueue* queue = nullptr;
};

class Q_DECL_HIDDEN KeyboardShortcutsInhibitorV1::Private
{
public:
    Private(KeyboardShortcutsInhibitorV1* q);

    void setup(zwp_keyboard_shortcuts_inhibitor_v1* arg);
    static void
    ActiveCallback(void* data,
                   zwp_keyboard_shortcuts_inhibitor_v1* zwp_keyboard_shortcuts_inhibitor_v1);
    static void
    InactiveCallback(void* data,
                     zwp_keyboard_shortcuts_inhibitor_v1* zwp_keyboard_shortcuts_inhibitor_v1);

    WaylandPointer<zwp_keyboard_shortcuts_inhibitor_v1, zwp_keyboard_shortcuts_inhibitor_v1_destroy>
        idleinhibitor;

    bool inhibitor_active;

private:
    KeyboardShortcutsInhibitorV1* q;
    static const struct zwp_keyboard_shortcuts_inhibitor_v1_listener s_listener;
};

}
