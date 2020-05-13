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
#include "display.h"
#include "kde_idle_p.h"
#include "seat_p.h"

#include <QTimer>
#include <functional>

#include <wayland-idle-server-protocol.h>
#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_kwin_idle_interface KdeIdle::Private::s_interface = {getIdleTimeoutCallback};

KdeIdle::Private::Private(D_isplay* display, KdeIdle* qptr)
    : Wayland::Global<KdeIdle>(qptr, display, &org_kde_kwin_idle_interface, &s_interface)
{
    create();
}
KdeIdle::Private::~Private() = default;

void KdeIdle::Private::getIdleTimeoutCallback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              uint32_t id,
                                              wl_resource* wlSeat,
                                              uint32_t timeout)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);
    auto seat = SeatGlobal::handle(wlSeat);

    auto idleTimeout
        = new IdleTimeout(bind->client()->handle(), bind->version(), id, seat, priv->handle());
    if (!idleTimeout->d_ptr->resource()) {
        wl_resource_post_no_memory(wlResource);
        delete idleTimeout;
        return;
    }
    priv->idleTimeouts.push_back(idleTimeout);
    QObject::connect(
        idleTimeout, &IdleTimeout::resourceDestroyed, priv->handle(), [priv, idleTimeout]() {
            priv->idleTimeouts.erase(
                std::remove(priv->idleTimeouts.begin(), priv->idleTimeouts.end(), idleTimeout),
                priv->idleTimeouts.end());
        });

    idleTimeout->d_ptr->setup(timeout);
}

KdeIdle::KdeIdle(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

KdeIdle::~KdeIdle() = default;

void KdeIdle::inhibit()
{
    d_ptr->inhibitCount++;
    if (d_ptr->inhibitCount == 1) {
        Q_EMIT inhibitedChanged();
    }
}

void KdeIdle::uninhibit()
{
    d_ptr->inhibitCount--;
    if (d_ptr->inhibitCount == 0) {
        Q_EMIT inhibitedChanged();
    }
}

bool KdeIdle::isInhibited() const
{
    return d_ptr->inhibitCount > 0;
}

void KdeIdle::simulateUserActivity()
{
    for (auto i : d_ptr->idleTimeouts) {
        i->d_ptr->simulateUserActivity();
    }
}

const struct org_kde_kwin_idle_timeout_interface IdleTimeout::Private::s_interface = {
    destroyCallback,
    simulateUserActivityCallback,
};

IdleTimeout::Private::Private(Client* client,
                              uint32_t version,
                              uint32_t id,
                              Seat* seat,
                              KdeIdle* manager,
                              IdleTimeout* q)
    : Wayland::Resource<IdleTimeout>(client,
                                     version,
                                     id,
                                     &org_kde_kwin_idle_timeout_interface,
                                     &s_interface,
                                     q)
    , seat(seat)
    , manager(manager)
{
}

IdleTimeout::Private::~Private() = default;

void IdleTimeout::Private::simulateUserActivityCallback([[maybe_unused]] wl_client* wlClient,
                                                        wl_resource* wlResource)
{
    handle(wlResource)->d_ptr->simulateUserActivity();
}

void IdleTimeout::Private::simulateUserActivity()
{

    if (manager->isInhibited()) {
        // ignored while inhibited
        return;
    }
    if (!timer->isActive() && resource()) {
        send<org_kde_kwin_idle_timeout_send_resumed>();
    }
    timer->start();
}

void IdleTimeout::Private::setup(quint32 timeout)
{
    if (timer) {
        return;
    }
    auto q_ptr = handle();
    timer = new QTimer(q_ptr);
    timer->setSingleShot(true);
    // less than 5 sec is not idle by definition
    timer->setInterval(qMax(timeout, 5000u));
    QObject::connect(
        timer, &QTimer::timeout, q_ptr, [this] { send<org_kde_kwin_idle_timeout_send_idle>(); });
    if (manager->isInhibited()) {
        // don't start if inhibited
        return;
    }
    timer->start();
}

IdleTimeout::IdleTimeout(Client* client, uint32_t version, uint32_t id, Seat* seat, KdeIdle* parent)
    : QObject(parent)
    , d_ptr(new Private(client, version, id, seat, parent, this))
{
    connect(seat, &Seat::timestampChanged, this, [this] { d_ptr->simulateUserActivity(); });
    connect(parent, &KdeIdle::inhibitedChanged, this, [this] {
        if (!d_ptr->timer) {
            // not yet configured
            return;
        }
        if (d_ptr->manager->isInhibited()) {
            if (!d_ptr->timer->isActive() && d_ptr->resource()) {
                d_ptr->send<org_kde_kwin_idle_timeout_send_resumed>();
            }
            d_ptr->timer->stop();
        } else {
            d_ptr->timer->start();
        }
    });
}

IdleTimeout::~IdleTimeout() = default;

}
