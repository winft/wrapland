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

#include "plasma_window.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QIcon>
#include <QObject>

#include <wayland-plasma-window-management-server-protocol.h>

class QSize;

namespace Wrapland::Server
{

class Display;
class PlasmaWindow;
class Surface;
class PlasmaVirtualDesktopManager;
class PlasmaWindowRes;

constexpr uint32_t PlasmaWindowManagerVersion = 9;
using PlasmaWindowManagerGlobal = Wayland::Global<PlasmaWindowManager, PlasmaWindowManagerVersion>;
using PlasmaWindowManagerBind = Wayland::Resource<PlasmaWindowManager, PlasmaWindowManagerGlobal>;

class PlasmaWindowManager::Private : public PlasmaWindowManagerGlobal
{
public:
    Private(Display* display, PlasmaWindowManager* qptr);
    void sendShowingDesktopState();

    void bindInit(PlasmaWindowManagerBind* bind) override;

    ShowingDesktopState desktopState = ShowingDesktopState::Disabled;
    QList<PlasmaWindow*> windows;
    PlasmaVirtualDesktopManager* virtualDesktopManager = nullptr;
    uint32_t windowIdCounter = 0;

private:
    static void
    showDesktopCallback(wl_client* client, wl_resource* resource, uint32_t desktopState);
    static void getWindowCallback(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  uint32_t internalWindowId);

    static const struct org_kde_plasma_window_management_interface s_interface;
};

class PlasmaWindow::Private
{
public:
    Private(PlasmaWindowManager* manager, PlasmaWindow* q);
    ~Private();

    void createResource(uint32_t version, uint32_t id, Wayland::Client* client, bool temporary);
    void setTitle(const QString& title);
    void setAppId(const QString& appId);
    void setPid(uint32_t pid);
    void setThemedIconName(const QString& iconName);
    void setIcon(const QIcon& icon);
    void unmap();
    void setState(org_kde_plasma_window_management_state flag, bool set);
    void setParentWindow(PlasmaWindow* parent);
    void setGeometry(const QRect& geometry);
    PlasmaWindowRes* getResourceOfParent(PlasmaWindow* parent, PlasmaWindowRes* childRes) const;

    QVector<PlasmaWindowRes*> resources;
    uint32_t windowId = 0;
    QHash<Surface*, QRect> minimizedGeometries;
    PlasmaWindowManager* manager;

    PlasmaWindow* parentWindow = nullptr;
    QMetaObject::Connection parentWindowDestroyConnection;
    QStringList plasmaVirtualDesktops;
    QRect geometry;

private:
    friend class PlasmaWindowRes;

    PlasmaWindow* q_ptr;
    QString m_title;
    QString m_appId;
    uint32_t m_pid = 0;
    QString m_themedIconName;
    QIcon m_icon;
    uint32_t m_virtualDesktop = 0;
    uint32_t m_desktopState = 0;
    wl_listener listener;
};

class PlasmaWindowRes : public QObject
{
    Q_OBJECT
public:
    PlasmaWindowRes(Wayland::Client* client, uint32_t version, uint32_t id, PlasmaWindow* parent);

    void unmap();

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class PlasmaWindow;
    class Private;
    Private* d_ptr;
};

class PlasmaWindowRes::Private : public Wayland::Resource<PlasmaWindowRes>
{
public:
    Private(Wayland::Client* client,
            uint32_t version,
            uint32_t id,
            PlasmaWindow* window,
            PlasmaWindowRes* q);

    void unmap();

private:
    static void setStateCallback(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t flags,
                                 uint32_t desktopState);
    static void
    setVirtualDesktopCallback(wl_client* client, wl_resource* resource, uint32_t number);
    static void closeCallback(wl_client* client, wl_resource* resource);
    static void requestMoveCallback(wl_client* client, wl_resource* resource);
    static void requestResizeCallback(wl_client* client, wl_resource* resource);
    static void setMinimizedGeometryCallback(wl_client* client,
                                             wl_resource* resource,
                                             wl_resource* wlPanel,
                                             uint32_t x,
                                             uint32_t y,
                                             uint32_t width,
                                             uint32_t height);
    static void
    unsetMinimizedGeometryCallback(wl_client* client, wl_resource* resource, wl_resource* wlPanel);
    static void getIconCallback(wl_client* client, wl_resource* resource, int32_t fd);
    static void
    requestEnterVirtualDesktopCallback(wl_client* client, wl_resource* resource, const char* id);
    static void requestEnterNewVirtualDesktopCallback(wl_client* client, wl_resource* resource);
    static void
    requestLeaveVirtualDesktopCallback(wl_client* client, wl_resource* resource, const char* id);

    PlasmaWindow* window;
    static const struct org_kde_plasma_window_interface s_interface;
};

}
