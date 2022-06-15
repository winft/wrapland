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

#include "bind.h"
#include "display.h"
#include "nucleus.h"
#include "resource.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <vector>

#include <wayland-server.h>

namespace Wrapland::Server
{
class Client;
class Display;

namespace Wayland
{
class Client;

template<typename Handle, int Version = 1>
class Global
{
public:
    using type = Global<Handle, Version>;
    static int constexpr version = Version;

    Global(Global const&) = delete;
    Global& operator=(Global const&) = delete;
    Global(Global&&) noexcept = delete;
    Global& operator=(Global&&) noexcept = delete;

    virtual ~Global()
    {
        nucleus->remove();
    }

    void create()
    {
        nucleus->create();
    }

    Display* display()
    {
        return nucleus->display;
    }

    static Handle* get_handle(wl_resource* wlResource)
    {
        auto bind = static_cast<Bind<type>*>(wl_resource_get_user_data(wlResource));

        if (auto global = bind->global()) {
            return global->handle;
        }

        // If we are here the global has been removed while not yet destroyed.
        return nullptr;
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Bind<type>* bind, Args&&... args)
    {
        // See Vandevoorde et al.: C++ Templates - The Complete Guide p.79
        // or https://stackoverflow.com/a/4942746.
        bind->template send<sender, minVersion>(std::forward<Args>(args)...);
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Client* client, Args&&... args)
    {
        for (auto bind : nucleus->binds()) {
            if (bind->client() == client) {
                bind->template send<sender, minVersion>(std::forward<Args>(args)...);
            }
        }
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Args&&... args)
    {
        for (auto bind : nucleus->binds) {
            bind->template send<sender, minVersion>(std::forward<Args>(args)...);
        }
    }

    Bind<type>* getBind(wl_resource* wlResource)
    {
        for (auto bind : nucleus->binds) {
            if (bind->resource == wlResource) {
                return bind;
            }
        }
        return nullptr;
    }

    std::vector<Bind<type>*> getBinds()
    {
        return nucleus->binds;
    }

    std::vector<Bind<type>*> getBinds(Server::Client* client)
    {
        std::vector<Bind<type>*> ret;
        for (auto bind : nucleus->binds) {
            if (bind->client->handle == client) {
                ret.push_back(bind);
            }
        }
        return ret;
    }

    virtual void bindInit([[maybe_unused]] Bind<type>* bind)
    {
    }

    virtual void prepareUnbind([[maybe_unused]] Bind<type>* bind)
    {
    }

    Handle* handle;

protected:
    Global(Handle* handle,
           Server::Display* display,
           const wl_interface* interface,
           void const* implementation)
        : handle{handle}
        , nucleus{new Nucleus<type>(this, display, interface, implementation)}
    {
        // TODO(romangg): allow to create and destroy Globals while keeping the object existing (but
        //                always create on ctor call?).
    }

    static void resourceDestroyCallback(wl_client* wlClient, wl_resource* wlResource)
    {
        Bind<type>::destroy_callback(wlClient, wlResource);
    }

    template<auto callback, typename... Args>
    static void cb([[maybe_unused]] wl_client* client, wl_resource* resource, Args... args)
    {
        // The global might be destroyed already on the compositor side.
        if (get_handle(resource)) {
            auto bind = static_cast<Bind<type>*>(wl_resource_get_user_data(resource));
            callback(bind, std::forward<Args>(args)...);
        }
    }

private:
    Nucleus<type>* nucleus;
};

}
}
