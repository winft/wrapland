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

constexpr uint32_t PlasmaWindowManagerVersion = 16;
using PlasmaWindowManagerGlobal = Wayland::Global<PlasmaWindowManager, PlasmaWindowManagerVersion>;

class PlasmaWindowManager::Private : public PlasmaWindowManagerGlobal
{
public:
    Private(Display* display, PlasmaWindowManager* q_ptr);

    void sendShowingDesktopState();
    void send_stacking_order_changed();
    void send_stacking_order_changed(PlasmaWindowManagerGlobal::bind_t* bind);
    void send_stacking_order_uuid_changed();
    void send_stacking_order_uuid_changed(PlasmaWindowManagerGlobal::bind_t* bind);

    void bindInit(PlasmaWindowManagerGlobal::bind_t* bind) override;

    ShowingDesktopState desktopState = ShowingDesktopState::Disabled;
    std::vector<PlasmaWindow*> windows;
    std::vector<uint32_t> id_stack;
    std::vector<std::string> uuid_stack;
    PlasmaVirtualDesktopManager* virtualDesktopManager = nullptr;
    uint32_t windowIdCounter = 0;

private:
    static void
    showDesktopCallback(wl_client* client, wl_resource* resource, uint32_t desktopState);
    static void getWindowCallback(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  uint32_t internalWindowId);
    static void get_window_by_uuid_callback(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t id,
                                            char const* uuid);

    static const struct org_kde_plasma_window_management_interface s_interface;
};

class PlasmaWindow::Private
{
public:
    Private(PlasmaWindowManager* manager, PlasmaWindow* q_ptr);
    ~Private();

    void createResource(uint32_t version, uint32_t id, Wayland::Client* client, bool temporary);
    void setTitle(QString const& title);
    void setAppId(QString const& appId);
    void setPid(uint32_t pid);
    void setThemedIconName(QString const& iconName);
    void setIcon(QIcon const& icon);
    void setState(org_kde_plasma_window_management_state flag, bool set);
    void setParentWindow(PlasmaWindow* window);
    void setGeometry(QRect const& geometry);
    void setApplicationMenuPaths(QString const& serviceName, QString const& objectPath);
    void set_resource_name(std::string const& resource_name);

    // TODO(romangg): Might make sense to have this as a non-static member function instead.
    static PlasmaWindowRes* getResourceOfParent(PlasmaWindow* parent, PlasmaWindowRes* childRes);

    std::vector<PlasmaWindowRes*> resources;
    std::string uuid;
    uint32_t windowId = 0;
    QHash<Surface*, QRect> minimizedGeometries;
    PlasmaWindowManager* manager;

    PlasmaWindow* parentWindow = nullptr;
    QMetaObject::Connection parentWindowDestroyConnection;
    std::vector<std::string> plasmaVirtualDesktops;
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
    struct {
        QString serviceName;
        QString objectPath;
    } m_applicationMenu;
    std::string resource_name;
};

class PlasmaWindowRes : public QObject
{
    Q_OBJECT
public:
    PlasmaWindowRes(Wayland::Client* client, uint32_t version, uint32_t id, PlasmaWindow* window);

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
            PlasmaWindowRes* q_ptr);

    PlasmaWindow* window;

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
                                             uint32_t pos_x,
                                             uint32_t pos_y,
                                             uint32_t width,
                                             uint32_t height);
    static void
    unsetMinimizedGeometryCallback(wl_client* client, wl_resource* resource, wl_resource* wlPanel);
    static void getIconCallback(wl_client* client, wl_resource* resource, int32_t fd);
    static void
    requestEnterVirtualDesktopCallback(wl_client* client, wl_resource* resource, char const* id);
    static void requestEnterNewVirtualDesktopCallback(wl_client* client, wl_resource* resource);
    static void
    requestLeaveVirtualDesktopCallback(wl_client* client, wl_resource* resource, char const* id);
    static void
    request_enter_activity_callback(wl_client* client, wl_resource* resource, char const* id);
    static void
    request_leave_activity_callback(wl_client* client, wl_resource* resource, char const* id);
    static void
    send_to_output_callback(wl_client* client, wl_resource* resource, wl_resource* output);

    static const struct org_kde_plasma_window_interface s_interface;
};

}
