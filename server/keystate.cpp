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
#include "keystate_p.h"

namespace Wrapland::Server
{

const struct org_kde_kwin_keystate_interface KeyState::Private::s_interface = {fetchStatesCallback};

KeyState::Private::Private(D_isplay* display, KeyState* qptr)
    : Wayland::Global<KeyState>(qptr, display, &org_kde_kwin_keystate_interface, &s_interface)
{
    create();
}

KeyState::Private::~Private() = default;

void KeyState::Private::fetchStatesCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource)
{
    auto handle = fromResource(wlResource);
    auto bind = handle->d_ptr->getBind(wlResource);

    for (uint32_t i = 0; i < handle->d_ptr->m_keyStates.size(); ++i) {
        handle->d_ptr->send<org_kde_kwin_keystate_send_stateChanged>(
            bind, i, handle->d_ptr->m_keyStates[i]);
    }
}

KeyState::KeyState(D_isplay* d, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(d, this))
{
}
KeyState::~KeyState() = default;

void KeyState::setState(KeyState::Key key, KeyState::State state)
{
    d_ptr->m_keyStates[int(key)] = state;
    d_ptr->send<org_kde_kwin_keystate_send_stateChanged>(int(key), int(state));
}

}
