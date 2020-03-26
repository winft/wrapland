/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "global.h"

#include "../display.h"
#include "client.h"
#include "display.h"

#include <iostream>

namespace Wrapland
{
namespace Server
{
namespace Wayland
{

Global::Global(D_isplay* display, const wl_interface* interface, void const* implementation)
    : m_display(Wayland::Display::backendCast(display))
    , m_interface(interface)
    , m_implementation(implementation)
    , m_global(nullptr)
{
    // TODO: allow to create and destroy Globals while keeping the object existing (but always
    //       create on ctor call?).
}

Global::~Global()
{
    for (auto bind : m_binds) {
        bind->setGlobal(nullptr);
    }
    remove();
}

void Global::create()
{
    Q_ASSERT(!m_global);

    wl_display* wlDisplay = *display()->handle();
    m_global = wl_global_create(wlDisplay, m_interface, version(), this, bind);
}

void Global::remove()
{
    if (!m_global) {
        return;
    }
    wl_global_remove(m_global);

    // TODO: call destroy with timer.
    destroy(m_global);
}

void Global::destroy(wl_global* global)
{
    wl_global_destroy(global);
}

void Global::bind(wl_client* wlClient, void* data, uint32_t version, uint32_t id)
{
    auto global = reinterpret_cast<Global*>(data);

    auto getClient = [&global, &wlClient] { return global->display()->getClient(wlClient); };
    auto bindToGlobal = [&global, version, id](auto* client) { global->bind(client, version, id); };

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

void Global::bind(Client* client, uint32_t version, uint32_t id)
{
    auto resource = new Resource(client, version, id, m_interface, m_implementation);
    resource->setGlobal(this);
    m_binds.push_back(resource);
    bindInit(client, version, id);
}

void Global::bindInit(Wayland::Client* client, uint32_t version, uint32_t id)
{
    Q_UNUSED(client);
    Q_UNUSED(version);
    Q_UNUSED(id);
}

void Global::unbind(Resource* bind)
{
    m_binds.erase(std::remove(m_binds.begin(), m_binds.end(), bind), m_binds.end());
}

void Global::resourceDestroyCallback(wl_client* wlClient, wl_resource* wlResource)
{
    Resource::destroyCallback(wlClient, wlResource);
}

uint32_t Global::version() const
{
    return 1;
}

Global::operator wl_global*() const
{
    return m_global;
}

Global::operator wl_global*()
{
    return m_global;
}

Display* Global::display()
{
    return m_display;
}

wl_interface const* Global::interface() const
{
    return m_interface;
}

void const* Global::implementation() const
{
    return m_implementation;
}

Resource* Global::findBind(Client* client)
{
    for (auto* bind : m_binds) {
        if (bind->client() == client) {
            return bind;
        }
    }
    return nullptr;
}

QVector<wl_resource*> Global::getResources(Server::Client* client)
{
    QVector<wl_resource*> ret;
    for (auto* bind : m_binds) {
        if (bind->client()->handle() == client) {
            ret << bind->resource();
        }
    }
    return ret;
}

}
}
}
