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
#pragma once

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include <wayland-server.h>

#include <memory>

namespace Wrapland::Server
{

class DataDevice;
class DataSource;
class Display;

class WRAPLANDSERVER_EXPORT DataDeviceManager : public QObject
{
    Q_OBJECT
public:
    using device_t = Wrapland::Server::DataDevice;
    using source_t = Wrapland::Server::DataSource;

    ~DataDeviceManager() override;

    enum class DnDAction {
        None = 0,
        Copy = 1 << 0,
        Move = 1 << 1,
        Ask = 1 << 2,
    };
    Q_DECLARE_FLAGS(DnDActions, DnDAction)

Q_SIGNALS:
    void sourceCreated(Wrapland::Server::DataSource* source);
    void deviceCreated(Wrapland::Server::DataDevice* device);

private:
    friend class Display;
    explicit DataDeviceManager(Display* display, QObject* parent = nullptr);

    template<typename Global>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend void create_selection_source(wl_client* wlClient, wl_resource* wlResource, uint32_t id);
    template<typename Global>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend void get_selection_device(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t id,
                                     wl_resource* wlSeat);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::DataDeviceManager::DnDActions)
