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
#include "relative_pointer_v1.h"
#include "relative_pointer_v1_p.h"

#include "display.h"
#include "pointer.h"
#include "pointer_p.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QRectF>

namespace Wrapland::Server
{

RelativePointerManagerV1::Private::Private(RelativePointerManagerV1* q, D_isplay* display)
    : Wayland::Global<RelativePointerManagerV1>(q,
                                                display,
                                                &zwp_relative_pointer_manager_v1_interface,
                                                &s_interface)
{
}

const struct zwp_relative_pointer_manager_v1_interface
    RelativePointerManagerV1::Private::s_interface
    = {
        resourceDestroyCallback,
        relativePointerCallback,
};

void RelativePointerManagerV1::Private::relativePointerCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    uint32_t id,
    wl_resource* wlPointer)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    auto relative = new RelativePointerV1(bind->client()->handle(), bind->version(), id);
    if (!relative) {
        return;
    }

    auto pointer = Wayland::Resource<Pointer>::handle(wlPointer);
    pointer->d_ptr->registerRelativePointer(relative);
}

RelativePointerManagerV1::RelativePointerManagerV1(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

RelativePointerManagerV1::~RelativePointerManagerV1() = default;

RelativePointerV1::Private::Private(Client* client,
                                    uint32_t version,
                                    uint32_t id,
                                    RelativePointerV1* q)
    : Wayland::Resource<RelativePointerV1>(client,
                                           version,
                                           id,
                                           &zwp_relative_pointer_v1_interface,
                                           &s_interface,
                                           q)
{
}

const struct zwp_relative_pointer_v1_interface RelativePointerV1::Private::s_interface = {
    destroyCallback,
};

RelativePointerV1::RelativePointerV1(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

void RelativePointerV1::relativeMotion(quint64 microseconds,
                                       const QSizeF& delta,
                                       const QSizeF& deltaNonAccelerated)
{
    d_ptr->send<zwp_relative_pointer_v1_send_relative_motion>(
        (microseconds >> 32),
        microseconds,
        wl_fixed_from_double(delta.width()),
        wl_fixed_from_double(delta.height()),
        wl_fixed_from_double(deltaNonAccelerated.width()),
        wl_fixed_from_double(deltaNonAccelerated.height()));
}

}
