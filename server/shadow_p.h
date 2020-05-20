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

#include <cassert>
#include <type_traits>
#include <wayland-shadow-server-protocol.h>

#include <wayland-server.h>

namespace Wrapland::Server
{

class Buffer;
class Display;

constexpr uint32_t ShadowManagerVersion = 2;
using ShadowManagerGlobal = Wayland::Global<ShadowManager, ShadowManagerVersion>;

class ShadowManager::Private : public ShadowManagerGlobal
{
public:
    Private(Display* display, ShadowManager* qptr);

private:
    void createShadow(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface);

    static void
    createCallback(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface);
    static void unsetCallback(wl_client* client, wl_resource* resource, wl_resource* surface);

    static const struct org_kde_kwin_shadow_manager_interface s_interface;
};

class Shadow::Private : public Wayland::Resource<Shadow>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Shadow* q);
    ~Private() override;

    enum class AttachSide {
        Left,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
    };

    enum class OffsetSide {
        Left,
        Top,
        Right,
        Bottom,
    };

    struct State {
        template<AttachSide side>
        Buffer*& get()
        {
            if constexpr (side == AttachSide::Left) {
                return left;
            } else if constexpr (side == AttachSide::TopLeft) {
                return topLeft;
            } else if constexpr (side == AttachSide::Top) {
                return top;
            } else if constexpr (side == AttachSide::TopRight) {
                return topRight;
            } else if constexpr (side == AttachSide::Right) {
                return right;
            } else if constexpr (side == AttachSide::BottomRight) {
                return bottomRight;
            } else if constexpr (side == AttachSide::Bottom) {
                return bottom;
            } else {
                static_assert(side == AttachSide::BottomLeft);
                return bottomLeft;
            }
        }

        // We need this for our QObject connections. Once we use a signal system with good template
        // support it can be replaced by above templated getter.
        Buffer*& get(AttachSide side)
        {
            if (side == AttachSide::Left) {
                return left;
            } else if (side == AttachSide::TopLeft) {
                return topLeft;
            } else if (side == AttachSide::Top) {
                return top;
            } else if (side == AttachSide::TopRight) {
                return topRight;
            } else if (side == AttachSide::Right) {
                return right;
            } else if (side == AttachSide::BottomRight) {
                return bottomRight;
            } else if (side == AttachSide::Bottom) {
                return bottom;
            } else {
                assert(side == AttachSide::BottomLeft);
                return bottomLeft;
            }
        }

        template<OffsetSide side>
        void setOffset(double _offset)
        {
            if constexpr (side == OffsetSide::Left) {
                offset.setLeft(_offset);
            } else if constexpr (side == OffsetSide::Top) {
                offset.setTop(_offset);
            } else if constexpr (side == OffsetSide::Right) {
                offset.setRight(_offset);
            } else {
                static_assert(side == OffsetSide::Bottom);
                offset.setBottom(_offset);
            }
            offsetIsSet = true;
        }

        template<AttachSide side>
        void commit(State& pending)
        {
            auto& currentBuf = get<side>();
            auto& pendingBuf = pending.get<side>();

            if (currentBuf) {
                currentBuf->unref();
            }
            if (pendingBuf) {
                pendingBuf->ref();
            }
            currentBuf = pendingBuf;
            pendingBuf = nullptr;
        }

        template<AttachSide side>
        void unref()
        {
            if (auto buf = get<side>()) {
                buf->unref();
            }
        }

        // TODO: unique_ptr?
        Buffer* left = nullptr;
        Buffer* topLeft = nullptr;
        Buffer* top = nullptr;
        Buffer* topRight = nullptr;
        Buffer* right = nullptr;
        Buffer* bottomRight = nullptr;
        Buffer* bottom = nullptr;
        Buffer* bottomLeft = nullptr;

        QMarginsF offset;
        bool offsetIsSet = false;
    };
    State current;
    State pending;

private:
    template<AttachSide side>
    static void attachCallback([[maybe_unused]] wl_client* wlClient,
                               wl_resource* wlResource,
                               wl_resource* wlBuffer)
    {
        auto priv = handle(wlResource)->d_ptr;
        priv->attach<side>(wlBuffer);
    }

    template<OffsetSide side>
    static void offsetCallback([[maybe_unused]] wl_client* wlClient,
                               wl_resource* wlResource,
                               wl_fixed_t wlOffset)
    {
        auto priv = handle(wlResource)->d_ptr;
        priv->pending.setOffset<side>(wl_fixed_to_double(wlOffset));
    }

    static void commitCallback(wl_client* client, wl_resource* resource);

    template<AttachSide side>
    void attach(wl_resource* wlBuffer)
    {
        auto display = client()->display()->handle();
        auto buffer = Buffer::get(display, wlBuffer);

        attachConnect(side, buffer);
        pending.get<side>() = buffer;
    }

    void attachConnect(AttachSide side, Buffer* buffer);
    void commit();

    static const struct org_kde_kwin_shadow_interface s_interface;
};

}
