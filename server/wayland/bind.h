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
#include "nucleus.h"
#include "send.h"

#include <tuple>
#include <wayland-server.h>

struct wl_client;
struct wl_resource;

namespace Wrapland::Server::Wayland
{
class Client;

template<typename Global>
class Nucleus;

template<typename Global>
class Bind
{
public:
    Bind(Client* client, uint32_t version, uint32_t id, Nucleus<Global>* global_nucleus)
        : m_client{client}
        , m_version{version}
        , m_global_nucleus{global_nucleus}
        , m_resource{client->createResource(global_nucleus->interface(), version, id)}
    {
        wl_resource_set_user_data(m_resource, this);
        wl_resource_set_implementation(m_resource, global_nucleus->implementation(), this, destroy);
    }

    Bind(Bind&) = delete;
    Bind& operator=(Bind) = delete;
    Bind(Bind&&) noexcept = delete;
    Bind& operator=(Bind&&) noexcept = delete;
    virtual ~Bind() = default;

    wl_resource* resource() const
    {
        return m_resource;
    }

    Global* global() const
    {
        assert(m_global_nucleus);
        return m_global_nucleus->global();
    }

    void unset_global()
    {
        m_global_nucleus = nullptr;
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

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Args&&... args)
    {
        Wayland::send<sender, minVersion>(m_resource, m_version, args...);
    }

    // We only support std::tuple but since it's just internal API that should be well enough.
    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(std::tuple<Args...>&& tuple)
    {
        Wayland::send_tuple<sender, minVersion>(
            m_resource, m_version, std::forward<decltype(tuple)>(tuple));
    }

    void post_error(uint32_t code, char const* msg, ...)
    {
        va_list args;
        va_start(args, msg);
        wl_resource_post_error(m_resource, code, msg, args);
        va_end(args);
    }

    void post_no_memory()
    {
        wl_resource_post_no_memory(m_resource);
    }

    static void destroy_callback([[maybe_unused]] wl_client* client, wl_resource* wl_res)
    {
        auto resource = self(wl_res);
        wl_resource_destroy(resource->resource());
    }

    void serverSideDestroy()
    {
        wl_resource_set_destructor(m_resource, nullptr);
        wl_resource_destroy(m_resource);
    }

private:
    static Bind<Global>* self(wl_resource* resource)
    {
        return static_cast<Bind<Global>*>(wl_resource_get_user_data(resource));
    }

    static void destroy(wl_resource* wlResource)
    {
        auto resource = self(wlResource);

        resource->onDestroy();
        delete resource;
    }

    void onDestroy()
    {
        if (m_global_nucleus) {
            m_global_nucleus->unbind(this);
        }
    }

    Client* m_client;
    uint32_t m_version;

    Nucleus<Global>* m_global_nucleus;

    wl_resource* m_resource;
};

}
