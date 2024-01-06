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

namespace Wrapland::Server
{
class Client;
class Display;

namespace Wayland
{
class Display;

class Client
{
public:
    Client(wl_client* native, Server::Client* handle);

    Client(Client const&) = delete;
    Client& operator=(Client const&) = delete;
    Client(Client&&) noexcept = default;
    Client& operator=(Client&&) noexcept = default;

    virtual ~Client();

    void flush() const;
    wl_resource* createResource(wl_interface const* interface, uint32_t version, uint32_t id) const;

    Display* display() const;

    pid_t processId() const;
    uid_t userId() const;
    gid_t groupId() const;
    std::string executablePath() const;

    void destroy() const;

    wl_client* native;
    Server::Client* handle;

private:
    static void destroyListenerCallback(wl_listener* listener, void* data);

    pid_t m_pid = 0;
    uid_t m_user = 0;
    gid_t m_group = 0;
    std::string m_executablePath;

    struct DestroyWrapper {
        Client* client;
        struct wl_listener listener;
    };
    DestroyWrapper m_destroyWrapper;
};

}
}
