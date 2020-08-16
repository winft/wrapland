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

#include "capsule.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_global;

class QObject;

namespace Wrapland::Server
{
class Client;
class Display;

namespace Wayland
{
class BufferManager;
class Client;

using GlobalCapsule = Capsule<wl_global>;

class Display
{
public:
    explicit Display(Server::Display* parent);
    virtual ~Display();

    void setSocketName(const std::string& name);

    void addGlobal(GlobalCapsule* capsule);
    void removeGlobal(GlobalCapsule* capsule);

    wl_display* native() const;
    std::string socketName() const;

    void start(bool createSocket);
    void terminate();

    void startLoop();

    void flush();

    void dispatchEvents(int msecTimeout = -1);
    void dispatch();

    bool running() const;
    void setRunning(bool running);
    void installSocketNotifier(QObject* parent);

    wl_client* createClient(int fd);
    Client* getClient(wl_client* wlClient);

    void setupClient(Client* client);

    static Client* castClient(Server::Client* client);

    std::vector<Client*> const& clients() const;

    Server::Display* handle() const;

    static Display* backendCast(Server::Display* display);

    BufferManager* bufferManager() const;

protected:
    virtual Client* castClientImpl(Server::Client* client) = 0;

private:
    void addSocket();
    wl_display* m_display = nullptr;
    wl_event_loop* m_loop = nullptr;
    std::string m_socketName;

    bool m_running = false;

    std::vector<GlobalCapsule*> m_globals;
    std::vector<GlobalCapsule*> m_stale_globals;
    static int constexpr s_global_stale_time{5000};

    std::vector<Client*> m_clients;
    std::unique_ptr<BufferManager> m_bufferManager;

    Server::Display* m_handle;
};

}
}
