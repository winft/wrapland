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

class Display;
class Surface;
class Appmenu;

constexpr uint32_t AppmenuManagerVersion = 1;
using AppmenuManagerGlobal = Wayland::Global<AppmenuManager, AppmenuManagerVersion>;
using AppmenuManagerBind = Wayland::Bind<AppmenuManagerGlobal>;

class AppmenuManager::Private : public AppmenuManagerGlobal
{
public:
    Private(Display* display, AppmenuManager* qpt);

    std::vector<Appmenu*> appmenus;

private:
    static void createCallback(AppmenuManagerBind* bind, uint32_t id, wl_resource* surface);

    static const struct org_kde_kwin_appmenu_manager_interface s_interface;
};

class Appmenu::Private : public Wayland::Resource<Appmenu>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Surface* surface, Appmenu* qptr);
    ~Private() override;

    Surface* surface;
    InterfaceAddress address;

private:
    static void setAddressCallback(wl_client* wlClient,
                                   wl_resource* wlResource,
                                   char const* service_name,
                                   char const* object_path);

    static const struct org_kde_kwin_appmenu_interface s_interface;
};
}
