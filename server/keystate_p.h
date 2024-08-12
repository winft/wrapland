/********************************************************************
Copyright 2020 Adrien Faveraux <ad1rie3@hotmail.fr>

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

#pragma once

#include "display.h"
#include "keystate.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <array>
#include <memory>
#include <wayland-keystate-server-protocol.h>

namespace Wrapland::Server
{

class KeyState;

constexpr uint32_t KeyStateVersion = 1;
using KeyStateGlobal = Wayland::Global<KeyState, KeyStateVersion>;

class KeyState::Private : public KeyStateGlobal
{
public:
    Private(Display* display, KeyState* q_ptr);

    static void fetchStatesCallback(KeyStateGlobal::bind_t* bind);

    std::array<State, 3> key_states{Unlocked};
    static const struct org_kde_kwin_keystate_interface s_interface;
};
}
