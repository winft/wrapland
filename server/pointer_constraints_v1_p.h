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
#pragma once

#include "pointer_constraints_v1.h"

#include "wayland/global.h"

#include <QRegion>

#include <wayland-pointer-constraints-unstable-v1-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t PointerConstraintsV1Version = 1;
using PointerConstraintsV1Global
    = Wayland::Global<PointerConstraintsV1, PointerConstraintsV1Version>;

class PointerConstraintsV1::Private : public PointerConstraintsV1Global
{
public:
    Private(PointerConstraintsV1* q, Display* display);

    static void destroyCallback(wl_client* client, wl_resource* resource);
    static void lockPointerCallback(wl_client* wlClient,
                                    wl_resource* wlResource,
                                    uint32_t id,
                                    wl_resource* wlSurface,
                                    wl_resource* wlPointer,
                                    wl_resource* wlRegion,
                                    uint32_t lifetime);
    static void confinePointerCallback(wl_client* wlClient,
                                       wl_resource* wlResource,
                                       uint32_t id,
                                       wl_resource* wlSurface,
                                       wl_resource* wlPointer,
                                       wl_resource* wlRegion,
                                       uint32_t lifetime);

    template<class Constraint>
    void createConstraint(wl_resource* wlResource,
                          uint32_t id,
                          wl_resource* wlSurface,
                          wl_resource* wlPointer,
                          wl_resource* wlRegion,
                          uint32_t lifetime);

private:
    static const struct zwp_pointer_constraints_v1_interface s_interface;

    PointerConstraintsV1* q_ptr;
};

class LockedPointerV1::Private : public Wayland::Resource<LockedPointerV1>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, LockedPointerV1* q);

    void update();
    void commit();

    LifeTime lifeTime;
    QRegion region;
    bool locked = false;
    QPointF hint = QPointF(-1., -1.);

    QRegion pendingRegion;
    bool regionIsSet = false;

    QPointF pendingHint;
    bool hintIsSet = false;

private:
    static const struct zwp_locked_pointer_v1_interface s_interface;
    static void setCursorPositionHintCallback(wl_client* wlClient,
                                              wl_resource* wlResource,
                                              wl_fixed_t surface_x,
                                              wl_fixed_t surface_y);
    static void
    setRegionCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlRegion);

    LockedPointerV1* q_ptr;
};

class ConfinedPointerV1::Private : public Wayland::Resource<ConfinedPointerV1>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, ConfinedPointerV1* q);

    void update();
    void commit();

    LifeTime lifeTime;
    QRegion region;

    bool confined = false;

    QRegion pendingRegion;
    bool regionIsSet = false;

private:
    static const struct zwp_confined_pointer_v1_interface s_interface;
    static void
    setRegionCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlRegion);

    ConfinedPointerV1* q_ptr;
};

}
