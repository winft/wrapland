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
#include "touch.h"
#include "touch_pool.h"

#include "drag_pool.h"
#include "seat.h"
#include "surface.h"

#include "wayland/client.h"
#include "wayland/display.h"
#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

class Touch::Private : public Wayland::Resource<Touch>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Seat* seat, Touch* q);

    Seat* seat;

private:
    static const struct wl_touch_interface s_interface;
};

const struct wl_touch_interface Touch::Private::s_interface = {destroyCallback};

Touch::Private::Private(Client* client, uint32_t version, uint32_t id, Seat* seat, Touch* q)
    : Wayland::Resource<Touch>(client, version, id, &wl_touch_interface, &s_interface, q)
    , seat(seat)
{
}

Touch::Touch(Client* client, uint32_t version, uint32_t id, Seat* seat)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, seat, this))
{
}

void Touch::cancel()
{
    d_ptr->send<wl_touch_send_cancel>();
    d_ptr->client->flush();
}

void Touch::frame()
{
    d_ptr->send<wl_touch_send_frame>();
    d_ptr->client->flush();
}

void Touch::move(qint32 id, QPointF const& localPos)
{
    if (d_ptr->seat->drags().is_touch_drag()) {
        // Handled by data_device.
        return;
    }
    d_ptr->send<wl_touch_send_motion>(d_ptr->seat->timestamp(),
                                      id,
                                      wl_fixed_from_double(localPos.x()),
                                      wl_fixed_from_double(localPos.y()));
    d_ptr->client->flush();
}

void Touch::up(qint32 id, quint32 serial)
{
    d_ptr->send<wl_touch_send_up>(serial, d_ptr->seat->timestamp(), id);
    d_ptr->client->flush();
}

void Touch::down(qint32 id, quint32 serial, QPointF const& localPos)
{
    d_ptr->send<wl_touch_send_down>(serial,
                                    d_ptr->seat->timestamp(),
                                    d_ptr->seat->touches().get_focus().surface->resource(),
                                    id,
                                    wl_fixed_from_double(localPos.x()),
                                    wl_fixed_from_double(localPos.y()));
    d_ptr->client->flush();
}

Client* Touch::client() const
{
    return d_ptr->client->handle;
}

}
