/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "region.h"

#include "compositor.h"
#include "display.h"

#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

class Region::Private : public Wayland::Resource<Region>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Region* q);

    QRegion qtRegion;

private:
    static void addCallback(wl_client* wlClient,
                            wl_resource* wlResource,
                            int32_t x,
                            int32_t y,
                            int32_t width,
                            int32_t height);
    static void subtractCallback(wl_client* wlClient,
                                 wl_resource* wlResource,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height);

    static const struct wl_region_interface s_interface;
};

const struct wl_region_interface Region::Private::s_interface = {
    destroyCallback,
    addCallback,
    subtractCallback,
};

Region::Private::Private(Client* client, uint32_t version, uint32_t id, Region* q)
    : Wayland::Resource<Region>(client, version, id, &wl_region_interface, &s_interface, q)
{
}

void Region::Private::addCallback([[maybe_unused]] wl_client* client,
                                  wl_resource* wlResource,
                                  int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height)
{
    auto priv = handle(wlResource)->d_ptr;

    priv->qtRegion = priv->qtRegion.united(QRect(x, y, width, height));
    Q_EMIT priv->handle()->regionChanged(priv->qtRegion);
}

void Region::Private::subtractCallback([[maybe_unused]] wl_client* wlClient,
                                       wl_resource* wlResource,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width,
                                       int32_t height)
{
    auto priv = handle(wlResource)->d_ptr;

    if (priv->qtRegion.isEmpty()) {
        return;
    }
    priv->qtRegion = priv->qtRegion.subtracted(QRect(x, y, width, height));
    Q_EMIT priv->handle()->regionChanged(priv->qtRegion);
}

Region::Region(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

QRegion Region::region() const
{
    return d_ptr->qtRegion;
}

Region* Region::get(void* native)
{
    return Region::Private::handle(static_cast<wl_resource*>(native));
    //    d_ptr->resou
    //    return Private::get<Region>(native);
    return nullptr;
}

Client* Region::client() const
{
    return d_ptr->client()->handle();
}

}
