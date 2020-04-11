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
#include <QObject>

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QThread>

#include "logging.h"

#include "display.h"

#include "client.h"
#include "global.h"

#include "../client.h"
#include "../display.h"
#include "../display_p.h"

// Legacy
#include "../../src/server/display.h"

#include <algorithm>
#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{
namespace Wayland
{

Display* Display::backendCast(Server::D_isplay* display)
{
    return Private::castDisplay(display);
}

Client* Display::castClient(Server::Client* client)
{
    return backendCast(client->display())->castClientImpl(client);
}

Display::Display(Server::D_isplay* parent)
    : m_handle(parent)
{
}

Display::~Display()
{
    terminate();
    if (m_display) {
        wl_display_destroy(m_display);
    }
}

wl_display* Display::display() const
{
    return m_display;
}

std::string Display::socketName() const
{
    return m_socketName;
}

void Display::setSocketName(const std::string& name)
{
    m_socketName = name;
}

bool Display::running() const
{
    return m_running;
}

void Display::setRunning(bool running)
{
    Q_ASSERT(m_running != running);
    m_running = running;
}

void Display::addGlobal(Server::G_lobal* global)
{
    m_globals.push_back(global);
}

void Display::removeGlobal(Server::G_lobal* global)
{
    m_globals.erase(std::remove(m_globals.begin(), m_globals.end(), global), m_globals.end());
}

void Display::addSocket()
{
    if (!m_socketName.empty()) {
        // TODO: check return value. Throw.
        wl_display_add_socket(m_display, m_socketName.c_str());
        return;
    }

    char const* socket = wl_display_add_socket_auto(m_display);
    if (socket == nullptr) {
        qCWarning(WRAPLAND_SERVER) << "Failed to create Wayland socket";
    }
}

void Display::start(bool createSocket)
{
    Q_ASSERT(!m_running);
    Q_ASSERT(!m_display);

    m_display = wl_display_create();

    if (createSocket) {
        addSocket();
    }

    m_loop = wl_display_get_event_loop(m_display);
    installSocketNotifier(m_handle);
}

void Display::startLoop()
{
    Q_ASSERT(!running());
    Q_ASSERT(display());
    installSocketNotifier(m_handle);
}

void Display::terminate()
{
    if (!m_running) {
        return;
    }

    // That call is not really necessary because we run our own Qt-embedded event loop and do not
    // call wl_display_run() in the beginning. That being said leave it in here as a reminder that
    // there is this possibility.
    wl_display_terminate(m_display);

    // Then we destroy all remaining clients. There might be clients that have established
    // a connection but not yet interacted with us in any way.
    wl_display_destroy_clients(m_display);

    // Globals are also destroyed in wl_display_destroy automatically, but we need to remove all
    // global interfaces first because otherwise they would call back into the Wayland connection
    // with wl_global_destroy again. Can we do it differently?
    Q_EMIT m_handle->legacy->aboutToTerminate();

    wl_display_destroy(m_display);

    m_display = nullptr;
    m_loop = nullptr;
    setRunning(false);
}

void Display::installSocketNotifier(QObject* parent)
{
    if (!QThread::currentThread()) {
        return;
    }

    int fd = wl_event_loop_get_fd(m_loop);
    if (fd == -1) {
        qCWarning(WRAPLAND_SERVER) << "Did not get the file descriptor for the event loop";
        return;
    }

    auto* m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, parent);
    QObject::connect(m_notifier, &QSocketNotifier::activated, parent, [this] { dispatch(); });

    QObject::connect(QThread::currentThread()->eventDispatcher(),
                     &QAbstractEventDispatcher::aboutToBlock,
                     parent,
                     [this] { flush(); });
    setRunning(true);
}

void Display::flush()
{
    if (!m_display || !m_loop) {
        return;
    }
    wl_display_flush_clients(m_display);
}

void Display::dispatchEvents(int msecTimeout)
{
    Q_ASSERT(m_display);

    if (m_running) {
        dispatch();
    } else if (m_loop) {
        wl_event_loop_dispatch(m_loop, msecTimeout);
        wl_display_flush_clients(m_display);
    }
}

void Display::dispatch()
{
    if (!m_display || !m_loop) {
        return;
    }
    if (wl_event_loop_dispatch(m_loop, 0) != 0) {
        qCWarning(WRAPLAND_SERVER) << "Error on dispatching Wayland event loop";
    }
}

wl_client* Display::createClient(int fd)
{
    Q_ASSERT(fd >= 0);
    Q_ASSERT(m_display);

    auto* wlClient = wl_client_create(m_display, fd);

    // TODO: throw instead?
    if (!wlClient) {
        return nullptr;
    }
    return wlClient;
}

Client* Display::getClient(wl_client* wlClient)
{
    Q_ASSERT(wlClient);

    auto it = std::find_if(m_clients.cbegin(), m_clients.cend(), [wlClient](Client* client) {
        return *client == wlClient;
    });

    if (it != m_clients.cend()) {
        return *it;
    }

    return nullptr;
}

void Display::setupClient(Client* client)
{
    m_clients.push_back(client);

    QObject::connect(
        client->handle(), &Server::Client::disconnected, m_handle, [this](Server::Client* client) {
            for (auto* internal : m_clients) {
                if (internal->handle() != client) {
                    continue;
                }

                m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), internal),
                                m_clients.end());
                Q_EMIT m_handle->clientDisconnected(client);

                Q_EMIT m_handle->legacy->clientDisconnected(client->legacy);
            }
        });
    Q_EMIT m_handle->clientConnected(client->handle());

    Q_EMIT m_handle->legacy->clientConnected(client->handle()->legacy);
}

std::vector<Client*> const& Display::clients() const
{
    return m_clients;
}

Server::D_isplay* Display::handle() const
{
    return m_handle;
}

}
}
}
