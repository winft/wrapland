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

#include "data_device_manager.h"

namespace Wrapland
{
namespace Server
{

class Client;
class DataDevice;
class DataSource;

class WRAPLANDSERVER_EXPORT DataOffer : public QObject
{
    Q_OBJECT
public:
    ~DataOffer() override;

    void sendAllOffers();

    DataDeviceManager::DnDActions supportedDragAndDropActions() const;
    DataDeviceManager::DnDAction preferredDragAndDropAction() const;
    void dndAction(DataDeviceManager::DnDAction action);

Q_SIGNALS:
    void dragAndDropActionsChanged();
    void resourceDestroyed();

private:
    friend class DataDevice;

    explicit DataOffer(Client* client, uint32_t version, DataSource* source);

    class Private;
    Private* d_ptr;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::DataOffer*)
