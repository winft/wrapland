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

#include <string>
#include <sys/types.h>

struct wl_client;
struct wl_interface;
struct wl_resource;

namespace Wrapland
{
namespace Server
{

class D_isplay;

//
// Legacy
class ClientConnection;
//
//

class WRAPLANDSERVER_EXPORT Client : public QObject
{
    Q_OBJECT
public:
    ~Client() override;
    void destroy();

    void flush();

    //
    // Legacy (?)
    wl_resource* createResource(const wl_interface* interface, quint32 version, quint32 id);
    wl_resource* getResource(quint32 id);

    ClientConnection* legacy;
    //
    //

    wl_client* client();
    D_isplay* display();

    pid_t processId() const;
    uid_t userId() const;
    gid_t groupId() const;
    std::string executablePath() const;

    operator wl_client*();
    operator wl_client*() const;

Q_SIGNALS:
    void disconnected(Client*);

private:
    Client(wl_client* wlClient, D_isplay* display);
    friend class Private;

    class Private;
    Private* d_ptr;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::Client*)
