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
#include "wayland/client.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "appmenu_p.h"
#include "client.h"
#include "display.h"
#include "surface.h"
#include "surface_p.h"

#include <QLoggingCategory>
#include <QtGlobal>

namespace Wrapland::Server
{

const struct org_kde_kwin_appmenu_manager_interface AppMenuManager::Private::s_interface = {
    createCallback,
};

AppMenuManager::Private::Private(D_isplay* display, AppMenuManager* qptr)
    : AppMenuManagerGlobal(qptr, display, &org_kde_kwin_appmenu_manager_interface, &s_interface)
{
    create();
}

AppMenuManager::Private::~Private() = default;

AppMenuManager::AppMenuManager(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

AppMenuManager::~AppMenuManager() = default;

void AppMenuManager::Private::createCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             uint32_t id,
                                             wl_resource* wlSurface)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);

    auto appmenu = new AppMenu(bind->client()->handle(), bind->version(), id, surface);

    if (!appmenu->d_ptr->resource()) {
        wl_resource_post_no_memory(wlResource);
        delete appmenu;
        return;
    }
    priv->appmenus.push_back(appmenu);

    QObject::connect(appmenu, &AppMenu::resourceDestroyed, priv->handle(), [=]() {
        priv->appmenus.erase(std::remove(priv->appmenus.begin(), priv->appmenus.end(), appmenu),
                             priv->appmenus.end());
    });

    Q_EMIT priv->handle()->appMenuCreated(appmenu);
}

const struct org_kde_kwin_appmenu_interface AppMenu::Private::s_interface = {
    setAddressCallback,
    destroyCallback,
};

AppMenu::Private::Private(Client* client,
                          uint32_t version,
                          uint32_t id,
                          Surface* surface,
                          AppMenu* qptr)
    : Wayland::Resource<AppMenu>(client,
                                 version,
                                 id,
                                 &org_kde_kwin_appmenu_interface,
                                 &s_interface,
                                 qptr)
    , surface(surface)
{
}

AppMenu::Private::~Private() = default;

AppMenu::AppMenu(Client* client, uint32_t version, uint32_t id, Surface* surface)
    : d_ptr(new Private(client, version, id, surface, this))
{
}

void AppMenu::Private::setAddressCallback([[maybe_unused]] wl_client* wlClient,
                                          wl_resource* wlResource,
                                          const char* service_name,
                                          const char* object_path)
{
    auto priv = handle(wlResource)->d_ptr;

    if (priv->address.serviceName == QLatin1String(service_name)
        && priv->address.objectPath == QLatin1String(object_path)) {
        return;
    }

    priv->address.serviceName = QString::fromLatin1(service_name);
    priv->address.objectPath = QString::fromLatin1(object_path);
    Q_EMIT priv->handle()->addressChanged(priv->address);
}

AppMenu* AppMenuManager::appMenuForSurface(Surface* surface)
{
    for (AppMenu* menu : d_ptr->appmenus) {
        if (menu->surface() == surface) {
            return menu;
        }
    }
    return nullptr;
}

AppMenu::InterfaceAddress AppMenu::address() const
{
    return d_ptr->address;
}

Surface* AppMenu::surface() const
{
    return d_ptr->surface;
}
}
