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

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{
class Client;
class data_device;
class data_source;
class Display;
class Seat;

class WRAPLANDSERVER_EXPORT data_device_manager : public QObject
{
    Q_OBJECT
public:
    using device_t = Wrapland::Server::data_device;
    using source_t = Wrapland::Server::data_source;

    explicit data_device_manager(Display* display);
    ~data_device_manager() override;

    void create_source(Client* client, uint32_t version, uint32_t id);
    void get_device(Client* client, uint32_t version, uint32_t id, Seat* seat);

Q_SIGNALS:
    void source_created(Wrapland::Server::data_source* source);
    void device_created(Wrapland::Server::data_device* device);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
