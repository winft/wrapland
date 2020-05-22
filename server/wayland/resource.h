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

#include "client.h"
#include "display.h"

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

template<typename Handle, typename GlobalHandle = void>
class Resource
{
public:
    using ResourceType = Resource<Handle, GlobalHandle>;

    Resource(Resource* parent,
             uint32_t id,
             const wl_interface* interface,
             const void* impl,
             Handle* handle)
        : Resource(parent->client(), parent->version(), id, interface, impl, handle)
    {
    }

    Resource(Server::Client* client,
             uint32_t version,
             uint32_t id,
             const wl_interface* interface,
             const void* impl,
             Handle* handle)
        : Resource(Display::castClient(client), version, id, interface, impl, handle)
    {
    }

    Resource(Client* client,
             uint32_t version,
             uint32_t id,
             const wl_interface* interface,
             const void* impl,
             Handle* handle)
        : m_client{client}
        , m_version{version}
        , m_handle{handle}
        , m_global{nullptr}
        , m_resource{client->createResource(interface, version, id)}
    {
        wl_resource_set_user_data(m_resource, this);
        wl_resource_set_implementation(m_resource, impl, this, destroy);
    }

    template<typename = std::enable_if<!std::is_same_v<GlobalHandle, void>>>
    Resource(Client* client,
             uint32_t version,
             uint32_t id,
             const wl_interface* interface,
             const void* impl)
        : Resource(client, version, id, interface, impl, nullptr)
    {
    }

    Resource(Resource&) = delete;
    Resource& operator=(Resource) = delete;
    Resource(Resource&&) noexcept = delete;
    Resource& operator=(Resource&&) noexcept = delete;

    virtual ~Resource() = default;

    wl_resource* resource() const
    {
        return m_resource;
    }

    template<typename = std::enable_if<!std::is_same_v<GlobalHandle, void>>>
    GlobalHandle* global() const
    {
        return m_global;
    }

    template<typename = std::enable_if<!std::is_same_v<GlobalHandle, void>>>
    void setGlobal(GlobalHandle* global)
    {
        m_global = global;
    }

    Client* client() const
    {
        return m_client;
    }

    uint32_t version() const
    {
        return m_version;
    }

    uint32_t id() const
    {
        return m_resource ? wl_resource_get_id(m_resource) : 0;
    }

    void flush()
    {
        m_client->flush();
    }

    template<typename = std::enable_if<!std::is_same_v<GlobalHandle, void>>>
    static Handle* handle(wl_resource* resource)
    {
        return self(resource)->m_handle;
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Args&&... args)
    {
        if constexpr (minVersion <= 1) {
            sender(m_resource, args...);
        } else {
            if (m_version >= minVersion) {
                sender(m_resource, args...);
            }
        }
    }

    // We only support std::tuple but since it's just internal API that should be well enough.
    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(std::tuple<Args...>&& tuple)
    {
        constexpr auto size = std::tuple_size_v<std::decay_t<decltype(tuple)>>;
        constexpr auto indices = std::make_index_sequence<size>{};
        sendTuple<sender, minVersion>(std::forward<decltype(tuple)>(tuple), indices);
    }

    void postError(uint32_t code, char const* msg, ...)
    {
        va_list args;
        va_start(args, msg);
        wl_resource_post_error(m_resource, code, msg, args);
        va_end(args);
    }

    static void destroyCallback([[maybe_unused]] wl_client* client, wl_resource* wlResource)
    {
        auto resource = self(wlResource);
        wl_resource_destroy(resource->resource());
    }

    template<typename = std::enable_if<std::is_same_v<GlobalHandle, void>>>
    Handle* handle()
    {
        return m_handle;
    }

private:
    static ResourceType* self(wl_resource* resource)
    {
        return static_cast<ResourceType*>(wl_resource_get_user_data(resource));
    }

    static void destroy(wl_resource* wlResource)
    {
        auto resource = self(wlResource);

        resource->onDestroy();
        delete resource;
    }

    void onDestroy()
    {
        if constexpr (std::is_same_v<GlobalHandle, void>) {
            Q_EMIT m_handle->resourceDestroyed();
            delete m_handle;
        } else {
            if (m_global) {
                m_global->unbind(this);
            }
        }
    }

    template<auto sender, uint32_t minVersion, typename Tuple, std::size_t... Indices>
    void sendTuple(Tuple&& tuple, [[maybe_unused]] std::index_sequence<Indices...> indices)
    {
        send<sender, minVersion>(std::get<Indices>(tuple)...);
    }

    Client* m_client;
    uint32_t m_version;

    Handle* m_handle;
    GlobalHandle* m_global;

    wl_resource* m_resource;
};

}
