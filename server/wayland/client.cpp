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
#include "client.h"
#include "../client.h"
#include "../client_p.h"

#include "display.h"

// For legacy
#include "../../src/server/display.h"
#include "../display.h"
//

#include <QFileInfo>

#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{
namespace Wayland
{

Client::Client(wl_client* wlClient, Server::Client* clientHandle, Server::Display* legacy)
    : m_client{wlClient}
    , q_ptr{clientHandle}
{
    allClients.push_back(this);
    q_ptr->legacy = new Server::ClientConnection(m_client, legacy);
    q_ptr->legacy->newClient = q_ptr;

    m_listener.notify = destroyListenerCallback;
    wl_client_add_destroy_listener(wlClient, &m_listener);
    wl_client_get_credentials(wlClient, &m_pid, &m_user, &m_group);
    m_executablePath
        = QFileInfo(QStringLiteral("/proc/%1/exe").arg(m_pid)).symLinkTarget().toUtf8().constData();
}

Client::~Client()
{
    if (m_client) {
        wl_list_remove(&m_listener.link);
    }
    allClients.erase(std::remove(allClients.begin(), allClients.end(), this), allClients.end());
}

Server::Client* Client::handle() const
{
    return q_ptr;
}

void Client::flush()
{
    if (!m_client) {
        return;
    }
    wl_client_flush(m_client);
}

void Client::destroy()
{
    if (!m_client) {
        return;
    }
    wl_client_destroy(m_client);
}

std::vector<Client*> Client::allClients;

void Client::destroyListenerCallback(wl_listener* listener, void* data)
{
    Q_UNUSED(listener);

    auto* wlClient = reinterpret_cast<wl_client*>(data);

    auto it = std::find_if(allClients.cbegin(), allClients.cend(), [wlClient](Client* client) {
        return *client == wlClient;
    });
    Q_ASSERT(it != allClients.cend());
    auto* client = *it;

    wl_list_remove(&client->m_listener.link);
    client->m_client = nullptr;
    Q_EMIT client->q_ptr->disconnected(client->q_ptr);
    Q_EMIT client->q_ptr->legacy->disconnected(client->q_ptr->legacy);
    delete client->handle();
}

wl_resource* Client::createResource(const wl_interface* interface, uint32_t version, uint32_t id)
{
    if (!m_client) {
        return nullptr;
    }
    return wl_resource_create(m_client, interface, version, id);
}

wl_resource* Client::getResource(uint32_t id)
{
    if (!m_client) {
        return nullptr;
    }
    return wl_client_get_object(m_client, id);
}

Client::operator wl_client*()
{
    return m_client;
}

Client::operator wl_client*() const
{
    return m_client;
}

gid_t Client::groupId() const
{
    return m_group;
}

pid_t Client::processId() const
{
    return m_pid;
}

uid_t Client::userId() const
{
    return m_user;
}

std::string Client::executablePath() const
{
    return m_executablePath;
}

}
}
}
