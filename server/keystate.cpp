/********************************************************************
Copyright 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

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


namespace Wrapland
{
namespace Server
{


const struct org_kde_kwin_keystate_interface KeyState::Private::s_interface = {
    fetchStatesCallback
};

KeyState::Private::Private(D_isplay* display, KeyState* q)
    : Wayland::Global<KeyState>(q,display, &org_kde_kwin_keystate_interface, &s_interface)
{
    create();
}
   
void KeyState::Private::bind(Wayland::Client * client, uint32_t version, uint32_t id) override {
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_keystate_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    m_resources << resource;
}

void KeyState::Private::unbind(Wayland::Resource *resource) {
    auto *d = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    d->m_resources.removeAll(resource);
}


void KeyState::Private::fetchStatesCallback(struct Wayland::Client */*client*/, struct Wayland::Resource *resource) {
    auto s = reinterpret_cast<KeyStateInterface::Private*>(wl_resource_get_user_data(resource));

    for (int i=0; i<s->m_keyStates.count(); ++i)
        org_kde_kwin_keystate_send_stateChanged(resource, i, s->m_keyStates[i]);
}


KeyState::KeyState(D_isplay* d, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(d,this))
{}

KeyState::~KeyState() = default;



void KeyState::setState(KeyState::Key key, KeyState::State state)
{
    auto dptr = static_cast<KeyState::Private*>(d_ptr.get());
    dptr->m_keyStates[int(key)] = state;

    for(auto r : qAsConst(dptr->m_resources)) {
        org_kde_kwin_keystate_send_stateChanged(r, int(key), int(state));
    }
}

}

}
