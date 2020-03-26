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
#include "resource.h"

#include "client.h"
#include "global.h"

#include <cassert>
#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{
namespace Wayland
{

Resource::Resource(Resource* parent, uint32_t id, const wl_interface* interface, const void* impl)
    : Resource(parent->client(), parent->version(), id, interface, impl)
{
}

Resource::Resource(Server::Client* client,
                   uint32_t version,
                   uint32_t id,
                   const wl_interface* interface,
                   const void* impl)
    : Resource(Client::getInternal(client), version, id, interface, impl)
{
}

Resource::Resource(Client* client,
                   uint32_t version,
                   uint32_t id,
                   const wl_interface* interface,
                   const void* impl)
    : m_client{client}
    , m_version{version}
    , m_id{id}
{
    m_resource = m_client->createResource(interface, m_version, id);

    wl_resource_set_user_data(m_resource, this);
    if (impl) {
        wl_resource_set_implementation(m_resource, impl, this, unbind);
    }
}

Resource::~Resource()
{
}

void Resource::destroyCallback(wl_client* client, wl_resource* wlResource)
{
    Q_UNUSED(client);

    auto resource = Resource::internalResource(wlResource);

    resource->unbindGlobal();
    wl_resource_destroy(resource->resource());
    delete resource;
}

void Resource::unbind(wl_resource* wlResource)
{
    auto* resource = reinterpret_cast<Resource*>(wlResource->data);
    resource->unbindGlobal();
    delete resource;
}

void Resource::unbindGlobal()
{
    if (m_global) {
        m_global->unbind(this);
    }
}

wl_resource* Resource::resource() const
{
    return m_resource;
}

Global* Resource::global() const
{
    return m_global;
}

void Resource::setGlobal(Global* global)
{
    m_global = global;
}

Client* Resource::client() const
{
    return m_client;
}

uint32_t Resource::version() const
{
    return m_version;
}

uint32_t Resource::id() const
{
    return m_resource ? wl_resource_get_id(m_resource) : 0;
}

void Resource::flush()
{
    m_client->flush();
}

Resource* Resource::internalResource(wl_resource* wlResource)
{
    return reinterpret_cast<Resource*>(wlResource->data);
}

}
}
}
