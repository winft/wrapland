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

namespace Wrapland::Server
{

const struct org_kde_kwin_shadow_manager_interface ShadowManager::Private::s_interface = {
    cb<createCallback>,
    cb<unsetCallback>,
    resourceDestroyCallback,
};

ShadowManager::Private::Private(Display* display, ShadowManager* q_ptr)
    : ShadowManagerGlobal(q_ptr, display, &org_kde_kwin_shadow_manager_interface, &s_interface)
{
    create();
}

void ShadowManager::Private::createCallback(ShadowManagerBind* bind,
                                            uint32_t id,
                                            wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::get_handle(wlSurface);

    auto shadow = new Shadow(bind->client->handle, bind->version, id);
    if (!shadow->d_ptr->resource) {
        bind->post_no_memory();
        delete shadow;
        return;
    }
    surface->d_ptr->setShadow(shadow);
}

void ShadowManager::Private::unsetCallback([[maybe_unused]] ShadowManagerBind* bind,
                                           wl_resource* wlSurface)
{
    auto surface = Wayland::Resource<Surface>::get_handle(wlSurface);
    surface->d_ptr->setShadow(nullptr);
}

ShadowManager::ShadowManager(Display* display)
    : d_ptr(new Private(display, this))
{
}

ShadowManager::~ShadowManager() = default;

const struct org_kde_kwin_shadow_interface Shadow::Private::s_interface = {
    commitCallback,
    attachCallback<AttachSide::Left>,
    attachCallback<AttachSide::TopLeft>,
    attachCallback<AttachSide::Top>,
    attachCallback<AttachSide::TopRight>,
    attachCallback<AttachSide::Right>,
    attachCallback<AttachSide::BottomRight>,
    attachCallback<AttachSide::Bottom>,
    attachCallback<AttachSide::BottomLeft>,
    offsetCallback<OffsetSide::Left>,
    offsetCallback<OffsetSide::Top>,
    offsetCallback<OffsetSide::Right>,
    offsetCallback<OffsetSide::Bottom>,
    destroyCallback,
};

void Shadow::Private::commitCallback([[maybe_unused]] wl_client* wlClient, wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->commit();
}

void Shadow::Private::commit()
{
    current.commit<AttachSide::Left>(pending);
    current.commit<AttachSide::TopLeft>(pending);
    current.commit<AttachSide::Top>(pending);
    current.commit<AttachSide::TopRight>(pending);
    current.commit<AttachSide::Right>(pending);
    current.commit<AttachSide::BottomRight>(pending);
    current.commit<AttachSide::Bottom>(pending);
    current.commit<AttachSide::BottomLeft>(pending);

    if (pending.offsetIsSet) {
        current.offset = pending.offset;
    }
    pending = State();
}

void Shadow::Private::attachConnect(AttachSide side, Buffer* buffer)
{
    if (!buffer) {
        return;
    }

    QObject::connect(buffer, &Buffer::resourceDestroyed, handle, [this, buffer, side]() {
        if (auto& buf = pending.get(side); buf.get() == buffer) {
            buf.reset();
        }
        if (auto& buf = current.get(side); buf.get() == buffer) {
            buf.reset();
        }
    });
}

Shadow::Private::Private(Client* client, uint32_t version, uint32_t id, Shadow* q_ptr)
    : Wayland::Resource<Shadow>(client,
                                version,
                                id,
                                &org_kde_kwin_shadow_interface,
                                &s_interface,
                                q_ptr)
{
}

Shadow::Private::~Private()
{
    current.unref<AttachSide::Left>();
    current.unref<AttachSide::TopLeft>();
    current.unref<AttachSide::Top>();
    current.unref<AttachSide::TopRight>();
    current.unref<AttachSide::Right>();
    current.unref<AttachSide::BottomRight>();
    current.unref<AttachSide::Bottom>();
    current.unref<AttachSide::BottomLeft>();
}

Shadow::Shadow(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Shadow::Private(client, version, id, this))
{
}

QMarginsF Shadow::offset() const
{
    return d_ptr->current.offset;
}

// TODO(romangg): replace this with template function once we can use headers-only classes.
#define BUFFER(__PART__, __UPPER_)                                                                 \
    std::shared_ptr<Buffer> Shadow::__PART__() const                                               \
    {                                                                                              \
        return d_ptr->current.get<Private::AttachSide::__UPPER_>();                                \
    }

BUFFER(left, Left)
BUFFER(topLeft, TopLeft)
BUFFER(top, Top)
BUFFER(topRight, TopRight)
BUFFER(right, Right)
BUFFER(bottomRight, BottomRight)
BUFFER(bottom, Bottom)
BUFFER(bottomLeft, BottomLeft)

}
