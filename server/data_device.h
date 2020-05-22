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
    Seat* seat() const;
    Client* client() const;

    DataSource* dragSource() const;
    Surface* origin() const;
    Surface* icon() const;

    quint32 dragImplicitGrabSerial() const;

    DataSource* selection() const;

    void sendSelection(DataDevice* other);
    void sendClearSelection();

    void drop();

    void updateDragTarget(Surface* surface, quint32 serial);
    void updateProxy(Surface* remote);

Q_SIGNALS:
    void dragStarted();
    void selectionChanged(Wrapland::Server::DataSource*);
    void selectionCleared();
    void resourceDestroyed();

private:
    friend class DataDeviceManager;
    explicit DataDevice(Client* client, uint32_t version, uint32_t id, Seat* seat);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::DataDevice*)
