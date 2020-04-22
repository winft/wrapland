/****************************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

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
#include "pointer_constraints_v1.h"
#include "pointer_constraints_v1_p.h"

#include "display.h"
#include "pointer_p.h"
#include "region.h"
#include "surface.h"
#include "surface_p.h"

#include "wayland/client.h"
#include "wayland/display.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <functional>

#include <wayland-pointer-constraints-unstable-v1-server-protocol.h>

namespace Wrapland
{
namespace Server
{

PointerConstraintsV1::Private::Private(PointerConstraintsV1* q, D_isplay* display)
    : Wayland::Global<PointerConstraintsV1>(q,
                                            display,
                                            &zwp_pointer_constraints_v1_interface,
                                            &s_interface)
    , q_ptr(q)
{
}

const struct zwp_pointer_constraints_v1_interface PointerConstraintsV1::Private::s_interface = {
    resourceDestroyCallback,
    lockPointerCallback,
    confinePointerCallback,
};

void PointerConstraintsV1::Private::lockPointerCallback(wl_client* wlClient,
                                                        wl_resource* wlResource,
                                                        uint32_t id,
                                                        wl_resource* wlSurface,
                                                        wl_resource* wlPointer,
                                                        wl_resource* wlRegion,
                                                        uint32_t lifetime)
{
    auto handle = fromResource(wlResource);
    handle->d_ptr->createConstraint<LockedPointerV1>(
        wlClient, id, wlSurface, wlPointer, wlRegion, lifetime);
}

void PointerConstraintsV1::Private::confinePointerCallback(wl_client* wlClient,
                                                           wl_resource* wlResource,
                                                           uint32_t id,
                                                           wl_resource* wlSurface,
                                                           wl_resource* wlPointer,
                                                           wl_resource* wlRegion,
                                                           uint32_t lifetime)
{
    auto handle = fromResource(wlResource);
    handle->d_ptr->createConstraint<ConfinedPointerV1>(
        wlClient, id, wlSurface, wlPointer, wlRegion, lifetime);
}

template<class Constraint>
void PointerConstraintsV1::Private::createConstraint(wl_client* wlClient,
                                                     uint32_t id,
                                                     wl_resource* wlSurface,
                                                     wl_resource* wlPointer,
                                                     wl_resource* wlRegion,
                                                     uint32_t lifetime)
{
    auto client = display()->handle()->getClient(wlClient);
    auto surface = Wayland::Resource<Surface>::fromResource(wlSurface)->handle();
    auto pointer = Wayland::Resource<Pointer>::fromResource(wlPointer)->handle();

    if (!surface || !pointer) {
        // send error?
        return;
    }
    if (!surface->lockedPointer().isNull() || !surface->confinedPointer().isNull()) {
        surface->d_ptr->postError(ZWP_POINTER_CONSTRAINTS_V1_ERROR_ALREADY_CONSTRAINED,
                                  "Surface already constrained");
        return;
    }

    auto constraint = new Constraint(client, version(), id, handle());

    switch (lifetime) {
    case ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT:
        constraint->d_ptr->lifeTime = Constraint::LifeTime::Persistent;
        break;
    case ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT:
    default:
        constraint->d_ptr->lifeTime = Constraint::LifeTime::OneShot;
        break;
    }

    auto region = wlRegion ? Wayland::Resource<Region>::fromResource(wlRegion)->handle() : nullptr;
    constraint->d_ptr->region = region ? region->region() : QRegion();

    surface->d_ptr->installPointerConstraint(constraint);
}

PointerConstraintsV1::PointerConstraintsV1(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

PointerConstraintsV1::~PointerConstraintsV1() = default;

LockedPointerV1::Private::Private(Client* client, uint32_t version, uint32_t id, LockedPointerV1* q)
    : Wayland::Resource<LockedPointerV1>(client,
                                         version,
                                         id,
                                         &zwp_locked_pointer_v1_interface,
                                         &s_interface,
                                         q)
    , q_ptr(q)
{
}

LockedPointerV1::Private::~Private() = default;

const struct zwp_locked_pointer_v1_interface LockedPointerV1::Private::s_interface = {
    destroyCallback,
    setCursorPositionHintCallback,
    setRegionCallback,
};

void LockedPointerV1::Private::setCursorPositionHintCallback([[maybe_unused]] wl_client* client,
                                                             wl_resource* wlResource,
                                                             wl_fixed_t surface_x,
                                                             wl_fixed_t surface_y)
{
    auto priv = fromResource(wlResource)->handle()->d_ptr;

    priv->pendingHint = QPointF(wl_fixed_to_double(surface_x), wl_fixed_to_double(surface_y));
    priv->hintIsSet = true;
}

void LockedPointerV1::Private::setRegionCallback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 wl_resource* wlRegion)
{
    auto priv = fromResource(wlResource)->handle()->d_ptr;
    auto region = wlRegion ? Wayland::Resource<Region>::fromResource(wlRegion)->handle() : nullptr;

    priv->pendingRegion = region ? region->region() : QRegion();
    priv->regionIsSet = true;
}

void LockedPointerV1::Private::commit()
{
    if (regionIsSet) {
        region = pendingRegion;
        pendingRegion = QRegion();
        regionIsSet = false;
        Q_EMIT q_ptr->regionChanged();
    }
    if (hintIsSet) {
        hint = pendingHint;
        hintIsSet = false;
        Q_EMIT q_ptr->cursorPositionHintChanged();
    }
}

void LockedPointerV1::Private::update()
{
    if (locked) {
        send<zwp_locked_pointer_v1_send_locked>();
    } else {
        send<zwp_locked_pointer_v1_send_unlocked>();
    }
}

LockedPointerV1::LockedPointerV1(Client* client,
                                 uint32_t version,
                                 uint32_t id,
                                 PointerConstraintsV1* constraints)
    : QObject(constraints)
    , d_ptr(new Private(client, version, id, this))
{
    connect(this, &LockedPointerV1::resourceDestroyed, this, [this]() { setLocked(false); });
}

LockedPointerV1::~LockedPointerV1() = default;

LockedPointerV1::LifeTime LockedPointerV1::lifeTime() const
{
    return d_ptr->lifeTime;
}

QRegion LockedPointerV1::region() const
{
    return d_ptr->region;
}

QPointF LockedPointerV1::cursorPositionHint() const
{
    return d_ptr->hint;
}

bool LockedPointerV1::isLocked() const
{
    return d_ptr->locked;
}

void LockedPointerV1::setLocked(bool locked)
{
    if (locked == d_ptr->locked) {
        return;
    }
    if (!locked) {
        d_ptr->hint = QPointF(-1., -1.);
    }
    d_ptr->locked = locked;
    d_ptr->update();
    Q_EMIT lockedChanged();
}

ConfinedPointerV1::Private::Private(Client* client,
                                    uint32_t version,
                                    uint32_t id,
                                    ConfinedPointerV1* q)
    : Wayland::Resource<ConfinedPointerV1>(client,
                                           version,
                                           id,
                                           &zwp_confined_pointer_v1_interface,
                                           &s_interface,
                                           q)
    , q_ptr(q)
{
}

ConfinedPointerV1::Private::~Private() = default;

const struct zwp_confined_pointer_v1_interface ConfinedPointerV1::Private::s_interface = {
    destroyCallback,
    setRegionCallback,
};

void ConfinedPointerV1::Private::setRegionCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource,
                                                   wl_resource* wlRegion)
{
    auto priv = fromResource(wlResource);
    auto region = wlRegion ? Wayland::Resource<Region>::fromResource(wlRegion)->handle() : nullptr;

    priv->handle()->d_ptr->pendingRegion = region ? region->region() : QRegion();
    priv->handle()->d_ptr->regionIsSet = true;
}

void ConfinedPointerV1::Private::update()
{
    if (confined) {
        send<zwp_confined_pointer_v1_send_confined>();
    } else {
        send<zwp_confined_pointer_v1_send_unconfined>();
    }
}

void ConfinedPointerV1::Private::commit()
{
    if (!regionIsSet) {
        return;
    }

    region = pendingRegion;
    pendingRegion = QRegion();
    regionIsSet = false;

    Q_EMIT q_ptr->regionChanged();
}

ConfinedPointerV1::ConfinedPointerV1(Client* client,
                                     uint32_t version,
                                     uint32_t id,
                                     PointerConstraintsV1* constraints)
    : QObject(constraints)
    , d_ptr(new Private(client, version, id, this))
{
    connect(this, &ConfinedPointerV1::resourceDestroyed, this, [this]() { setConfined(false); });
}

ConfinedPointerV1::~ConfinedPointerV1() = default;

ConfinedPointerV1::LifeTime ConfinedPointerV1::lifeTime() const
{
    return d_ptr->lifeTime;
}

QRegion ConfinedPointerV1::region() const
{
    return d_ptr->region;
}

bool ConfinedPointerV1::isConfined() const
{
    return d_ptr->confined;
}

void ConfinedPointerV1::setConfined(bool confined)
{
    if (confined == d_ptr->confined) {
        return;
    }
    d_ptr->confined = confined;
    d_ptr->update();
    Q_EMIT confinedChanged();
}

}
}
