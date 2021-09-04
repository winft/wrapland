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

#include "data_device_manager.h"

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>

namespace Wrapland::Server
{

class Client;
class data_device;
class data_source;

class WRAPLANDSERVER_EXPORT data_offer : public QObject
{
    Q_OBJECT
public:
    void send_all_offers();

    dnd_actions supported_dnd_actions() const;
    dnd_action preferred_dnd_action() const;
    void send_action(dnd_action action);

Q_SIGNALS:
    void dnd_actions_changed();
    void resourceDestroyed();

private:
    friend class data_device;
    explicit data_offer(Client* client, uint32_t version, data_source* source);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::data_offer*)
