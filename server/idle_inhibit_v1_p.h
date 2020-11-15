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

#include "display.h"
#include "idle_inhibit_v1.h"
#include "surface_p.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-idle-inhibit-unstable-v1-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t IdleInhibitManagerV1Version = 1;
using IdleInhibitManagerV1Global
    = Wayland::Global<IdleInhibitManagerV1, IdleInhibitManagerV1Version>;

class IdleInhibitManagerV1::Private : public IdleInhibitManagerV1Global
{
public:
    Private(Display* display, IdleInhibitManagerV1* q);
    ~Private() override;

private:
    static void createInhibitorCallback(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* surface);

    static const struct zwp_idle_inhibit_manager_v1_interface s_interface;
};

class WRAPLANDSERVER_EXPORT IdleInhibitor : public QObject
{
    Q_OBJECT
public:
    explicit IdleInhibitor(Client* client, uint32_t version, uint32_t id);
    friend class IdleInhibitManagerV1;
    ~IdleInhibitor() override;
    class Private;
    Private* d_ptr;
Q_SIGNALS:
    void resourceDestroyed();
};

class IdleInhibitor::Private : public Wayland::Resource<IdleInhibitor>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, IdleInhibitor* q);
    ~Private() override;

private:
    static const struct zwp_idle_inhibitor_v1_interface s_interface;
};

}
