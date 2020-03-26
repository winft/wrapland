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

#include <QDebug>

#include <cstdint>
#include <functional>
#include <wayland-server.h>

struct wl_client;
struct wl_interface;
struct wl_resource;

namespace Wrapland
{
namespace Server
{
class Client;

namespace Wayland
{

class Client;
class Global;

class Resource
{
public:
    Resource(Resource* parent, uint32_t id, const wl_interface* interface, const void* impl);

    Resource(Server::Client* client,
             uint32_t version,
             uint32_t id,
             const wl_interface* interface,
             const void* impl);

    Resource(Client* client,
             uint32_t version,
             uint32_t id,
             const wl_interface* interface,
             const void* impl);

    virtual ~Resource();

    Client* client() const;
    uint32_t id() const;
    uint32_t version() const;
    wl_resource* resource() const;

    Global* global() const;
    void setGlobal(Global* global);

    template<typename T>
    static T* fromResource(wl_resource* resource)
    {
        return reinterpret_cast<T*>(wl_resource_get_user_data(resource));
    }

    template<typename F>
    void send(F&& sendFunction)
    {
        sendFunction(m_resource);
    }

    template<typename F>
    void send(F&& sendFunction, uint32_t minVersion)
    {
        if (m_version >= minVersion) {
            sendFunction(m_resource);
        }
    }

    void flush();

    static void destroyCallback(wl_client* client, wl_resource* wlResource);

    static Resource* internalResource(wl_resource* wlResource);

private:
    static void unbind(wl_resource* wlResource);
    void unbindGlobal();
    Client* m_client;
    uint32_t m_version;
    uint32_t m_id;
    wl_resource* m_resource;

    Global* m_global = nullptr;

    wl_interface* m_interface;
    void* m_implementation;
};

}
}
}
