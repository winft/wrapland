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
#include "selection_device_manager_p.h"

#include "wayland/global.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

constexpr uint32_t DataDeviceManagerVersion = 3;
using DataDeviceManagerGlobal = Wayland::Global<DataDeviceManager, DataDeviceManagerVersion>;

class DataDeviceManager::Private : public DataDeviceManagerGlobal
{
public:
    Private(DataDeviceManager* q, Display* display);

private:
    static const struct wl_data_device_manager_interface s_interface;
};

const struct wl_data_device_manager_interface DataDeviceManager::Private::s_interface = {
    create_selection_source<DataDeviceManagerGlobal>,
    get_selection_device<DataDeviceManagerGlobal>,
};

DataDeviceManager::Private::Private(DataDeviceManager* q, Display* display)
    : DataDeviceManagerGlobal(q, display, &wl_data_device_manager_interface, &s_interface)
{
}

DataDeviceManager::DataDeviceManager(Display* display, [[maybe_unused]] QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

DataDeviceManager::~DataDeviceManager() = default;

}
