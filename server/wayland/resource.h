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

#include "client.h"
#include "display.h"
#include "send.h"

#include <QObject>
#include <cstdint>
#include <functional>
#include <tuple>
#include <wayland-server.h>

struct wl_client;
struct wl_interface;
struct wl_resource;

namespace Wrapland::Server::Wayland
{

class Client;

template<typename Handle>
class Resource
{
public:
    Resource(Resource* parent,
             uint32_t id,
             wl_interface const* interface,
             void const* impl,
             Handle* handle)
        : Resource(parent->client(), parent->version(), id, interface, impl, handle)
    {
    }

    Resource(Server::Client* client,
             uint32_t version,
             uint32_t id,
             wl_interface const* interface,
             void const* impl,
             Handle* handle)
        : Resource(Client::cast_client(client), version, id, interface, impl, handle)
    {
    }

    Resource(Client* client,
             uint32_t version,
             uint32_t id,
             wl_interface const* interface,
             void const* impl,
             Handle* handle)
        : client{client}
        , version{version}
        , handle{handle}
        , resource{client->createResource(interface, version, id)}
    {
        wl_resource_set_user_data(resource, this);
        wl_resource_set_implementation(resource, impl, this, destroy);
    }

    Resource(Resource&) = delete;
    Resource& operator=(Resource) = delete;
    Resource(Resource&&) noexcept = delete;
    Resource& operator=(Resource&&) noexcept = delete;

    virtual ~Resource() = default;

    uint32_t id() const
    {
        return resource ? wl_resource_get_id(resource) : 0;
    }

    void flush()
    {
        client->flush();
    }

    static Handle* get_handle(wl_resource* resource)
    {
        return self(resource)->handle;
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Args&&... args)
    {
        Wayland::send<sender, minVersion>(resource, version, std::forward<Args>(args)...);
    }

    // We only support std::tuple but since it's just internal API that should be well enough.
    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(std::tuple<Args...>&& tuple)
    {
        Wayland::send_tuple<sender, minVersion>(resource, version, std::move(tuple));
    }

    void postError(uint32_t code, char const* msg, ...)
    {
        va_list args;
        va_start(args, msg);
        wl_resource_post_error(resource, code, msg, args);
        va_end(args);
    }

    static void destroyCallback([[maybe_unused]] wl_client* client, wl_resource* wlResource)
    {
        auto res = self(wlResource);
        wl_resource_destroy(res->resource);
    }

    void serverSideDestroy()
    {
        wl_resource_set_destructor(resource, nullptr);
        wl_resource_destroy(resource);
    }

    Client* client;
    uint32_t version;
    Handle* handle;
    wl_resource* resource;

private:
    static auto self(wl_resource* resource)
    {
        return static_cast<Resource<Handle>*>(wl_resource_get_user_data(resource));
    }

    static void destroy(wl_resource* wlResource)
    {
        auto resource = self(wlResource);

        resource->onDestroy();
        delete resource;
    }

    void onDestroy()
    {
        Q_EMIT handle->resourceDestroyed();
        delete handle;
    }
};

}
