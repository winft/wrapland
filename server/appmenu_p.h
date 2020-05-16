/****************************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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
****************************************************************************/
#pragma once

#include <QObject>
#include <memory>

#include "appmenu.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-appmenu-server-protocol.h>

namespace Wrapland::Server
{

class D_isplay;
class Surface;
class AppMenu;

constexpr uint32_t AppMenuManagerVersion = 1;
using AppMenuManagerGlobal = Wayland::Global<AppMenuManager, AppMenuManagerVersion>;

class AppMenuManager::Private : public AppMenuManagerGlobal
{
public:
    Private(D_isplay* display, AppMenuManager* qpt);
    ~Private() override;

    std::vector<AppMenu*> appmenus;

private:
    static void
    createCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id, wl_resource* surface);

    static const struct org_kde_kwin_appmenu_manager_interface s_interface;
};

class AppMenu::Private : public Wayland::Resource<AppMenu>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Surface* surface, AppMenu* qptr);
    ~Private() override;

    Surface* surface;
    InterfaceAddress address;

private:
    static void setAddressCallback(wl_client* wlClient,
                                   wl_resource* wlResource,
                                   const char* service_name,
                                   const char* object_path);

    static const struct org_kde_kwin_appmenu_interface s_interface;
};
}
