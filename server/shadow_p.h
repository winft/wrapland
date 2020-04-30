/********************************************************************
Copyright 2020  Adrien Faveraux <ad1rie3@hotmail.fr>

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

#include "shadow.h"

#include <QMarginsF>
#include <QObject>

#include <wayland-shadow-server-protocol.h>

namespace Wrapland::Server
{

class Buffer;
class D_isplay;

class ShadowManager::Private : public Wayland::Global<ShadowManager>
{
public:
    Private(D_isplay* display, ShadowManager* qptr);
    ~Private() override;
    uint32_t version() const override;

private:
    void createShadow(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface);

    static void
    createCallback(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface);
    static void unsetCallback(wl_client* client, wl_resource* resource, wl_resource* surface);

    static const struct org_kde_kwin_shadow_manager_interface s_interface;
    static const uint32_t s_version;
};

class Shadow::Private : public Wayland::Resource<Shadow>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Shadow* q);
    ~Private() override;

    struct State {
        enum Flags {
            None = 0,
            LeftBuffer = 1 << 0,
            TopLeftBuffer = 1 << 1,
            TopBuffer = 1 << 2,
            TopRightBuffer = 1 << 3,
            RightBuffer = 1 << 4,
            BottomRightBuffer = 1 << 5,
            BottomBuffer = 1 << 6,
            BottomLeftBuffer = 1 << 7,
            Offset = 1 << 8
        };
        Buffer* left = nullptr;
        Buffer* topLeft = nullptr;
        Buffer* top = nullptr;
        Buffer* topRight = nullptr;
        Buffer* right = nullptr;
        Buffer* bottomRight = nullptr;
        Buffer* bottom = nullptr;
        Buffer* bottomLeft = nullptr;
        QMarginsF offset;
        Flags flags = Flags::None;
    };
    State current;
    State pending;

private:
    void commit();
    void attach(State::Flags flag, wl_resource* wlBuffer);

    static void commitCallback(wl_client* client, wl_resource* resource);
    static void attachLeftCallback(wl_client* client, wl_resource* resource, wl_resource* buffer);
    static void
    attachTopLeftCallback(wl_client* client, wl_resource* resource, wl_resource* buffer);
    static void attachTopCallback(wl_client* client, wl_resource* resource, wl_resource* buffer);
    static void
    attachTopRightCallback(wl_client* client, wl_resource* resource, wl_resource* buffer);
    static void attachRightCallback(wl_client* client, wl_resource* resource, wl_resource* buffer);
    static void
    attachBottomRightCallback(wl_client* client, wl_resource* resource, wl_resource* buffer);
    static void attachBottomCallback(wl_client* client, wl_resource* resource, wl_resource* buffer);
    static void
    attachBottomLeftCallback(wl_client* client, wl_resource* resource, wl_resource* buffer);
    static void offsetLeftCallback(wl_client* client, wl_resource* resource, wl_fixed_t offset);
    static void offsetTopCallback(wl_client* client, wl_resource* resource, wl_fixed_t offset);
    static void offsetRightCallback(wl_client* client, wl_resource* resource, wl_fixed_t offset);
    static void offsetBottomCallback(wl_client* client, wl_resource* resource, wl_fixed_t offset);

    static const struct org_kde_kwin_shadow_interface s_interface;
};

}
