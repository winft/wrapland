/********************************************************************
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
*********************************************************************/
#include "data_device.h"

#include "data_device_manager.h"
#include "data_offer_p.h"
#include "data_source.h"
#include "data_source_p.h"
#include "seat.h"
#include "selection_p.h"
#include "surface.h"
#include "surface_p.h"

#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

class data_device::Private : public Wayland::Resource<data_device>
{
public:
    using source_res_t = Wrapland::Server::data_source_res;

    Private(Client* client, uint32_t version, uint32_t id, Seat* seat, data_device* q);
    ~Private() override;

    data_offer* createDataOffer(data_source* source);

    Seat* seat;
    data_source* source = nullptr;
    Surface* surface = nullptr;
    Surface* icon = nullptr;

    data_source* selection = nullptr;
    QMetaObject::Connection selectionDestroyedConnection;

private:
    static void startDragCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  wl_resource* wlSource,
                                  wl_resource* wlOrigin,
                                  wl_resource* wlIcon,
                                  uint32_t serial);
    static void set_selection_callback(wl_client* wlClient,
                                       wl_resource* wlResource,
                                       wl_resource* wlSource,
                                       uint32_t id);

    void startDrag(data_source* dataSource, Surface* origin, Surface* icon, quint32 serial) const;

    static const struct wl_data_device_interface s_interface;
};

const struct wl_data_device_interface data_device::Private::s_interface = {
    startDragCallback,
    set_selection_callback,
    destroyCallback,
};

data_device::Private::Private(Client* client,
                              uint32_t version,
                              uint32_t id,
                              Seat* seat,
                              data_device* q)
    : Wayland::Resource<data_device>(client,
                                     version,
                                     id,
                                     &wl_data_device_interface,
                                     &s_interface,
                                     q)
    , seat(seat)
{
}

data_device::Private::~Private() = default;

void data_device::Private::startDragCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             wl_resource* wlSource,
                                             wl_resource* wlOrigin,
                                             wl_resource* wlIcon,
                                             uint32_t serial)
{
    auto priv = handle(wlResource)->d_ptr;
    auto source = wlSource ? Resource<data_source_res>::handle(wlSource)->src() : nullptr;
    auto origin = Resource<Surface>::handle(wlOrigin);
    auto icon = wlIcon ? Resource<Surface>::handle(wlIcon) : nullptr;

    priv->startDrag(source, origin, icon, serial);
}

void data_device::Private::startDrag(data_source* dataSource,
                                     Surface* origin,
                                     Surface* _icon,
                                     quint32 serial) const
{
    // TODO(unknown author): verify serial

    auto pointerGrab = false;
    if (seat->hasPointer()) {
        auto& pointers = seat->pointers();
        pointerGrab = pointers.has_implicit_grab(serial) && pointers.get_focus().surface == origin;
    }

    if (!pointerGrab) {
        // Client doesn't have pointer grab.
        if (!seat->hasTouch()) {
            // Client has no pointer grab and no touch capability.
            return;
        }
        auto& touches = seat->touches();
        if (!touches.has_implicit_grab(serial) || touches.get_focus().surface != origin) {
            // Client neither has pointer nor touch grab. No drag start allowed.
            return;
        }
    }

    seat->drags().start(dataSource, origin, _icon, serial);
}

void data_device::Private::set_selection_callback(wl_client* /*wlClient*/,
                                                  wl_resource* wlResource,
                                                  wl_resource* wlSource,
                                                  uint32_t /*id*/)
{
    // TODO(unknown author): verify serial
    auto handle = Resource::handle(wlResource);
    auto source_res = wlSource ? Wayland::Resource<data_source_res>::handle(wlSource) : nullptr;

    // TODO(romangg): move errors into Wayland namespace.
    if (source_res && source_res->src()->supported_dnd_actions()
        && wl_resource_get_version(wlSource) >= WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        wl_resource_post_error(
            wlSource, WL_DATA_SOURCE_ERROR_INVALID_SOURCE, "Data source is for drag and drop");
        return;
    }

    set_selection(handle, handle->d_ptr, wlSource);
}

data_offer* data_device::Private::createDataOffer(data_source* source)
{
    if (!source) {
        // A data offer can only exist together with a source.
        return nullptr;
    }

    auto offer = new data_offer(client()->handle(), version(), source);

    if (!offer->d_ptr->resource()) {
        // TODO(unknown author): send error?
        delete offer;
        return nullptr;
    }

    send<wl_data_device_send_data_offer>(offer->d_ptr->resource());
    offer->send_all_offers();
    return offer;
}

data_device::data_device(Client* client, uint32_t version, uint32_t id, Seat* seat)
    : d_ptr(new Private(client, version, id, seat, this))
{
}

Seat* data_device::seat() const
{
    return d_ptr->seat;
}

data_source* data_device::selection() const
{
    return d_ptr->selection;
}

void data_device::send_selection(data_source* source)
{
    if (!source) {
        send_clear_selection();
        return;
    }

    auto offer = d_ptr->createDataOffer(source);
    if (!offer) {
        return;
    }

    d_ptr->send<wl_data_device_send_selection>(offer->d_ptr->resource());
}

void data_device::send_clear_selection()
{
    d_ptr->send<wl_data_device_send_selection>(nullptr);
}

data_offer* data_device::create_offer(data_source* source)
{
    return d_ptr->createDataOffer(source);
}

void data_device::enter(uint32_t serial, Surface* surface, QPointF const& pos, data_offer* offer)
{
    assert(surface);
    d_ptr->send<wl_data_device_send_enter>(serial,
                                           surface->d_ptr->resource(),
                                           wl_fixed_from_double(pos.x()),
                                           wl_fixed_from_double(pos.y()),
                                           offer ? offer->d_ptr->resource() : nullptr);
}

void data_device::motion(uint32_t time, QPointF const& pos)
{
    d_ptr->send<wl_data_device_send_motion>(
        time, wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
}

void data_device::leave()
{
    d_ptr->send<wl_data_device_send_leave>();
}

void data_device::drop()
{
    d_ptr->send<wl_data_device_send_drop>();
}

Client* data_device::client() const
{
    return d_ptr->client()->handle();
}

}
