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
#include "data_device_manager.h"

#include "data_device.h"
#include "data_source.h"

#include "display.h"
#include "seat_p.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

constexpr uint32_t DataDeviceManagerVersion = 3;
using DataDeviceManagerGlobal = Wayland::Global<DataDeviceManager, DataDeviceManagerVersion>;
using DataDeviceManagerBind = Wayland::Bind<DataDeviceManagerGlobal>;

class DataDeviceManager::Private : public DataDeviceManagerGlobal
{
public:
    Private(DataDeviceManager* q, Display* display);

private:
    static void createDataSourceCallback(DataDeviceManagerBind* bind, uint32_t id);
    static void
    getDataDeviceCallback(DataDeviceManagerBind* bind, uint32_t id, wl_resource* wlSeat);

    static const struct wl_data_device_manager_interface s_interface;
};

const struct wl_data_device_manager_interface DataDeviceManager::Private::s_interface = {
    cb<createDataSourceCallback>,
    cb<getDataDeviceCallback>,
};

DataDeviceManager::Private::Private(DataDeviceManager* q, Display* display)
    : DataDeviceManagerGlobal(q, display, &wl_data_device_manager_interface, &s_interface)
{
}

void DataDeviceManager::Private::createDataSourceCallback(DataDeviceManagerBind* bind, uint32_t id)
{
    auto priv = bind->global()->handle()->d_ptr.get();

    auto dataSource = new DataSource(bind->client()->handle(), bind->version(), id);
    if (!dataSource) {
        return;
    }

    Q_EMIT priv->handle()->dataSourceCreated(dataSource);
}

void DataDeviceManager::Private::getDataDeviceCallback(DataDeviceManagerBind* bind,
                                                       uint32_t id,
                                                       wl_resource* wlSeat)
{
    auto priv = bind->global()->handle()->d_ptr.get();
    auto seat = SeatGlobal::handle(wlSeat);

    auto dataDevice = new DataDevice(bind->client()->handle(), bind->version(), id, seat);
    if (!dataDevice) {
        return;
    }

    seat->d_ptr->registerDataDevice(dataDevice);
    Q_EMIT priv->handle()->dataDeviceCreated(dataDevice);
}

DataDeviceManager::DataDeviceManager(Display* display, [[maybe_unused]] QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

DataDeviceManager::~DataDeviceManager() = default;

}
