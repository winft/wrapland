/********************************************************************
Copyright © 2020 Adrien Faveraux <ad1rie3@hotmail.fr>

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
#include <memory>

#include "data_device_manager.h"

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class DataDeviceManager;
class Client;

class WRAPLANDSERVER_EXPORT AbstractDataSource : public QObject
{
    Q_OBJECT
public:
    virtual void accept([[maybe_unused]] std::string const& mimeType){};

    virtual void requestData(std::string const& mimeType, qint32 fd) = 0;
    virtual void cancel() = 0;

    virtual std::vector<std::string> mimeTypes() const = 0;

    virtual DataDeviceManager::DnDActions supportedDragAndDropActions() const
    {
        return {};
    };

    virtual void dropPerformed(){};
    virtual void dndFinished(){};
    virtual void dndAction([[maybe_unused]] DataDeviceManager::DnDAction action){};

    virtual Client* client() const = 0;

Q_SIGNALS:
    void mimeTypeOffered(std::string);
    void supportedDragAndDropActionsChanged();
    void resourceDestroyed();

protected:
    ~AbstractDataSource() override;
    explicit AbstractDataSource(QObject* parent = nullptr);
};

}
