/********************************************************************
Copyright © 2020, 2021 Roman Gilg <subdiff@gmail.com>

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
#include <memory>

namespace Wrapland::Server
{
class Client;

class WRAPLANDSERVER_EXPORT data_source : public QObject
{
    Q_OBJECT
public:
    void accept(std::string const& mimeType) const;
    void request_data(std::string const& mimeType, qint32 fd) const;
    void cancel() const;

    std::vector<std::string> mime_types() const;
    dnd_actions supported_dnd_actions() const;

    void send_dnd_drop_performed() const;
    void send_dnd_finished() const;
    void send_action(dnd_action action) const;

    Client* client() const;

Q_SIGNALS:
    void mime_type_offered(std::string);
    void supported_dnd_actions_changed();
    void resourceDestroyed();

private:
    friend class data_control_device_v1;
    friend class data_control_source_v1_res;
    friend class data_source_res;
    data_source();

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::data_source*)
