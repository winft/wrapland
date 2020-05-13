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

#include "buffer.h"
#include "display.h"
#include "surface.h"
#include "surface_p.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include "shadow_p.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_kwin_shadow_manager_interface ShadowManager::Private::s_interface = {
    createCallback,
    unsetCallback,
    resourceDestroyCallback,
};

ShadowManager::Private::Private(D_isplay* display, ShadowManager* qptr)
    : ShadowManagerGlobal(qptr, display, &org_kde_kwin_shadow_manager_interface, &s_interface)
{
    create();
}

ShadowManager::Private::~Private() = default;

void ShadowManager::Private::createCallback(wl_client* wlClient,
                                            wl_resource* wlResource,
                                            uint32_t id,
                                            wl_resource* wlSurface)
{

    handle(wlResource)->d_ptr->createShadow(wlClient, wlResource, id, wlSurface);
}

void ShadowManager::Private::createShadow(wl_client* wlClient,
                                          wl_resource* wlResource,
                                          uint32_t id,
                                          wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);
    auto client = display()->handle()->getClient(wlClient);
    auto shadow = new Shadow(client, wl_resource_get_version(wlResource), id);

    if (!shadow->d_ptr->resource()) {
        wl_resource_post_no_memory(wlResource);
        delete shadow;
        return;
    }
    surface->d_ptr->setShadow(QPointer<Shadow>(shadow));
}

void ShadowManager::Private::unsetCallback([[maybe_unused]] wl_client* wlClient,
                                           [[maybe_unused]] wl_resource* wlResource,
                                           wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);
    surface->d_ptr->setShadow(QPointer<Shadow>());
}

ShadowManager::ShadowManager(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

ShadowManager::~ShadowManager() = default;

const struct org_kde_kwin_shadow_interface Shadow::Private::s_interface = {
    commitCallback,
    attachLeftCallback,
    attachTopLeftCallback,
    attachTopCallback,
    attachTopRightCallback,
    attachRightCallback,
    attachBottomRightCallback,
    attachBottomCallback,
    attachBottomLeftCallback,
    offsetLeftCallback,
    offsetTopCallback,
    offsetRightCallback,
    offsetBottomCallback,
    destroyCallback,
};

void Shadow::Private::commitCallback([[maybe_unused]] wl_client* wlClient, wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->commit();
}

void Shadow::Private::commit()
{
#define BUFFER(__FLAG__, __PART__)                                                                 \
    if (pending.flags & State::Flags::__FLAG__##Buffer) {                                          \
        if (current.__PART__) {                                                                    \
            current.__PART__->unref();                                                             \
        }                                                                                          \
        if (pending.__PART__) {                                                                    \
            pending.__PART__->ref();                                                               \
        }                                                                                          \
        current.__PART__ = pending.__PART__;                                                       \
    }
    BUFFER(Left, left)
    BUFFER(TopLeft, topLeft)
    BUFFER(Top, top)
    BUFFER(TopRight, topRight)
    BUFFER(Right, right)
    BUFFER(BottomRight, bottomRight)
    BUFFER(Bottom, bottom)
    BUFFER(BottomLeft, bottomLeft)
#undef BUFFER

    if (pending.flags & State::Offset) {
        current.offset = pending.offset;
    }
    pending = State();
}

void Shadow::Private::attach(Shadow::Private::State::Flags flag, wl_resource* wlBuffer)
{
    auto display = client()->display()->handle();
    auto buffer = Buffer::get(display, wlBuffer);
    if (buffer) {
        QObject::connect(buffer, &Buffer::resourceDestroyed, handle(), [this, buffer]() {
#define PENDING(__PART__)                                                                          \
    if (pending.__PART__ == buffer) {                                                              \
        pending.__PART__ = nullptr;                                                                \
    }
            PENDING(left)
            PENDING(topLeft)
            PENDING(top)
            PENDING(topRight)
            PENDING(right)
            PENDING(bottomRight)
            PENDING(bottom)
            PENDING(bottomLeft)
#undef PENDING

#define CURRENT(__PART__)                                                                          \
    if (current.__PART__ == buffer) {                                                              \
        current.__PART__->unref();                                                                 \
        current.__PART__ = nullptr;                                                                \
    }
            CURRENT(left)
            CURRENT(topLeft)
            CURRENT(top)
            CURRENT(topRight)
            CURRENT(right)
            CURRENT(bottomRight)
            CURRENT(bottom)
            CURRENT(bottomLeft)
#undef CURRENT
        });
    }
    switch (flag) {
    case State::LeftBuffer:
        pending.left = buffer;
        break;
    case State::TopLeftBuffer:
        pending.topLeft = buffer;
        break;
    case State::TopBuffer:
        pending.top = buffer;
        break;
    case State::TopRightBuffer:
        pending.topRight = buffer;
        break;
    case State::RightBuffer:
        pending.right = buffer;
        break;
    case State::BottomRightBuffer:
        pending.bottomRight = buffer;
        break;
    case State::BottomBuffer:
        pending.bottom = buffer;
        break;
    case State::BottomLeftBuffer:
        pending.bottomLeft = buffer;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    pending.flags = State::Flags(pending.flags | flag);
}

#define ATTACH(__PART__)                                                                           \
    void Shadow::Private::attach##__PART__##Callback(                                              \
        [[maybe_unused]] wl_client* wlClient, wl_resource* wlResource, wl_resource* wlBuffer)      \
    {                                                                                              \
        auto priv = handle(wlResource)->d_ptr;                                                     \
        priv->attach(State::__PART__##Buffer, wlBuffer);                                           \
    }

ATTACH(Left)
ATTACH(TopLeft)
ATTACH(Top)
ATTACH(TopRight)
ATTACH(Right)
ATTACH(BottomRight)
ATTACH(Bottom)
ATTACH(BottomLeft)

#undef ATTACH

#define OFFSET(__PART__)                                                                           \
    void Shadow::Private::offset##__PART__##Callback([[maybe_unused]] wl_client* wlClient,         \
                                                     [[maybe_unused]] wl_resource* wlResource,     \
                                                     wl_fixed_t offset)                            \
    {                                                                                              \
        auto priv = handle(wlResource)->d_ptr;                                                     \
        priv->pending.flags = State::Flags(priv->pending.flags | State::Offset);                   \
        priv->pending.offset.set##__PART__(wl_fixed_to_double(offset));                            \
    }

OFFSET(Left)
OFFSET(Top)
OFFSET(Right)
OFFSET(Bottom)

#undef OFFSET

Shadow::Private::Private(Client* client, uint32_t version, uint32_t id, Shadow* q)
    : Wayland::Resource<Shadow>(client,
                                version,
                                id,
                                &org_kde_kwin_shadow_interface,
                                &s_interface,
                                q)
{
}

Shadow::Private::~Private()
{

#define CURRENT(__PART__)                                                                          \
    if (current.__PART__) {                                                                        \
        current.__PART__->unref();                                                                 \
    }
    CURRENT(left)
    CURRENT(topLeft)
    CURRENT(top)
    CURRENT(topRight)
    CURRENT(right)
    CURRENT(bottomRight)
    CURRENT(bottom)
    CURRENT(bottomLeft)
#undef CURRENT
}

Shadow::Shadow(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Shadow::Private(client, version, id, this))
{
}

Shadow::~Shadow() = default;

QMarginsF Shadow::offset() const
{
    return d_ptr->current.offset;
}

#define BUFFER(__PART__)                                                                           \
    Buffer* Shadow::__PART__() const                                                               \
    {                                                                                              \
        return d_ptr->current.__PART__;                                                            \
    }

BUFFER(left)
BUFFER(topLeft)
BUFFER(top)
BUFFER(topRight)
BUFFER(right)
BUFFER(bottomRight)
BUFFER(bottom)
BUFFER(bottomLeft)

}
