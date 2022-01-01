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

#include "bind.h"
#include "display.h"
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

template<typename Global>
class Bind;

class BasicNucleus
{
public:
    explicit BasicNucleus(Server::Display* display)
        : display{Display::backendCast(display)}
    {
    }
    virtual ~BasicNucleus() = default;

    void release()
    {
        display = nullptr;
        native_global = nullptr;
    }

    wl_global* native_global{nullptr};
    Display* display;
};

template<typename Global>
class Nucleus : public BasicNucleus
{
public:
    Nucleus(Global* global,
            Server::Display* display,
            const wl_interface* interface,
            void const* implementation)
        : BasicNucleus(display)
        , global{global}
        , interface {
        interface
    }, implementation{implementation}
    {
        this->display->addGlobal(this);
    }

    Nucleus(Nucleus const&) = delete;
    Nucleus& operator=(Nucleus const&) = delete;
    Nucleus(Nucleus&&) noexcept = delete;
    Nucleus& operator=(Nucleus&&) noexcept = delete;

    ~Nucleus() override
    {
        if (native_global) {
            wl_global_set_user_data(native_global, nullptr);
        }
        for (auto bind : binds) {
            bind->unset_global();
        }
    }

    void remove()
    {
        global = nullptr;

        if (native_global) {
            wl_global_remove(native_global);
            display->removeGlobal(this);
        } else {
            delete this;
        }
    }

    void create()
    {
        assert(!native_global);
        native_global = wl_global_create(display->native(), interface, Global::version, this, bind);
    }

    void unbind(Bind<Global>* bind)
    {
        if (global) {
            global->prepareUnbind(bind);
        }
        binds.erase(std::remove(binds.begin(), binds.end(), bind), binds.end());
    }

    Global* global;
    wl_interface const* interface;
    void const* implementation;

    std::vector<Bind<Global>*> binds;

private:
    static void bind(wl_client* wlClient, void* data, uint32_t version, uint32_t id)
    {
        auto nucleus = static_cast<Nucleus<Global>*>(data);
        if (!nucleus) {
            // Nucleus already destroyed. Bind came in after destroy timer expired.
            return;
        }

        auto get_client = [&nucleus, &wlClient] { return nucleus->display->getClient(wlClient); };
        auto bind_to_global
            = [&nucleus, version, id](auto client) { nucleus->bind(client, version, id); };

        if (auto client = get_client()) {
            bind_to_global(client);
            return;
        }
        // Client not yet known to the server.
        // TODO(romangg): Create backend representation only?
        nucleus->display->handle->getClient(wlClient);

        // Now the client must be available.
        // TODO(romangg): otherwise send protocol error (oom)
        auto client = get_client();
        bind_to_global(client);
    }

    void bind(Client* client, uint32_t version, uint32_t id)
    {
        auto resource = new Bind(client, version, id, this);
        binds.push_back(resource);

        if (global) {
            global->bindInit(resource);
        }
    }
};

}
}
