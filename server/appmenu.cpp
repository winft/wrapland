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

const struct org_kde_kwin_appmenu_manager_interface AppmenuManager::Private::s_interface = {
    cb<createCallback>,
};

AppmenuManager::Private::Private(Display* display, AppmenuManager* qptr)
    : AppmenuManagerGlobal(qptr, display, &org_kde_kwin_appmenu_manager_interface, &s_interface)
{
    create();
}

AppmenuManager::Private::~Private() = default;

AppmenuManager::AppmenuManager(Display* display)
    : d_ptr(new Private(display, this))
{
}

AppmenuManager::~AppmenuManager() = default;

void AppmenuManager::Private::createCallback(AppmenuManagerBind* bind,
                                             uint32_t id,
                                             wl_resource* wlSurface)
{
    auto priv = bind->global()->handle()->d_ptr.get();
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);

    auto appmenu = new Appmenu(bind->client()->handle(), bind->version(), id, surface);

    if (!appmenu->d_ptr->resource()) {
        bind->post_no_memory();
        delete appmenu;
        return;
    }
    priv->appmenus.push_back(appmenu);

    QObject::connect(appmenu, &Appmenu::resourceDestroyed, priv->handle(), [=]() {
        priv->appmenus.erase(std::remove(priv->appmenus.begin(), priv->appmenus.end(), appmenu),
                             priv->appmenus.end());
    });

    Q_EMIT priv->handle()->appmenuCreated(appmenu);
}

const struct org_kde_kwin_appmenu_interface Appmenu::Private::s_interface = {
    setAddressCallback,
    destroyCallback,
};

Appmenu::Private::Private(Client* client,
                          uint32_t version,
                          uint32_t id,
                          Surface* surface,
                          Appmenu* qptr)
    : Wayland::Resource<Appmenu>(client,
                                 version,
                                 id,
                                 &org_kde_kwin_appmenu_interface,
                                 &s_interface,
                                 qptr)
    , surface(surface)
{
}

Appmenu::Private::~Private() = default;

Appmenu::Appmenu(Client* client, uint32_t version, uint32_t id, Surface* surface)
    : d_ptr(new Private(client, version, id, surface, this))
{
}

void Appmenu::Private::setAddressCallback([[maybe_unused]] wl_client* wlClient,
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

Appmenu* AppmenuManager::appmenuForSurface(Surface* surface)
{
    for (auto menu : d_ptr->appmenus) {
        if (menu->surface() == surface) {
            return menu;
        }
    }
    return nullptr;
}

Appmenu::InterfaceAddress Appmenu::address() const
{
    return d_ptr->address;
}

Surface* Appmenu::surface() const
{
    return d_ptr->surface;
}
}
