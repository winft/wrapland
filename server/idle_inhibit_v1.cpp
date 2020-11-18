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
#include "idle_inhibit_v1_p.h"

namespace Wrapland::Server
{

const struct zwp_idle_inhibit_manager_v1_interface IdleInhibitManagerV1::Private::s_interface = {
    resourceDestroyCallback,
    cb<createInhibitorCallback>,
};

IdleInhibitManagerV1::Private::Private(Display* display, IdleInhibitManagerV1* q)
    : Wayland::Global<IdleInhibitManagerV1>(q,
                                            display,
                                            &zwp_idle_inhibit_manager_v1_interface,
                                            &s_interface)
{
    create();
}
IdleInhibitManagerV1::Private::~Private() = default;

void IdleInhibitManagerV1::Private::createInhibitorCallback(IdleInhibitManagerV1Bind* bind,
                                                            uint32_t id,
                                                            wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);
    auto inhibitor = new IdleInhibitor(bind->client()->handle(), bind->version(), id);

    surface->d_ptr->installIdleInhibitor(inhibitor);
}

IdleInhibitManagerV1::IdleInhibitManagerV1(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))

{
}

IdleInhibitManagerV1::~IdleInhibitManagerV1() = default;

const struct zwp_idle_inhibitor_v1_interface IdleInhibitor::Private::s_interface
    = {destroyCallback};

IdleInhibitor::Private::Private(Client* client, uint32_t version, uint32_t id, IdleInhibitor* q)
    : Wayland::Resource<IdleInhibitor>(client,
                                       version,
                                       id,
                                       &zwp_idle_inhibitor_v1_interface,
                                       &s_interface,
                                       q)
{
}

IdleInhibitor::Private::~Private() = default;

IdleInhibitor::IdleInhibitor(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}
IdleInhibitor::~IdleInhibitor() = default;
}
