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

#include "resource.h"

#include <cstdint>
#include <functional>
#include <vector>

#include <wayland-server.h>

struct wl_client;
struct wl_interface;
struct wl_global;
struct wl_resource;

namespace Wrapland
{
namespace Server
{
class Client;
class D_isplay;

namespace Wayland
{
class Client;
class Display;

class Global
{
public:
    virtual ~Global();

    Global global();

    void create();

    virtual uint32_t version() const;

    /**
     * @returns the Display the Global got created on.
     */
    Display* display();

    /**
     * Cast operator to the native wl_global this Global represents.
     */
    operator wl_global*();
    /**
     * Cast operator to the native wl_global this Global represents.
     */
    operator wl_global*() const;

    wl_interface const* interface() const;
    void const* implementation() const;

    template<typename Priv>
    static Priv fromResource(wl_resource* wlResource)
    {
        auto resource = reinterpret_cast<Resource*>(wl_resource_get_user_data(wlResource));
        return static_cast<Priv>(resource->global());
    }

    template<typename F>
    void send(Client* client, F&& sendFunction, uint32_t minVersion = 0)
    {
        auto bind = findBind(client);

        if (bind) {
            bind->send(std::forward<F>(sendFunction), minVersion);
        }
    }

    template<typename F>
    void send(F&& sendFunction, uint32_t minVersion = 0)
    {
        if (minVersion <= 1) {
            // Available for all binds.
            for (auto bind : m_binds) {
                bind->send(std::forward<F>(sendFunction));
            }
        } else {
            for (auto bind : m_binds) {
                bind->send(std::forward<F>(sendFunction), minVersion);
            }
        }
    }

    void unbind(Resource* bind);

    // Legacy
    QVector<wl_resource*> getResources(Server::Client* client);
    //

protected:
    Global(D_isplay* display, wl_interface const* interface, void const* implementation);
    void remove();

    static void resourceDestroyCallback(wl_client* wlClient, wl_resource* wlResource);

    virtual void bindInit(Client* client, uint32_t version, uint32_t id);

private:
    static void destroy(wl_global* global);
    Resource* findBind(Client* client);

    static void bind(wl_client* wlClient, void* data, uint32_t version, uint32_t id);
    void bind(Client* client, uint32_t version, uint32_t id);

    Display* m_display;
    wl_interface const* m_interface;
    void const* m_implementation;
    wl_global* m_global;

    std::vector<Resource*> m_binds;
};

}
}
}
