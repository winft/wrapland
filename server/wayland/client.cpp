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

#include <QFileInfo>

#include <wayland-server.h>

namespace Wrapland::Server::Wayland
{

Client::Client(wl_client* native, Server::Client* handle)
    : native{native}
    , handle{handle}
{
    m_destroyWrapper.client = this;
    m_destroyWrapper.listener.notify = destroyListenerCallback;
    wl_client_add_destroy_listener(native, &m_destroyWrapper.listener);
    wl_client_get_credentials(native, &m_pid, &m_user, &m_group);

    m_executablePath
        // Qt types have limited compatibility with modern C++. Remove this clang-tidy exception
        // once it is ported to std::string.
        // NOLINTNEXTLINE(performance-no-automatic-move)
        = QFileInfo(QStringLiteral("/proc/%1/exe").arg(m_pid)).symLinkTarget().toUtf8().constData();
}

Client::~Client()
{
    if (native) {
        wl_list_remove(&m_destroyWrapper.listener.link);
    }
}

Display* Client::display() const
{
    return Display::backendCast(handle->display());
}

void Client::flush() const
{
    if (!native) {
        return;
    }
    wl_client_flush(native);
}

void Client::destroy() const
{
    if (!native) {
        return;
    }
    wl_client_destroy(native);
}

Client* Client::cast_client(Server::Client* client)
{
    return client->d_ptr.get();
}

Client* Client::create_client(wl_client* wlClient, Display* display)
{
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    return (new Server::Client(wlClient, display->handle))->d_ptr.get();
}

void Client::destroyListenerCallback(wl_listener* listener, [[maybe_unused]] void* data)
{
    // The wl_container_of macro can not be used with auto keyword and in the macro from libwayland
    // the alignment is increased.
    // Relevant clang-tidy checks are:
    // * clang-diagnostic-cast-align
    // * cppcoreguidelines-pro-bounds-pointer-arithmetic
    // * hicpp-use-auto
    // * modernize-use-auto
    // NOLINTNEXTLINE
    DestroyWrapper* wrapper = wl_container_of(listener, wrapper, listener);
    auto client = wrapper->client;

    wl_list_remove(&client->m_destroyWrapper.listener.link);
    client->native = nullptr;
    Q_EMIT client->handle->disconnected(client->handle);
    delete client->handle;
}

wl_resource*
Client::createResource(wl_interface const* interface, uint32_t version, uint32_t id) const
{
    if (!native) {
        return nullptr;
    }
    return wl_resource_create(native, interface, static_cast<int>(version), id);
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

std::string Client::security_context_app_id() const
{
    return m_security_context_app_id;
}

void Client::set_security_context_app_id(std::string const& id)
{
    m_security_context_app_id = id;
}

}
