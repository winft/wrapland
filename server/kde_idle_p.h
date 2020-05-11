/********************************************************************
Copyright 2020 Faveraux Adrien <ad1rie3@hotmail.fr>

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

#include "kde_idle.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QTimer>
#include <vector>

#include <wayland-idle-server-protocol.h>

namespace Wrapland
{
namespace Server
{

class D_isplay;
class Seat;
class IdleTimeout;

class KdeIdle::Private : public Wayland::Global<KdeIdle>
{
public:
    Private(D_isplay* d, KdeIdle* q);
    ~Private();
    int inhibitCount = 0;
    std::vector<IdleTimeout*> idleTimeouts;

private:
    static void getIdleTimeoutCallback(wl_client* wlClient,
                                       wl_resource* wlResource,
                                       uint32_t id,
                                       wl_resource* wlSeat,
                                       uint32_t timeout);

    static const struct org_kde_kwin_idle_interface s_interface;
};

class WRAPLANDSERVER_EXPORT IdleTimeout : public QObject
{
    Q_OBJECT
public:
    ~IdleTimeout() override;

Q_SIGNALS:
    void resourceDestroyed();

private:
    explicit IdleTimeout(Wayland::Client* client,
                         uint32_t version,
                         uint32_t id,
                         Seat* seat,
                         KdeIdle* parent);
    friend class KdeIdle;
    class Private;
    Private* d_ptr;
};

class IdleTimeout::Private : public Wayland::Resource<IdleTimeout>
{
public:
    Private(Wayland::Client* client,
            uint32_t version,
            uint32_t id,
            Seat* seat,
            KdeIdle* manager,
            IdleTimeout* q);

    ~Private();
    void setup(quint32 timeout);

    void simulateUserActivity();

    Seat* seat;
    KdeIdle* manager;
    QTimer* timer = nullptr;

private:
    static void simulateUserActivityCallback(wl_client* client, wl_resource* resource);
    static const struct org_kde_kwin_idle_timeout_interface s_interface;
};

}
}
