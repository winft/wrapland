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

#include <string>
#include <sys/types.h>
#include <vector>

struct wl_client;
struct wl_interface;
struct wl_resource;

#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{
class Client;
class D_isplay;
class Display;

namespace Wayland
{
class Display;

class Client
{
public:
    explicit Client(wl_client* wlClient, Server::Client* clientHandle);
    virtual ~Client();

    void flush();
    wl_resource* createResource(const wl_interface* interface, uint32_t version, uint32_t id);
    wl_resource* getResource(uint32_t id);

    Display* display() const;
    Server::Client* handle() const;

    pid_t processId() const;
    uid_t userId() const;
    gid_t groupId() const;
    std::string executablePath() const;

    wl_client* native() const;

    void destroy();

private:
    static void destroyListenerCallback(wl_listener* listener, void* data);
    static std::vector<Client*> allClients;

    wl_client* m_client;
    wl_listener m_listener;

    pid_t m_pid = 0;
    uid_t m_user = 0;
    gid_t m_group = 0;
    std::string m_executablePath;

    Server::Client* q_ptr;

    // TODO: move this into Server::Client?
    Server::D_isplay* m_display;
};

}
}
}
