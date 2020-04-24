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

#include "capsule.h"
#include "resource.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <vector>

#include <wayland-server.h>
#include <wayland-version.h>

namespace Wrapland
{
namespace Server
{
class Client;
class D_isplay;

namespace Wayland
{
class Client;

template<typename Handle>
class Global
{
public:
    using GlobalResource = Resource<Handle, Global<Handle>>;

    virtual ~Global()
    {
        m_display->removeGlobal(m_capsule.get());
        for (auto bind : m_binds) {
            bind->setGlobal(nullptr);
        }
        remove();
    }

    void create()
    {
        Q_ASSERT(!m_capsule->valid());

        m_capsule->create(
            wl_global_create(*display()->handle(), m_interface, version(), this, bind));
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
        return *m_capsule;
    }

    operator wl_global*()
    {
        return *m_capsule;
    }

    static Handle* fromResource(wl_resource* wlResource)
    {
        auto resource = reinterpret_cast<GlobalResource*>(wl_resource_get_user_data(wlResource));
        return resource->global()->handle();
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(GlobalResource* bind, Args&&... args)
    {
        // See Vandevoorde et al.: C++ Templates - The Complete Guide p.79
        // or https://stackoverflow.com/a/4942746.
        bind->template send<sender, minVersion>(std::forward<Args>(args)...);
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Client* client, Args&&... args)
    {
        for (auto bind : m_binds) {
            if (bind->client() == client) {
                bind->template send<sender, minVersion>(std::forward<Args>(args)...);
            }
        }
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Args&&... args)
    {
        for (auto bind : m_binds) {
            bind->template send<sender, minVersion>(std::forward<Args>(args)...);
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

    GlobalResource* getBind(wl_resource* wlResource)
    {
        for (auto bind : m_binds) {
            if (bind->resource() == wlResource) {
                return bind;
            }
        }
        return nullptr;
    }

    std::vector<GlobalResource*> getBinds(Server::Client* client)
    {
        std::vector<GlobalResource*> ret;
        for (auto bind : m_binds) {
            if (bind->client()->handle() == client) {
                ret.push_back(bind);
            }
        }
        return ret;
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
        , m_display(Display::backendCast(display))
        , m_interface(interface)
        , m_implementation(implementation)
        , m_capsule{new GlobalCapsule(wl_global_destroy)}
    {
        m_display->addGlobal(m_capsule.get());

        // TODO: allow to create and destroy Globals while keeping the object existing (but always
        //       create on ctor call?).
    }

    void remove()
    {
        if (!m_capsule->valid()) {
            return;
        }
#if (WAYLAND_VERSION_MAJOR > 1 || WAYLAND_VERSION_MINOR > 17)
        wl_global_remove(*m_capsule);
#endif
        // TODO: call destroy with timer.
        destroy(std::move(m_capsule));
    }

    static void resourceDestroyCallback(wl_client* wlClient, wl_resource* wlResource)
    {
        GlobalResource::destroyCallback(wlClient, wlResource);
    }

    virtual void bindInit([[maybe_unused]] GlobalResource* bind)
    {
    }

private:
    static void destroy(std::unique_ptr<GlobalCapsule> global)
    {
        global.reset();
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
        bindInit(resource);
    }

    Handle* m_handle;
    Display* m_display;
    wl_interface const* m_interface;
    void const* m_implementation;

    std::unique_ptr<GlobalCapsule> m_capsule;
    std::vector<GlobalResource*> m_binds;
};

}
}
}
