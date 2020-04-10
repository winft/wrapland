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

#include "display.h"
#include "resource.h"

#include <cstdint>
#include <functional>
#include <vector>

#include <wayland-server.h>

struct wl_client;
struct wl_interface;
struct wl_global;
struct wl_resource;

namespace Wrapland
{
namespace Server
{
class Client;
class D_isplay;

namespace Wayland
{
class Client;
class Display;

template<typename Handle>
class Global
{
public:
    using GlobalResource = Resource<Handle, Global<Handle>>;

    virtual ~Global()
    {
        for (auto bind : m_binds) {
            bind->setGlobal(nullptr);
        }
        remove();
    }

    Global global();

    void create()
    {
        Q_ASSERT(!m_global);

        wl_display* wlDisplay = *display()->handle();
        m_global = wl_global_create(wlDisplay, m_interface, version(), this, bind);
    }

    virtual uint32_t version() const
    {
        return 1;
    }

    Display* display()
    {
        return m_display;
    }

    wl_interface const* interface() const
    {
        return m_interface;
    }

    void const* implementation() const
    {
        return m_implementation;
    }

    operator wl_global*() const
    {
        return m_global;
    }

    operator wl_global*()
    {
        return m_global;
    }

    static Handle* fromResource(wl_resource* wlResource)
    {
        auto resource = reinterpret_cast<GlobalResource*>(wl_resource_get_user_data(wlResource));
        return resource->global()->handle();
    }

    template<typename F>
    void send(Client* client, F&& sendFunction, uint32_t minVersion = 0)
    {
        auto bind = findBind(client);

        if (bind) {
            bind->send(std::forward<F>(sendFunction), minVersion);
        }
    }

    template<typename F>
    void send(F&& sendFunction, uint32_t minVersion = 0)
    {
        if (minVersion <= 1) {
            // Available for all binds.
            for (auto bind : m_binds) {
                bind->send(std::forward<F>(sendFunction));
            }
        } else {
            for (auto bind : m_binds) {
                bind->send(std::forward<F>(sendFunction), minVersion);
            }
        }
    }

    Handle* handle()
    {
        return m_handle;
    }

    void unbind(GlobalResource* bind)
    {
        m_binds.erase(std::remove(m_binds.begin(), m_binds.end(), bind), m_binds.end());
    }

    // Legacy
    QVector<wl_resource*> getResources(Server::Client* client)
    {
        QVector<wl_resource*> ret;
        for (auto* bind : m_binds) {
            if (bind->client()->handle() == client) {
                ret << bind->resource();
            }
        }
        return ret;
    }
    //

protected:
    Global(Handle* handle,
           D_isplay* display,
           const wl_interface* interface,
           void const* implementation)
        : m_handle(handle)
        , m_display(Wayland::Display::backendCast(display))
        , m_interface(interface)
        , m_implementation(implementation)
        , m_global(nullptr)
    {
        // TODO: allow to create and destroy Globals while keeping the object existing (but always
        //       create on ctor call?).
    }

    void remove()
    {
        if (!m_global) {
            return;
        }
        wl_global_remove(m_global);

        // TODO: call destroy with timer.
        destroy(m_global);
    }

    static void resourceDestroyCallback(wl_client* wlClient, wl_resource* wlResource)
    {
        Resource<Handle, Global<Handle>>::destroyCallback(wlClient, wlResource);
    }

    virtual void bindInit(Wayland::Client* client, uint32_t version, uint32_t id)
    {
        Q_UNUSED(client);
        Q_UNUSED(version);
        Q_UNUSED(id);
    }

private:
    static void destroy(wl_global* global)
    {
        wl_global_destroy(global);
    }

    Resource<Handle, Global<Handle>>* findBind(Client* client)
    {
        for (auto* bind : m_binds) {
            if (bind->client() == client) {
                return bind;
            }
        }
        return nullptr;
    }

    static void bind(wl_client* wlClient, void* data, uint32_t version, uint32_t id)
    {
        auto global = reinterpret_cast<Global*>(data);
        auto getClient = [&global, &wlClient] { return global->display()->getClient(wlClient); };

        auto bindToGlobal
            = [&global, version, id](auto* client) { global->bind(client, version, id); };

        if (auto* client = getClient()) {
            bindToGlobal(client);
            return;
        }
        // Client not yet known to the server.
        // TODO: Create backend representation only?
        global->display()->handle()->getClient(wlClient);

        // Now the client must be available.
        // TODO: otherwise send protocol error (oom)
        auto* client = getClient();
        bindToGlobal(client);
    }

    void bind(Client* client, uint32_t version, uint32_t id)
    {
        auto resource = new GlobalResource(client, version, id, m_interface, m_implementation);
        resource->setGlobal(this);
        m_binds.push_back(resource);
        bindInit(client, version, id);
    }

    Handle* m_handle;
    Display* m_display;
    wl_interface const* m_interface;
    void const* m_implementation;
    wl_global* m_global;

    std::vector<GlobalResource*> m_binds;
};

}
}
}
