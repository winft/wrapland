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
class data_device_manager;
class data_offer;
class data_source;
class Seat;
class Surface;

class WRAPLANDSERVER_EXPORT data_device : public QObject
{
    Q_OBJECT
public:
    using source_t = Wrapland::Server::data_source;

    Seat* seat() const;
    Client* client() const;

    data_source* selection() const;

    void send_selection(data_source* source);
    void send_clear_selection();

    data_offer* create_offer(data_source* source);

    void enter(uint32_t serial, Surface* surface, QPointF const& pos, data_offer* offer);
    void motion(uint32_t time, QPointF const& pos);

    void leave();
    void drop();

Q_SIGNALS:
    void selection_changed();
    void resourceDestroyed();

private:
    friend class data_device_manager;
    data_device(Client* client, uint32_t version, uint32_t id, Seat* seat);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::data_device*)
