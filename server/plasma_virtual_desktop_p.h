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
#pragma once

#include "plasma_virtual_desktop.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-plasma-virtual-desktop-server-protocol.h>

#include <memory>

namespace Wrapland::Server
{

class D_isplay;
class PlasmaVirtualDesktop;

constexpr uint32_t PlasmaVirtualDesktopManagerVersion = 2;
using PlasmaVirtualDesktopManagerGlobal
    = Wayland::Global<PlasmaVirtualDesktopManager, PlasmaVirtualDesktopManagerVersion>;
using PlasmaVirtualDesktopManagerBind
    = Wayland::Resource<PlasmaVirtualDesktopManager, PlasmaVirtualDesktopManagerGlobal>;

class PlasmaVirtualDesktopManager::Private : public PlasmaVirtualDesktopManagerGlobal
{
public:
    Private(D_isplay* display, PlasmaVirtualDesktopManager* q);

    void bindInit(PlasmaVirtualDesktopManagerBind* bind) override;

    uint32_t rows = 0;
    uint32_t columns = 0;

    QList<PlasmaVirtualDesktop*> desktops;

    QList<PlasmaVirtualDesktop*>::const_iterator constFindDesktop(const QString& id);
    QList<PlasmaVirtualDesktop*>::iterator findDesktop(const QString& id);

private:
    static void getVirtualDesktopCallback(wl_client* client,
                                          wl_resource* resource,
                                          uint32_t serial,
                                          const char* id);
    static void requestCreateVirtualDesktopCallback(wl_client* client,
                                                    wl_resource* resource,
                                                    const char* name,
                                                    uint32_t position);
    static void
    requestRemoveVirtualDesktopCallback(wl_client* client, wl_resource* resource, const char* id);

    static const struct org_kde_plasma_virtual_desktop_management_interface s_interface;
};

class PlasmaVirtualDesktopRes;

class PlasmaVirtualDesktop::Private
{
public:
    Private(PlasmaVirtualDesktop* qptr, PlasmaVirtualDesktopManager* c);
    ~Private();

    void createResource(Wayland::Client* client, uint32_t version, uint32_t serial);

    QVector<PlasmaVirtualDesktopRes*> resources;
    QString id;
    QString name;
    bool active = false;

    PlasmaVirtualDesktopManager* manager;

private:
    PlasmaVirtualDesktop* q_ptr;
};

class PlasmaVirtualDesktopRes : public QObject
{
    Q_OBJECT
public:
    PlasmaVirtualDesktopRes(Client* client,
                            uint32_t version,
                            uint32_t id,
                            PlasmaVirtualDesktop* parent);

    void activated();
    void done();

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class PlasmaVirtualDesktopManager;
    friend class PlasmaVirtualDesktop;

    class Private;
    Private* d_ptr;
};

class PlasmaVirtualDesktopRes::Private : public Wayland::Resource<PlasmaVirtualDesktopRes>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            PlasmaVirtualDesktop* virtualDesktop,
            PlasmaVirtualDesktopRes* q);

    PlasmaVirtualDesktop* virtualDesktop;

private:
    static void requestActivateCallback(wl_client* wlClient, wl_resource* wlResource);
    static const struct org_kde_plasma_virtual_desktop_interface s_interface;
};

}
