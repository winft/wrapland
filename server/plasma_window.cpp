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
#include "plasma_window_p.h"

#include "display.h"
#include "plasma_virtual_desktop.h"
#include "surface.h"

#include <QFile>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QRect>
#include <QVector>
#include <QtConcurrentRun>

#include <signal.h>
#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_plasma_window_management_interface PlasmaWindowManager::Private::s_interface
    = {
        showDesktopCallback,
        getWindowCallback,
};

PlasmaWindowManager::Private::Private(Display* display, PlasmaWindowManager* qptr)
    : PlasmaWindowManagerGlobal(qptr,
                                display,
                                &org_kde_plasma_window_management_interface,
                                &s_interface)
{
    create();
}

PlasmaWindowManager::PlasmaWindowManager(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
    // Needed because the icon is sent via a pipe and when it closes while being written to would
    // kill off the compositor.
    // TODO(romangg): Replace the pipe with a Unix domain socket and set on it to ignore the SIGPIPE
    //                signal. See issue #7.
    signal(SIGPIPE, SIG_IGN);
}

PlasmaWindowManager::~PlasmaWindowManager() = default;

void PlasmaWindowManager::Private::bindInit(PlasmaWindowManagerBind* bind)
{
    for (auto it = windows.constBegin(); it != windows.constEnd(); ++it) {
        send<org_kde_plasma_window_management_send_window>(bind, (*it)->d_ptr->windowId);
    }
}

void PlasmaWindowManager::Private::sendShowingDesktopState()
{
    uint32_t state = 0;
    switch (desktopState) {
    case ShowingDesktopState::Enabled:
        state = ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED;
        break;
    case ShowingDesktopState::Disabled:
        state = ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    send<org_kde_plasma_window_management_send_show_desktop_changed>(state);
}

void PlasmaWindowManager::Private::showDesktopCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       uint32_t desktopState)
{
    auto state = ShowingDesktopState::Disabled;

    switch (desktopState) {
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED:
        state = ShowingDesktopState::Enabled;
        break;
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED:
    default:
        state = ShowingDesktopState::Disabled;
        break;
    }

    Q_EMIT handle(wlResource)->requestChangeShowingDesktop(state);
}

void PlasmaWindowManager::Private::getWindowCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource,
                                                     uint32_t id,
                                                     uint32_t internalWindowId)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    auto it = std::find_if(priv->windows.constBegin(),
                           priv->windows.constEnd(),
                           [internalWindowId](PlasmaWindow* window) {
                               return window->d_ptr->windowId == internalWindowId;
                           });

    if (it == priv->windows.constEnd()) {
        // Create a temp window just for the resource and directly send unmapped.
        auto window = std::unique_ptr<PlasmaWindow>(new PlasmaWindow(priv->handle()));
        window->d_ptr->createResource(bind->version(), id, bind->client(), true);
        return;
    }
    (*it)->d_ptr->createResource(bind->version(), id, bind->client(), false);
}

void PlasmaWindowManager::setShowingDesktopState(ShowingDesktopState desktopState)
{
    if (d_ptr->desktopState == desktopState) {
        return;
    }
    d_ptr->desktopState = desktopState;
    d_ptr->sendShowingDesktopState();
}

PlasmaWindow* PlasmaWindowManager::createWindow(QObject* parent)
{
    auto window = new PlasmaWindow(this, parent);

    // TODO(unknown author): improve window ids so that it cannot wrap around
    window->d_ptr->windowId = ++d_ptr->windowIdCounter;

    d_ptr->send<org_kde_plasma_window_management_send_window>(window->d_ptr->windowId);

    d_ptr->windows << window;
    connect(
        window, &QObject::destroyed, this, [this, window] { d_ptr->windows.removeAll(window); });

    return window;
}

QList<PlasmaWindow*> PlasmaWindowManager::windows() const
{
    return d_ptr->windows;
}

void PlasmaWindowManager::unmapWindow(PlasmaWindow* window)
{
    if (!window) {
        return;
    }

    d_ptr->windows.removeOne(window);
    Q_ASSERT(!d_ptr->windows.contains(window));

    window->d_ptr->unmap();
    delete window;
}

void PlasmaWindowManager::setVirtualDesktopManager(PlasmaVirtualDesktopManager* manager)
{
    d_ptr->virtualDesktopManager = manager;
}

PlasmaVirtualDesktopManager* PlasmaWindowManager::virtualDesktopManager() const
{
    return d_ptr->virtualDesktopManager;
}

/////////////////////////// Plasma Window ///////////////////////////

PlasmaWindow::Private::Private(PlasmaWindowManager* manager, PlasmaWindow* q)
    : manager(manager)
    , q_ptr(q)
{
}

PlasmaWindow::Private::~Private()
{
    for (auto resource : resources) {
        resource->unmap();
    }
}

PlasmaWindow::PlasmaWindow(PlasmaWindowManager* manager, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(manager, this))
{
}

PlasmaWindow::~PlasmaWindow() = default;

void PlasmaWindow::Private::createResource(uint32_t version,
                                           uint32_t id,
                                           Wayland::Client* client,
                                           bool temporary)
{
    auto windowRes = new PlasmaWindowRes(client, version, id, temporary ? nullptr : q_ptr);
    resources << windowRes;

    connect(windowRes, &PlasmaWindowRes::resourceDestroyed, q_ptr, [this, windowRes]() {
        resources.removeOne(windowRes);
    });

    windowRes->d_ptr->send<org_kde_plasma_window_send_virtual_desktop_changed>(m_virtualDesktop);
    for (const auto& desk : plasmaVirtualDesktops) {
        windowRes->d_ptr->send<org_kde_plasma_window_send_virtual_desktop_entered>(
            desk.toUtf8().constData());
    }
    if (!m_appId.isEmpty()) {
        windowRes->d_ptr->send<org_kde_plasma_window_send_app_id_changed>(
            m_appId.toUtf8().constData());
    }
    if (m_pid != 0) {
        windowRes->d_ptr->send<org_kde_plasma_window_send_pid_changed>(m_pid);
    }
    if (!m_title.isEmpty()) {
        windowRes->d_ptr->send<org_kde_plasma_window_send_title_changed>(
            m_title.toUtf8().constData());
    }
    windowRes->d_ptr->send<org_kde_plasma_window_send_state_changed>(m_desktopState);
    if (!m_themedIconName.isEmpty()) {
        windowRes->d_ptr->send<org_kde_plasma_window_send_themed_icon_name_changed>(
            m_themedIconName.toUtf8().constData());
    } else {
        if (version >= ORG_KDE_PLASMA_WINDOW_ICON_CHANGED_SINCE_VERSION) {
            windowRes->d_ptr->send<org_kde_plasma_window_send_icon_changed>();
        }
    }

    auto parentRes = getResourceOfParent(parentWindow, windowRes);
    windowRes->d_ptr->send<org_kde_plasma_window_send_parent_window>(
        parentRes ? parentRes->d_ptr->resource() : nullptr);

    if (temporary) {
        windowRes->d_ptr->send<org_kde_plasma_window_send_unmapped>();
    }

    if (geometry.isValid() && version >= ORG_KDE_PLASMA_WINDOW_GEOMETRY_SINCE_VERSION) {
        windowRes->d_ptr->send<org_kde_plasma_window_send_geometry>(
            geometry.x(), geometry.y(), geometry.width(), geometry.height());
    }

    if (version >= ORG_KDE_PLASMA_WINDOW_INITIAL_STATE_SINCE_VERSION) {
        windowRes->d_ptr->send<org_kde_plasma_window_send_initial_state>();
    }
    client->flush();
}

void PlasmaWindow::Private::setAppId(const QString& appId)
{
    if (m_appId == appId) {
        return;
    }
    m_appId = appId;
    const QByteArray utf8 = m_appId.toUtf8();
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        (*it)->d_ptr->send<org_kde_plasma_window_send_app_id_changed>(utf8.constData());
    }
}

void PlasmaWindow::Private::setPid(uint32_t pid)
{
    if (m_pid == pid) {
        return;
    }
    m_pid = pid;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        (*it)->d_ptr->send<org_kde_plasma_window_send_pid_changed>(pid);
    }
}

void PlasmaWindow::Private::setThemedIconName(const QString& iconName)
{
    if (m_themedIconName == iconName) {
        return;
    }
    m_themedIconName = iconName;
    const QByteArray utf8 = m_themedIconName.toUtf8();
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        (*it)->d_ptr->send<org_kde_plasma_window_send_themed_icon_name_changed>(utf8.constData());
    }
}

void PlasmaWindow::Private::setIcon(const QIcon& icon)
{
    m_icon = icon;
    setThemedIconName(m_icon.name());
    if (m_icon.name().isEmpty()) {
        for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
            if (wl_resource_get_version((*it)->d_ptr->resource())
                >= ORG_KDE_PLASMA_WINDOW_ICON_CHANGED_SINCE_VERSION) {
                (*it)->d_ptr->send<org_kde_plasma_window_send_icon_changed>();
            }
        }
    }
}

void PlasmaWindow::Private::setTitle(const QString& title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    const QByteArray utf8 = m_title.toUtf8();
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        (*it)->d_ptr->send<org_kde_plasma_window_send_title_changed>(utf8.constData());
    }
}

void PlasmaWindow::Private::unmap()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        (*it)->unmap();
    }
}

void PlasmaWindow::Private::setState(org_kde_plasma_window_management_state flag, bool set)
{
    uint32_t newState = m_desktopState;
    if (set) {
        newState |= flag;
    } else {
        newState &= ~flag;
    }
    if (newState == m_desktopState) {
        return;
    }
    m_desktopState = newState;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        (*it)->d_ptr->send<org_kde_plasma_window_send_state_changed>(m_desktopState);
    }
}

PlasmaWindowRes* PlasmaWindow::Private::getResourceOfParent(PlasmaWindow* parent,
                                                            PlasmaWindowRes* childRes)
{
    if (!parent) {
        return nullptr;
    }

    auto childClient = childRes->d_ptr->client();
    auto it = std::find_if(parent->d_ptr->resources.begin(),
                           parent->d_ptr->resources.end(),
                           [childClient](PlasmaWindowRes* parentRes) {
                               return parentRes->d_ptr->client() == childClient;
                           });
    return it != parent->d_ptr->resources.end() ? *it : nullptr;
}

void PlasmaWindow::Private::setParentWindow(PlasmaWindow* window)
{
    if (parentWindow == window) {
        return;
    }
    QObject::disconnect(parentWindowDestroyConnection);
    parentWindowDestroyConnection = QMetaObject::Connection();
    parentWindow = window;
    if (parentWindow) {
        parentWindowDestroyConnection
            = QObject::connect(window, &QObject::destroyed, q_ptr, [this] {
                  parentWindow = nullptr;
                  parentWindowDestroyConnection = QMetaObject::Connection();
                  for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
                      (*it)->d_ptr->send<org_kde_plasma_window_send_parent_window>(nullptr);
                  }
              });
    }
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        auto parentRes = getResourceOfParent(window, *it);
        (*it)->d_ptr->send<org_kde_plasma_window_send_parent_window>(
            parentRes ? parentRes->d_ptr->resource() : nullptr);
    }
}

void PlasmaWindow::Private::setGeometry(const QRect& geo)
{
    if (geometry == geo) {
        return;
    }
    geometry = geo;
    if (!geometry.isValid()) {
        return;
    }
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        auto resource = (*it)->d_ptr->resource();
        if (wl_resource_get_version(resource) < ORG_KDE_PLASMA_WINDOW_GEOMETRY_SINCE_VERSION) {
            continue;
        }
        (*it)->d_ptr->send<org_kde_plasma_window_send_geometry>(
            geometry.x(), geometry.y(), geometry.width(), geometry.height());
    }
}

void PlasmaWindow::setAppId(const QString& appId)
{
    d_ptr->setAppId(appId);
}

void PlasmaWindow::setPid(uint32_t pid)
{
    d_ptr->setPid(pid);
}

void PlasmaWindow::setTitle(const QString& title)
{
    d_ptr->setTitle(title);
}

void PlasmaWindow::unmap()
{
    d_ptr->manager->unmapWindow(this);
}

QHash<Surface*, QRect> PlasmaWindow::minimizedGeometries() const
{
    return d_ptr->minimizedGeometries;
}

void PlasmaWindow::setActive(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE, set);
}

void PlasmaWindow::setFullscreen(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN, set);
}

void PlasmaWindow::setKeepAbove(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE, set);
}

void PlasmaWindow::setKeepBelow(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW, set);
}

void PlasmaWindow::setMaximized(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED, set);
}

void PlasmaWindow::setMinimized(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED, set);
}

void PlasmaWindow::setOnAllDesktops(bool set)
{
    // the deprecated vd management
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ON_ALL_DESKTOPS, set);

    if (!d_ptr->manager->virtualDesktopManager()) {
        return;
    }

    // the current vd management
    if (set) {
        if (d_ptr->plasmaVirtualDesktops.isEmpty()) {
            return;
        }
        // leaving everything means on all desktops
        for (auto desk : plasmaVirtualDesktops()) {
            for (auto it = d_ptr->resources.constBegin(); it != d_ptr->resources.constEnd(); ++it) {
                (*it)->d_ptr->send<org_kde_plasma_window_send_virtual_desktop_left>(
                    desk.toUtf8().constData());
            }
        }
        d_ptr->plasmaVirtualDesktops.clear();
    } else {
        if (!d_ptr->plasmaVirtualDesktops.isEmpty()) {
            return;
        }
        // enters the desktops which are active (usually only one  but not a given)
        for (auto desk : d_ptr->manager->virtualDesktopManager()->desktops()) {
            if (desk->active() && !d_ptr->plasmaVirtualDesktops.contains(desk->id())) {
                d_ptr->plasmaVirtualDesktops << desk->id();
                for (auto it = d_ptr->resources.constBegin(); it != d_ptr->resources.constEnd();
                     ++it) {
                    (*it)->d_ptr->send<org_kde_plasma_window_send_virtual_desktop_entered>(
                        desk->id().toUtf8().constData());
                }
            }
        }
    }
}

void PlasmaWindow::setDemandsAttention(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION, set);
}

void PlasmaWindow::setCloseable(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE, set);
}

void PlasmaWindow::setFullscreenable(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE, set);
}

void PlasmaWindow::setMaximizeable(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE, set);
}

void PlasmaWindow::setMinimizeable(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE, set);
}

void PlasmaWindow::setSkipTaskbar(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR, set);
}

void PlasmaWindow::setSkipSwitcher(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER, set);
}

void PlasmaWindow::setIcon(const QIcon& icon)
{
    d_ptr->setIcon(icon);
}

void PlasmaWindow::addPlasmaVirtualDesktop(const QString& id)
{
    // don't add a desktop we're not sure it exists
    if (!d_ptr->manager->virtualDesktopManager() || d_ptr->plasmaVirtualDesktops.contains(id)) {
        return;
    }

    PlasmaVirtualDesktop* desktop = d_ptr->manager->virtualDesktopManager()->desktop(id);

    if (!desktop) {
        return;
    }

    d_ptr->plasmaVirtualDesktops << id;

    // if the desktop dies, remove it from or list
    connect(desktop, &QObject::destroyed, this, [this, id]() { removePlasmaVirtualDesktop(id); });

    for (auto it = d_ptr->resources.constBegin(); it != d_ptr->resources.constEnd(); ++it) {
        (*it)->d_ptr->send<org_kde_plasma_window_send_virtual_desktop_entered>(
            id.toUtf8().constData());
    }
}

void PlasmaWindow::removePlasmaVirtualDesktop(const QString& id)
{
    if (!d_ptr->plasmaVirtualDesktops.contains(id)) {
        return;
    }

    d_ptr->plasmaVirtualDesktops.removeAll(id);
    for (auto it = d_ptr->resources.constBegin(); it != d_ptr->resources.constEnd(); ++it) {
        (*it)->d_ptr->send<org_kde_plasma_window_send_virtual_desktop_left>(
            id.toUtf8().constData());
    }

    // we went on all desktops
    if (d_ptr->plasmaVirtualDesktops.isEmpty()) {
        setOnAllDesktops(true);
    }
}

QStringList PlasmaWindow::plasmaVirtualDesktops() const
{
    return d_ptr->plasmaVirtualDesktops;
}

void PlasmaWindow::setShadeable(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE, set);
}

void PlasmaWindow::setShaded(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED, set);
}

void PlasmaWindow::setMovable(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE, set);
}

void PlasmaWindow::setResizable(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE, set);
}

void PlasmaWindow::setVirtualDesktopChangeable(bool set)
{
    d_ptr->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE, set);
}

void PlasmaWindow::setParentWindow(PlasmaWindow* parentWindow)
{
    d_ptr->setParentWindow(parentWindow);
}

void PlasmaWindow::setGeometry(const QRect& geometry)
{
    d_ptr->setGeometry(geometry);
}

/////////////////////////// Plasma Window Resource ///////////////////////////

PlasmaWindowRes::Private::Private(Wayland::Client* client,
                                  uint32_t version,
                                  uint32_t id,
                                  PlasmaWindow* window,
                                  PlasmaWindowRes* q)
    : Wayland::Resource<PlasmaWindowRes>(client,
                                         version,
                                         id,
                                         &org_kde_plasma_window_interface,
                                         &s_interface,
                                         q)
    , window(window)
{
}

void PlasmaWindowRes::Private::unmap()
{
    window = nullptr;
    send<org_kde_plasma_window_send_unmapped>();
    client()->flush();
}

const struct org_kde_plasma_window_interface PlasmaWindowRes::Private::s_interface = {
    setStateCallback,
    setVirtualDesktopCallback,
    setMinimizedGeometryCallback,
    unsetMinimizedGeometryCallback,
    closeCallback,
    requestMoveCallback,
    requestResizeCallback,
    destroyCallback,
    getIconCallback,
    requestEnterVirtualDesktopCallback,
    requestEnterNewVirtualDesktopCallback,
    requestLeaveVirtualDesktopCallback,
};

void PlasmaWindowRes::Private::getIconCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               int32_t fd)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }
    QtConcurrent::run(
        [fd](const QIcon& icon) {
            QFile file;
            file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle);
            QDataStream ds(&file);
            ds << icon;
            file.close();
        },
        priv->window->d_ptr->m_icon);
}

void PlasmaWindowRes::Private::requestEnterVirtualDesktopCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    const char* id)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }
    Q_EMIT priv->window->enterPlasmaVirtualDesktopRequested(QString::fromUtf8(id));
}

void PlasmaWindowRes::Private::requestEnterNewVirtualDesktopCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }
    Q_EMIT priv->window->enterNewPlasmaVirtualDesktopRequested();
}

void PlasmaWindowRes::Private::requestLeaveVirtualDesktopCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    const char* id)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }
    Q_EMIT priv->window->leavePlasmaVirtualDesktopRequested(QString::fromUtf8(id));
}

void PlasmaWindowRes::Private::closeCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }
    Q_EMIT priv->window->closeRequested();
}

void PlasmaWindowRes::Private::requestMoveCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }
    Q_EMIT priv->window->moveRequested();
}

void PlasmaWindowRes::Private::requestResizeCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }
    Q_EMIT priv->window->resizeRequested();
}

void PlasmaWindowRes::Private::setVirtualDesktopCallback([[maybe_unused]] wl_client* wlClient,
                                                         [[maybe_unused]] wl_resource* wlResource,
                                                         [[maybe_unused]] uint32_t number)
{
}

void PlasmaWindowRes::Private::setStateCallback([[maybe_unused]] wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t flags,
                                                uint32_t desktopState)
{
    auto window = handle(wlResource)->d_ptr->window;
    if (!window) {
        return;
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE) {
        Q_EMIT window->activeRequested(desktopState
                                       & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED) {
        Q_EMIT window->minimizedRequested(desktopState
                                          & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED) {
        Q_EMIT window->maximizedRequested(desktopState
                                          & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN) {
        Q_EMIT window->fullscreenRequested(desktopState
                                           & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE) {
        Q_EMIT window->keepAboveRequested(desktopState
                                          & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW) {
        Q_EMIT window->keepBelowRequested(desktopState
                                          & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION) {
        Q_EMIT window->demandsAttentionRequested(
            desktopState & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE) {
        Q_EMIT window->closeableRequested(desktopState
                                          & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE) {
        Q_EMIT window->minimizeableRequested(desktopState
                                             & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE) {
        Q_EMIT window->maximizeableRequested(desktopState
                                             & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE) {
        Q_EMIT window->fullscreenableRequested(
            desktopState & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR) {
        Q_EMIT window->skipTaskbarRequested(desktopState
                                            & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER) {
        Q_EMIT window->skipSwitcherRequested(desktopState
                                             & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE) {
        Q_EMIT window->shadeableRequested(desktopState
                                          & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED) {
        Q_EMIT window->shadedRequested(desktopState
                                       & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE) {
        Q_EMIT window->movableRequested(desktopState
                                        & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE) {
        Q_EMIT window->resizableRequested(desktopState
                                          & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE);
    }

    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE) {
        Q_EMIT window->virtualDesktopChangeableRequested(
            desktopState & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE);
    }
}

void PlasmaWindowRes::Private::setMinimizedGeometryCallback([[maybe_unused]] wl_client* wlClient,
                                                            wl_resource* wlResource,
                                                            wl_resource* wlPanel,
                                                            uint32_t x,
                                                            uint32_t y,
                                                            uint32_t width,
                                                            uint32_t height)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }

    auto panel = Wayland::Resource<Surface>::handle(wlPanel);

    if (priv->window->d_ptr->minimizedGeometries.value(panel) == QRect(x, y, width, height)) {
        return;
    }

    priv->window->d_ptr->minimizedGeometries[panel] = QRect(x, y, width, height);
    Q_EMIT priv->window->minimizedGeometriesChanged();
    connect(panel, &Surface::resourceDestroyed, priv->handle(), [priv, panel]() {
        if (priv->window && priv->window->d_ptr->minimizedGeometries.remove(panel)) {
            Q_EMIT priv->window->minimizedGeometriesChanged();
        }
    });
}

void PlasmaWindowRes::Private::unsetMinimizedGeometryCallback([[maybe_unused]] wl_client* wlClient,
                                                              wl_resource* wlResource,
                                                              wl_resource* wlPanel)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->window) {
        return;
    }

    auto panel = Wayland::Resource<Surface>::handle(wlPanel);

    if (!panel) {
        return;
    }
    if (!priv->window->d_ptr->minimizedGeometries.contains(panel)) {
        return;
    }
    priv->window->d_ptr->minimizedGeometries.remove(panel);
    Q_EMIT priv->window->minimizedGeometriesChanged();
}

PlasmaWindowRes::PlasmaWindowRes(Wayland::Client* client,
                                 uint32_t version,
                                 uint32_t id,
                                 PlasmaWindow* window)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, window, this))
{
}

void PlasmaWindowRes::unmap()
{
    d_ptr->unmap();
}

}
