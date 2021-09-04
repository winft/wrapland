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

namespace Wrapland::Server
{
class Client;
class DataDeviceManager;
class DataSource;
class Seat;
class Surface;

class WRAPLANDSERVER_EXPORT DataDevice : public QObject
{
    Q_OBJECT
public:
    using source_t = Wrapland::Server::DataSource;

    Seat* seat() const;
    Client* client() const;

    DataSource* drag_source() const;
    Surface* origin() const;
    Surface* icon() const;

    quint32 drag_implicit_grab_serial() const;

    DataSource* selection() const;

    void send_selection(DataSource* source);
    void send_clear_selection();

    void drop();

    void update_drag_target(Surface* surface, quint32 serial);
    void update_proxy(Surface* remote);

Q_SIGNALS:
    void drag_started();
    void selection_changed(Wrapland::Server::DataSource*);
    void selection_cleared();
    void resourceDestroyed();

private:
    friend class DataDeviceManager;
    DataDevice(Client* client, uint32_t version, uint32_t id, Seat* seat);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::DataDevice*)
