/****************************************************************************
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
****************************************************************************/
#include "plasma_virtual_desktop_p.h"

#include "wayland/display.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_plasma_virtual_desktop_management_interface
    PlasmaVirtualDesktopManager::Private::s_interface
    = {
        getVirtualDesktopCallback,
        requestCreateVirtualDesktopCallback,
        requestRemoveVirtualDesktopCallback,
};

PlasmaVirtualDesktopManager::Private::Private(D_isplay* display, PlasmaVirtualDesktopManager* q)
    : PlasmaVirtualDesktopManagerGlobal(q,
                                        display,
                                        &org_kde_plasma_virtual_desktop_management_interface,
                                        &s_interface)
{
    create();
}

QList<PlasmaVirtualDesktop*>::const_iterator
PlasmaVirtualDesktopManager::Private::constFindDesktop(const QString& id)
{
    return std::find_if(desktops.constBegin(),
                        desktops.constEnd(),
                        [id](const PlasmaVirtualDesktop* desk) { return desk->id() == id; });
}

QList<PlasmaVirtualDesktop*>::iterator
PlasmaVirtualDesktopManager::Private::findDesktop(const QString& id)
{
    return std::find_if(desktops.begin(), desktops.end(), [id](const PlasmaVirtualDesktop* desk) {
        return desk->id() == id;
    });
}

void PlasmaVirtualDesktopManager::Private::getVirtualDesktopCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    uint32_t serial,
    const char* id)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);
    auto virtualDesktopIt = priv->constFindDesktop(QString::fromUtf8(id));

    if (virtualDesktopIt == priv->desktops.constEnd()) {
        return;
    }

    (*virtualDesktopIt)->d_ptr->createResource(bind->client(), bind->version(), serial);
}

void PlasmaVirtualDesktopManager::Private::requestCreateVirtualDesktopCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    const char* name,
    uint32_t position)
{
    auto manager = handle(wlResource);
    Q_EMIT manager->desktopCreateRequested(
        QString::fromUtf8(name),
        qBound<uint32_t>(0, position, (uint32_t)manager->desktops().count()));
}

void PlasmaVirtualDesktopManager::Private::requestRemoveVirtualDesktopCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    const char* id)
{
    auto manager = handle(wlResource);
    Q_EMIT manager->desktopRemoveRequested(QString::fromUtf8(id));
}

void PlasmaVirtualDesktopManager::Private::bindInit(PlasmaVirtualDesktopManagerBind* bind)
{
    uint32_t i = 0;
    for (auto it = desktops.constBegin(); it != desktops.constEnd(); ++it) {
        bind->send<org_kde_plasma_virtual_desktop_management_send_desktop_created>(
            (*it)->id().toUtf8().constData(), i++);
    }

    bind->send<org_kde_plasma_virtual_desktop_management_send_rows,
               ORG_KDE_PLASMA_VIRTUAL_DESKTOP_MANAGEMENT_ROWS_SINCE_VERSION>(rows);

    bind->send<org_kde_plasma_virtual_desktop_management_send_done>();
}

PlasmaVirtualDesktopManager::PlasmaVirtualDesktopManager(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

PlasmaVirtualDesktopManager::~PlasmaVirtualDesktopManager()
{
    for (auto desktop : d_ptr->desktops) {
        removeDesktop(desktop->id());
    }
}

void PlasmaVirtualDesktopManager::setRows(uint32_t rows)
{
    if (rows == 0 || d_ptr->rows == rows) {
        return;
    }

    d_ptr->rows = rows;
    d_ptr->send<org_kde_plasma_virtual_desktop_management_send_rows>(rows);
}

PlasmaVirtualDesktop* PlasmaVirtualDesktopManager::desktop(const QString& id)
{
    if (auto it = d_ptr->constFindDesktop(id); it != d_ptr->desktops.constEnd()) {
        return *it;
    }
    return nullptr;
}

PlasmaVirtualDesktop* PlasmaVirtualDesktopManager::createDesktop(const QString& id,
                                                                 uint32_t position)
{
    if (auto it = d_ptr->constFindDesktop(id); it != d_ptr->desktops.constEnd()) {
        return *it;
    }

    uint32_t const actualPosition = qMin(position, (uint32_t)d_ptr->desktops.count());

    auto desktop = new PlasmaVirtualDesktop(this);
    desktop->d_ptr->id = id;

    // Activate the first desktop.
    // TODO: to be done here?
    if (d_ptr->desktops.isEmpty()) {
        desktop->d_ptr->active = true;
    }

    d_ptr->desktops.insert(actualPosition, desktop);

    d_ptr->send<org_kde_plasma_virtual_desktop_management_send_desktop_created>(
        id.toUtf8().constData(), actualPosition);

    return desktop;
}

void PlasmaVirtualDesktopManager::removeDesktop(const QString& id)
{
    auto deskIt = d_ptr->findDesktop(id);
    if (deskIt == d_ptr->desktops.end()) {
        return;
    }

    delete *deskIt;
    d_ptr->desktops.erase(deskIt);

    d_ptr->send<org_kde_plasma_virtual_desktop_management_send_desktop_removed>(
        id.toUtf8().constData());
}

QList<PlasmaVirtualDesktop*> PlasmaVirtualDesktopManager::desktops() const
{
    return d_ptr->desktops;
}

void PlasmaVirtualDesktopManager::sendDone()
{
    d_ptr->send<org_kde_plasma_virtual_desktop_management_send_done>();
}

/////////////////////////// Plasma Virtual Desktop ///////////////////////////

PlasmaVirtualDesktop::Private::Private(PlasmaVirtualDesktop* qptr,
                                       PlasmaVirtualDesktopManager* manager)
    : manager(manager)
    , q_ptr(qptr)
{
}

PlasmaVirtualDesktop::Private::~Private()
{
    for (auto resource : resources) {
        resource->d_ptr->send<org_kde_plasma_virtual_desktop_send_removed>();
        resource->d_ptr->virtualDesktop = nullptr;
    }
}

void PlasmaVirtualDesktop::Private::createResource(Wayland::Client* client,
                                                   uint32_t version,
                                                   uint32_t serial)
{
    auto resource = new PlasmaVirtualDesktopRes(client->handle(), version, serial, q_ptr);
    resources << resource;
    connect(resource, &PlasmaVirtualDesktopRes::resourceDestroyed, q_ptr, [this, resource]() {
        resources.removeOne(resource);
    });

    resource->d_ptr->send<org_kde_plasma_virtual_desktop_send_desktop_id>(id.toUtf8().constData());

    if (!name.isEmpty()) {
        resource->d_ptr->send<org_kde_plasma_virtual_desktop_send_name>(name.toUtf8().constData());
    }

    if (active) {
        resource->d_ptr->send<org_kde_plasma_virtual_desktop_send_activated>();
    }

    resource->d_ptr->send<org_kde_plasma_virtual_desktop_send_done>();

    client->flush();
}

PlasmaVirtualDesktop::PlasmaVirtualDesktop(PlasmaVirtualDesktopManager* parent)
    : QObject(parent)
    , d_ptr(new Private(this, parent))
{
}

PlasmaVirtualDesktop::~PlasmaVirtualDesktop() = default;

QString PlasmaVirtualDesktop::id() const
{
    return d_ptr->id;
}

void PlasmaVirtualDesktop::setName(const QString& name)
{
    if (d_ptr->name == name) {
        return;
    }

    d_ptr->name = name;
    for (auto it = d_ptr->resources.constBegin(); it != d_ptr->resources.constEnd(); ++it) {
        (*it)->d_ptr->send<org_kde_plasma_virtual_desktop_send_name>(name.toUtf8().constData());
    }
}

QString PlasmaVirtualDesktop::name() const
{
    return d_ptr->name;
}

void PlasmaVirtualDesktop::setActive(bool active)
{
    if (d_ptr->active == active) {
        return;
    }

    d_ptr->active = active;
    if (active) {
        for (auto it = d_ptr->resources.constBegin(); it != d_ptr->resources.constEnd(); ++it) {
            (*it)->d_ptr->send<org_kde_plasma_virtual_desktop_send_activated>();
        }
    } else {
        for (auto it = d_ptr->resources.constBegin(); it != d_ptr->resources.constEnd(); ++it) {
            (*it)->d_ptr->send<org_kde_plasma_virtual_desktop_send_deactivated>();
        }
    }
}

bool PlasmaVirtualDesktop::active() const
{
    return d_ptr->active;
}

void PlasmaVirtualDesktop::sendDone()
{
    for (auto res : d_ptr->resources) {
        res->d_ptr->send<org_kde_plasma_virtual_desktop_send_done>();
    }
}

/////////////////////////// Plasma Virtual Desktop Resource ///////////////////////////

PlasmaVirtualDesktopRes::Private::Private(Client* client,
                                          uint32_t version,
                                          uint32_t id,
                                          PlasmaVirtualDesktop* virtualDesktop,
                                          PlasmaVirtualDesktopRes* q)
    : Wayland::Resource<PlasmaVirtualDesktopRes>(client,
                                                 version,
                                                 id,
                                                 &org_kde_plasma_virtual_desktop_interface,
                                                 &s_interface,
                                                 q)
    , virtualDesktop(virtualDesktop)
{
}

const struct org_kde_plasma_virtual_desktop_interface PlasmaVirtualDesktopRes::Private::s_interface
    = {requestActivateCallback};

void PlasmaVirtualDesktopRes::Private::requestActivateCallback([[maybe_unused]] wl_client* wlClient,
                                                               wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->virtualDesktop) {
        return;
    }
    Q_EMIT priv->virtualDesktop->activateRequested();
}

PlasmaVirtualDesktopRes::PlasmaVirtualDesktopRes(Client* client,
                                                 uint32_t version,
                                                 uint32_t id,
                                                 PlasmaVirtualDesktop* virtualDesktop)
    : QObject()
    , d_ptr(new Private(client, version, id, virtualDesktop, this))
{
}

}
