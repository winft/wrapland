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
        : m_display{Display::backendCast(display)}
    {
    }
    virtual ~BasicNucleus() = default;

    Display* display()
    {
        return m_display;
    }

    wl_global* native()
    {
        return m_global;
    }

    void release()
    {
        m_display = nullptr;
        m_global = nullptr;
    }

protected:
    void set_native(wl_global* global)
    {
        m_global = global;
    }

private:
    Display* m_display;
    wl_global* m_global{nullptr};
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
        , m_global(global)
        , m_interface(interface)
        , m_implementation(implementation)
    {
        this->display()->addGlobal(this);
    }

    Nucleus(Nucleus const&) = delete;
    Nucleus& operator=(Nucleus const&) = delete;
    Nucleus(Nucleus&&) noexcept = delete;
    Nucleus& operator=(Nucleus&&) noexcept = delete;

    ~Nucleus() override
    {
        if (native()) {
            wl_global_set_user_data(native(), nullptr);
        }
        for (auto bind : m_binds) {
            bind->unset_global();
        }
    }

    void remove()
    {
        m_global = nullptr;

        if (native()) {
            wl_global_remove(native());
            display()->removeGlobal(this);
        } else {
            delete this;
        }
    }

    void create()
    {
        assert(!native());
        set_native(wl_global_create(display()->native(), m_interface, Global::version, this, bind));
    }

    wl_interface const* interface() const
    {
        return m_interface;
    }

    void const* implementation() const
    {
        return m_implementation;
    }

    Global* global()
    {
        return m_global;
    }

    void unbind(Bind<Global>* bind)
    {
        if (m_global) {
            m_global->prepareUnbind(bind);
        }
        m_binds.erase(std::remove(m_binds.begin(), m_binds.end(), bind), m_binds.end());
    }

    std::vector<Bind<Global>*> const& binds()
    {
        return m_binds;
    }

private:
    static void bind(wl_client* wlClient, void* data, uint32_t version, uint32_t id)
    {
        auto nucleus = static_cast<Nucleus<Global>*>(data);
        if (!nucleus) {
            // Nucleus already destroyed. Bind came in after destroy timer expired.
            return;
        }

        auto get_client = [&nucleus, &wlClient] { return nucleus->display()->getClient(wlClient); };
        auto bind_to_global
            = [&nucleus, version, id](auto client) { nucleus->bind(client, version, id); };

        if (auto client = get_client()) {
            bind_to_global(client);
            return;
        }
        // Client not yet known to the server.
        // TODO(romangg): Create backend representation only?
        nucleus->display()->handle()->getClient(wlClient);

        // Now the client must be available.
        // TODO(romangg): otherwise send protocol error (oom)
        auto client = get_client();
        bind_to_global(client);
    }

    void bind(Client* client, uint32_t version, uint32_t id)
    {
        auto resource = new Bind(client, version, id, this);
        m_binds.push_back(resource);

        if (m_global) {
            m_global->bindInit(resource);
        }
    }

    Global* m_global;
    wl_interface const* m_interface;
    void const* m_implementation;
    std::vector<Bind<Global>*> m_binds;
};

}
}
