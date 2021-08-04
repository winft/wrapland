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

namespace Wrapland::Server
{
class Client;

class WRAPLANDSERVER_EXPORT DataSource : public QObject
{
    Q_OBJECT
public:
    explicit DataSource(Client* client, uint32_t version, uint32_t id);

    void accept(std::string const& mimeType);
    void requestData(std::string const& mimeType, qint32 fd);
    void cancel();

    std::vector<std::string> mimeTypes() const;

    DataDeviceManager::DnDActions supportedDragAndDropActions() const;

    void dropPerformed();
    void dndFinished();
    void dndAction(DataDeviceManager::DnDAction action);

    Client* client() const;

Q_SIGNALS:
    void mimeTypeOffered(std::string);
    void supportedDragAndDropActionsChanged();
    void resourceDestroyed();

private:
    class Private;
    Private* d_ptr;

    template<typename Resource>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend void
    // NOLINTNEXTLINE(readability-redundant-declaration)
    add_offered_mime_type(wl_client* wlClient, wl_resource* wlResource, char const* mimeType);
};

}

Q_DECLARE_METATYPE(Wrapland::Server::DataSource*)
