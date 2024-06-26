/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2022  Francesco Sorrentino <francesco.sorr@gmail.com>

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
#include "plasmawindowmanagement.h"
#include "event_queue.h"
#include "output.h"
#include "plasmavirtualdesktop.h"
#include "plasmawindowmodel.h"
#include "surface.h"
#include "wayland_pointer_p.h"
// Wayland
#include <wayland-plasma-window-management-client-protocol.h>
#include <wayland-util.h>

#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrentRun>
#include <qplatformdefs.h>

#include <cerrno>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN PlasmaWindowManagement::Private
{
public:
    explicit Private(PlasmaWindowManagement* q);
    WaylandPointer<org_kde_plasma_window_management, org_kde_plasma_window_management_destroy> wm;
    EventQueue* queue = nullptr;
    bool showingDesktop = false;
    QList<PlasmaWindow*> windows;
    std::vector<std::string> stacking_order_uuid;
    std::vector<uint32_t> stacking_order;
    PlasmaWindow* activeWindow = nullptr;

    void setup(org_kde_plasma_window_management* proxy);
    PlasmaWindow*
    windowCreated(org_kde_plasma_window* id, quint32 internalId, std::string const& uuid);

private:
    static void
    showDesktopCallback(void* data, org_kde_plasma_window_management* proxy, uint32_t state);
    static void windowCallback(void* data, org_kde_plasma_window_management* proxy, uint32_t id);
    static void stacking_order_changed_callback(void* data,
                                                org_kde_plasma_window_management* proxy,
                                                wl_array* ids);
    static void stacking_order_uuid_changed_callback(void* data,
                                                     org_kde_plasma_window_management* proxy,
                                                     char const* uuids);
    static void window_with_uuid_callback(void* data,
                                          org_kde_plasma_window_management* proxy,
                                          uint32_t id,
                                          char const* uuid);

    void setShowDesktop(bool set);
    void set_stacking_order(std::vector<uint32_t> const& stack);
    void set_stacking_order_uuids(std::vector<std::string> const& stack);

    org_kde_plasma_window_management_listener static const s_listener;
    PlasmaWindowManagement* q;
};

class Q_DECL_HIDDEN PlasmaWindow::Private
{
public:
    Private(org_kde_plasma_window* window, quint32 internalId, std::string uuid, PlasmaWindow* q);
    WaylandPointer<org_kde_plasma_window, org_kde_plasma_window_destroy> window;
    quint32 internalId;
    std::string uuid;
    std::string resource_name;
    QString title;
    QString appId;
    quint32 desktop = 0;
    bool active = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;
    bool keepAbove = false;
    bool keepBelow = false;
    bool onAllDesktops = false;
    bool demandsAttention = false;
    bool closeable = false;
    bool minimizeable = false;
    bool maximizeable = false;
    bool fullscreenable = false;
    bool skipTaskbar = false;
    bool skipSwitcher = false;
    bool shadeable = false;
    bool shaded = false;
    bool movable = false;
    bool resizable = false;
    bool virtualDesktopChangeable = false;
    QIcon icon;
    PlasmaWindowManagement* wm = nullptr;
    bool unmapped = false;
    QPointer<PlasmaWindow> parentWindow;
    QMetaObject::Connection parentWindowUnmappedConnection;
    QStringList plasmaVirtualDesktops;
    QRect geometry;
    quint32 pid = 0;
    struct {
        QString service_name;
        QString object_path;
    } application_menu;

private:
    static void titleChangedCallback(void* data, org_kde_plasma_window* window, char const* title);
    static void appIdChangedCallback(void* data, org_kde_plasma_window* window, char const* app_id);
    static void pidChangedCallback(void* data, org_kde_plasma_window* window, uint32_t pid);
    static void stateChangedCallback(void* data, org_kde_plasma_window* window, uint32_t state);
    static void
    virtualDesktopChangedCallback(void* data, org_kde_plasma_window* window, int32_t number);
    static void
    themedIconNameChangedCallback(void* data, org_kde_plasma_window* window, char const* name);
    static void unmappedCallback(void* data, org_kde_plasma_window* window);
    static void initialStateCallback(void* data, org_kde_plasma_window* window);
    static void
    parentWindowCallback(void* data, org_kde_plasma_window* window, org_kde_plasma_window* parent);
    static void windowGeometryCallback(void* data,
                                       org_kde_plasma_window* window,
                                       int32_t x,
                                       int32_t y,
                                       uint32_t width,
                                       uint32_t height);
    static void iconChangedCallback(void* data, org_kde_plasma_window* org_kde_plasma_window);
    static void virtualDesktopEnteredCallback(void* data,
                                              org_kde_plasma_window* org_kde_plasma_window,
                                              char const* id);
    static void virtualDesktopLeftCallback(void* data,
                                           org_kde_plasma_window* org_kde_plasma_window,
                                           char const* id);
    static void appmenuChangedCallback(void* data,
                                       org_kde_plasma_window* org_kde_plasma_window,
                                       char const* service_name,
                                       char const* object_path);
    static void
    activity_entered_callback(void* data, org_kde_plasma_window* window, char const* id);
    static void activity_left_callback(void* data, org_kde_plasma_window* window, char const* id);
    static void resource_name_changed_callback(void* data,
                                               org_kde_plasma_window* window,
                                               char const* resource_name);

    void setActive(bool set);
    void setMinimized(bool set);
    void setMaximized(bool set);
    void setFullscreen(bool set);
    void setKeepAbove(bool set);
    void setKeepBelow(bool set);
    void setOnAllDesktops(bool set);
    void setDemandsAttention(bool set);
    void setCloseable(bool set);
    void setMinimizeable(bool set);
    void setMaximizeable(bool set);
    void setFullscreenable(bool set);
    void setSkipTaskbar(bool skip);
    void setSkipSwitcher(bool skip);
    void setShadeable(bool set);
    void setShaded(bool set);
    void setMovable(bool set);
    void setResizable(bool set);
    void setVirtualDesktopChangeable(bool set);
    void setParentWindow(PlasmaWindow* parentWindow);

    static Private* cast(void* data)
    {
        return reinterpret_cast<Private*>(data);
    }

    PlasmaWindow* q;

    org_kde_plasma_window_listener static const s_listener;
};

PlasmaWindowManagement::Private::Private(PlasmaWindowManagement* q)
    : q(q)
{
}

org_kde_plasma_window_management_listener const PlasmaWindowManagement::Private::s_listener = {
    showDesktopCallback,
    windowCallback,
    stacking_order_changed_callback,
    stacking_order_uuid_changed_callback,
    window_with_uuid_callback,
};

void PlasmaWindowManagement::Private::setup(org_kde_plasma_window_management* proxy)
{
    Q_ASSERT(!wm);
    Q_ASSERT(proxy);
    wm.setup(proxy);
    org_kde_plasma_window_management_add_listener(proxy, &s_listener, this);
}

void PlasmaWindowManagement::Private::showDesktopCallback(void* data,
                                                          org_kde_plasma_window_management* proxy,
                                                          uint32_t state)
{
    auto wm = reinterpret_cast<PlasmaWindowManagement::Private*>(data);
    Q_ASSERT(wm->wm == proxy);
    switch (state) {
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED:
        wm->setShowDesktop(true);
        break;
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED:
        wm->setShowDesktop(false);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
}

void PlasmaWindowManagement::Private::setShowDesktop(bool set)
{
    if (showingDesktop == set) {
        return;
    }
    showingDesktop = set;
    Q_EMIT q->showingDesktopChanged(showingDesktop);
}

void PlasmaWindowManagement::Private::windowCallback(void* data,
                                                     org_kde_plasma_window_management* proxy,
                                                     uint32_t id)
{
    auto wm = reinterpret_cast<PlasmaWindowManagement::Private*>(data);
    Q_ASSERT(wm->wm == proxy);

    wm->windowCreated(org_kde_plasma_window_management_get_window(wm->wm, id), id, "unavailable");
}

void PlasmaWindowManagement::Private::stacking_order_changed_callback(
    void* data,
    org_kde_plasma_window_management* proxy,
    wl_array* ids)
{
    auto wm = reinterpret_cast<PlasmaWindowManagement::Private*>(data);
    Q_ASSERT(wm->wm == proxy);

    auto begin = static_cast<uint32_t*>(ids->data);
    auto end = begin + ids->size / sizeof(uint32_t);
    auto stack = std::vector(begin, end);

    wm->set_stacking_order(stack);
}

void PlasmaWindowManagement::Private::set_stacking_order(std::vector<uint32_t> const& stack)
{
    if (stacking_order == stack) {
        return;
    }

    stacking_order = stack;
    Q_EMIT q->stacking_order_changed();
}

void PlasmaWindowManagement::Private::stacking_order_uuid_changed_callback(
    void* data,
    org_kde_plasma_window_management* proxy,
    char const* uuids)
{
    auto wm = reinterpret_cast<PlasmaWindowManagement::Private*>(data);
    Q_ASSERT(wm->wm == proxy);

    // TODO(fsorrent): C++20
    // for (auto const uuid : std::views::split(std::string_view(uuids), ';')) {
    //     stack.push_back(uuid);
    // }

    auto s = std::string(uuids);
    auto stack = std::vector<std::string>();
    size_t pos = 0;
    while ((pos = s.find(';')) != std::string::npos) {
        std::string uuid = s.substr(0, pos);
        stack.push_back(uuid);
        s.erase(0, pos + 1);
    }
    if (!s.empty()) {
        stack.push_back(s);
    }

    wm->set_stacking_order_uuids(stack);
}

void PlasmaWindowManagement::Private::set_stacking_order_uuids(
    std::vector<std::string> const& stack)
{
    if (stacking_order_uuid == stack) {
        return;
    }

    stacking_order_uuid = stack;
    Q_EMIT q->stacking_order_uuid_changed();
}

void PlasmaWindowManagement::Private::window_with_uuid_callback(
    void* data,
    org_kde_plasma_window_management* proxy,
    uint32_t id,
    char const* uuid)
{
    auto wm = reinterpret_cast<PlasmaWindowManagement::Private*>(data);
    Q_ASSERT(wm->wm == proxy);
    Q_ASSERT(uuid != nullptr);

    wm->windowCreated(org_kde_plasma_window_management_get_window_by_uuid(wm->wm, uuid), id, uuid);

    Q_EMIT wm->q->window_with_uuid(uuid);
}

PlasmaWindow* PlasmaWindowManagement::Private::windowCreated(org_kde_plasma_window* id,
                                                             quint32 internalId,
                                                             std::string const& uuid)
{
    if (queue) {
        queue->addProxy(id);
    }
    auto window = new PlasmaWindow(q, id, internalId, uuid);
    window->d->wm = q;
    windows << window;
    QObject::connect(window, &QObject::destroyed, q, [this, window] {
        windows.removeAll(window);
        if (activeWindow == window) {
            activeWindow = nullptr;
            Q_EMIT q->activeWindowChanged();
        }
    });
    QObject::connect(window, &PlasmaWindow::unmapped, q, [this, window] {
        if (activeWindow == window) {
            activeWindow = nullptr;
            Q_EMIT q->activeWindowChanged();
        }
    });
    QObject::connect(window, &PlasmaWindow::activeChanged, q, [this, window] {
        if (window->isActive()) {
            if (activeWindow == window) {
                return;
            }
            activeWindow = window;
            Q_EMIT q->activeWindowChanged();
        } else {
            if (activeWindow == window) {
                activeWindow = nullptr;
                Q_EMIT q->activeWindowChanged();
            }
        }
    });

    return window;
}

PlasmaWindowManagement::PlasmaWindowManagement(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

PlasmaWindowManagement::~PlasmaWindowManagement()
{
    release();
}

void PlasmaWindowManagement::release()
{
    if (!d->wm) {
        return;
    }
    Q_EMIT interfaceAboutToBeReleased();
    d->wm.release();
}

void PlasmaWindowManagement::setup(org_kde_plasma_window_management* shell)
{
    d->setup(shell);
}

void PlasmaWindowManagement::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* PlasmaWindowManagement::eventQueue()
{
    return d->queue;
}

bool PlasmaWindowManagement::isValid() const
{
    return d->wm.isValid();
}

PlasmaWindowManagement::operator org_kde_plasma_window_management*()
{
    return d->wm;
}

PlasmaWindowManagement::operator org_kde_plasma_window_management*() const
{
    return d->wm;
}

void PlasmaWindowManagement::hideDesktop()
{
    setShowingDesktop(false);
}

void PlasmaWindowManagement::showDesktop()
{
    setShowingDesktop(true);
}

void PlasmaWindowManagement::setShowingDesktop(bool show)
{
    org_kde_plasma_window_management_show_desktop(
        d->wm,
        show ? ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED
             : ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED);
}

bool PlasmaWindowManagement::isShowingDesktop() const
{
    return d->showingDesktop;
}

PlasmaWindow* PlasmaWindowManagement::get_window_by_uuid(std::string const& uuid) const
{
    auto proxy = org_kde_plasma_window_management_get_window_by_uuid(d->wm, uuid.data());
    return d->windowCreated(proxy, 0, uuid);
}

QList<PlasmaWindow*> PlasmaWindowManagement::windows() const
{
    return d->windows;
}

std::vector<uint32_t> const& PlasmaWindowManagement::stacking_order() const
{
    return d->stacking_order;
}

std::vector<std::string> const& PlasmaWindowManagement::stacking_order_uuid() const
{
    return d->stacking_order_uuid;
}

PlasmaWindow* PlasmaWindowManagement::activeWindow() const
{
    return d->activeWindow;
}

PlasmaWindowModel* PlasmaWindowManagement::createWindowModel()
{
    return new PlasmaWindowModel(this);
}

org_kde_plasma_window_listener const PlasmaWindow::Private::s_listener = {
    titleChangedCallback,
    appIdChangedCallback,
    stateChangedCallback,
    virtualDesktopChangedCallback,
    themedIconNameChangedCallback,
    unmappedCallback,
    initialStateCallback,
    parentWindowCallback,
    windowGeometryCallback,
    iconChangedCallback,
    pidChangedCallback,
    virtualDesktopEnteredCallback,
    virtualDesktopLeftCallback,
    appmenuChangedCallback,
    activity_entered_callback,
    activity_left_callback,
    resource_name_changed_callback,
};

void PlasmaWindow::Private::activity_entered_callback(void* /*data*/,
                                                      org_kde_plasma_window* /*window*/,
                                                      char const* /*id*/)
{
}

void PlasmaWindow::Private::activity_left_callback(void* /*data*/,
                                                   org_kde_plasma_window* /*window*/,
                                                   char const* /*id*/)
{
}

void PlasmaWindow::Private::parentWindowCallback(void* data,
                                                 org_kde_plasma_window* window,
                                                 org_kde_plasma_window* parent)
{
    Q_UNUSED(window)
    Private* p = cast(data);
    auto const windows = p->wm->windows();
    auto it = std::find_if(windows.constBegin(),
                           windows.constEnd(),
                           [parent](PlasmaWindow const* w) { return *w == parent; });
    p->setParentWindow(it != windows.constEnd() ? *it : nullptr);
}

void PlasmaWindow::Private::windowGeometryCallback(void* data,
                                                   org_kde_plasma_window* window,
                                                   int32_t x,
                                                   int32_t y,
                                                   uint32_t width,
                                                   uint32_t height)
{
    Q_UNUSED(window)
    Private* p = cast(data);
    QRect geo(x, y, width, height);
    if (geo == p->geometry) {
        return;
    }
    p->geometry = geo;
    Q_EMIT p->q->geometryChanged();
}

void PlasmaWindow::Private::setParentWindow(PlasmaWindow* parent)
{
    auto const old = parentWindow;
    QObject::disconnect(parentWindowUnmappedConnection);
    if (parent && !parent->d->unmapped) {
        parentWindow = QPointer<PlasmaWindow>(parent);
        parentWindowUnmappedConnection = QObject::connect(
            parent, &PlasmaWindow::unmapped, q, [this] { setParentWindow(nullptr); });
    } else {
        parentWindow = QPointer<PlasmaWindow>();
        parentWindowUnmappedConnection = QMetaObject::Connection();
    }
    if (parentWindow.data() != old.data()) {
        Q_EMIT q->parentWindowChanged();
    }
}

void PlasmaWindow::Private::initialStateCallback(void* data, org_kde_plasma_window* window)
{
    Q_UNUSED(window)
    Private* p = cast(data);
    if (!p->unmapped) {
        Q_EMIT p->wm->windowCreated(p->q);
    }
}

void PlasmaWindow::Private::titleChangedCallback(void* data,
                                                 org_kde_plasma_window* window,
                                                 char const* title)
{
    Q_UNUSED(window)
    Private* p = cast(data);
    QString const t = QString::fromUtf8(title);
    if (p->title == t) {
        return;
    }
    p->title = t;
    Q_EMIT p->q->titleChanged();
}

void PlasmaWindow::Private::appIdChangedCallback(void* data,
                                                 org_kde_plasma_window* window,
                                                 char const* app_id)
{
    Q_UNUSED(window)
    Private* p = cast(data);
    QString const s = QString::fromUtf8(app_id);
    if (s == p->appId) {
        return;
    }
    p->appId = s;
    Q_EMIT p->q->appIdChanged();
}

void PlasmaWindow::Private::pidChangedCallback(void* data,
                                               org_kde_plasma_window* window,
                                               uint32_t pid)
{
    Q_UNUSED(window)
    Private* p = cast(data);
    if (p->pid == static_cast<quint32>(pid)) {
        return;
    }
    p->pid = pid;
}

void PlasmaWindow::Private::virtualDesktopChangedCallback(void* data,
                                                          org_kde_plasma_window* window,
                                                          int32_t number)
{
    Q_UNUSED(data)
    Q_UNUSED(window)
    Q_UNUSED(number)
}

void PlasmaWindow::Private::appmenuChangedCallback(void* data,
                                                   org_kde_plasma_window* window,
                                                   char const* service_name,
                                                   char const* object_path)
{
    Q_UNUSED(window)

    auto priv = cast(data);

    priv->application_menu.service_name = QString::fromLatin1(service_name);
    priv->application_menu.object_path = QString::fromLatin1(object_path);

    Q_EMIT priv->q->applicationMenuChanged();
}

void PlasmaWindow::Private::unmappedCallback(void* data, org_kde_plasma_window* window)
{
    auto p = cast(data);
    Q_UNUSED(window);
    p->unmapped = true;
    Q_EMIT p->q->unmapped();
    p->q->deleteLater();
}

void PlasmaWindow::Private::virtualDesktopEnteredCallback(void* data,
                                                          org_kde_plasma_window* window,
                                                          char const* id)
{
    auto p = cast(data);
    Q_UNUSED(window);
    QString const stringId(QString::fromUtf8(id));
    p->plasmaVirtualDesktops << stringId;
    Q_EMIT p->q->plasmaVirtualDesktopEntered(stringId);
    if (p->plasmaVirtualDesktops.count() == 1) {
        Q_EMIT p->q->onAllDesktopsChanged();
    }
}

void PlasmaWindow::Private::virtualDesktopLeftCallback(void* data,
                                                       org_kde_plasma_window* window,
                                                       char const* id)
{
    auto p = cast(data);
    Q_UNUSED(window);
    QString const stringId(QString::fromUtf8(id));
    p->plasmaVirtualDesktops.removeAll(stringId);
    Q_EMIT p->q->plasmaVirtualDesktopLeft(stringId);
    if (p->plasmaVirtualDesktops.isEmpty()) {
        Q_EMIT p->q->onAllDesktopsChanged();
    }
}

void PlasmaWindow::Private::stateChangedCallback(void* data,
                                                 org_kde_plasma_window* window,
                                                 uint32_t state)
{
    auto p = cast(data);
    Q_UNUSED(window);
    p->setActive(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
    p->setMinimized(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
    p->setMaximized(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED);
    p->setFullscreen(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN);
    p->setKeepAbove(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE);
    p->setKeepBelow(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW);
    p->setOnAllDesktops(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ON_ALL_DESKTOPS);
    p->setDemandsAttention(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION);
    p->setCloseable(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE);
    p->setFullscreenable(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE);
    p->setMaximizeable(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE);
    p->setMinimizeable(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE);
    p->setSkipTaskbar(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR);
    p->setSkipSwitcher(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER);
    p->setShadeable(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE);
    p->setShaded(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED);
    p->setMovable(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE);
    p->setResizable(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE);
    p->setVirtualDesktopChangeable(
        state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE);
}

void PlasmaWindow::Private::themedIconNameChangedCallback(void* data,
                                                          org_kde_plasma_window* window,
                                                          char const* name)
{
    auto p = cast(data);
    Q_UNUSED(window);
    QString const themedName = QString::fromUtf8(name);
    if (!themedName.isEmpty()) {
        QIcon icon = QIcon::fromTheme(themedName);
        p->icon = icon;
    } else {
        p->icon = QIcon();
    }
    Q_EMIT p->q->iconChanged();
}

void PlasmaWindow::Private::resource_name_changed_callback(void* data,
                                                           org_kde_plasma_window* window,
                                                           char const* resource_name)
{
    Q_UNUSED(window)
    auto priv = reinterpret_cast<Private*>(data);
    if (priv->resource_name == resource_name) {
        return;
    }
    priv->resource_name = resource_name;
    Q_EMIT priv->q->resource_name_changed();
}

static int readData(int fd, QByteArray& data)
{
    // implementation based on QtWayland file qwaylanddataoffer.cpp
    char buf[4096];
    int retryCount = 0;
    int n;
    while (true) {
        n = QT_READ(fd, buf, sizeof buf);
        if (n == -1 && (errno == EAGAIN) && ++retryCount < 1000) {
            usleep(1000);
        } else {
            break;
        }
    }
    if (n > 0) {
        data.append(buf, n);
        n = readData(fd, data);
    }
    return n;
}

void PlasmaWindow::Private::iconChangedCallback(void* data, org_kde_plasma_window* window)
{
    auto p = cast(data);
    Q_UNUSED(window);
    int pipeFds[2];
    if (pipe2(pipeFds, O_CLOEXEC | O_NONBLOCK) != 0) {
        return;
    }
    org_kde_plasma_window_get_icon(p->window, pipeFds[1]);
    close(pipeFds[1]);
    int const pipeFd = pipeFds[0];
    auto readIcon = [pipeFd]() -> QIcon {
        QByteArray content;
        if (readData(pipeFd, content) != 0) {
            close(pipeFd);
            return {};
        }
        close(pipeFd);
        QDataStream ds(content);
        QIcon icon;
        ds >> icon;
        return icon;
    };
    auto watcher = new QFutureWatcher<QIcon>(p->q);
    QObject::connect(watcher, &QFutureWatcher<QIcon>::finished, p->q, [p, watcher] {
        watcher->deleteLater();
        QIcon icon = watcher->result();
        if (!icon.isNull()) {
            p->icon = icon;
        } else {
            p->icon = QIcon::fromTheme(QStringLiteral("wayland"));
        }
        Q_EMIT p->q->iconChanged();
    });
    watcher->setFuture(QtConcurrent::run(readIcon));
}

void PlasmaWindow::Private::setActive(bool set)
{
    if (active == set) {
        return;
    }
    active = set;
    Q_EMIT q->activeChanged();
}

void PlasmaWindow::Private::setFullscreen(bool set)
{
    if (fullscreen == set) {
        return;
    }
    fullscreen = set;
    Q_EMIT q->fullscreenChanged();
}

void PlasmaWindow::Private::setKeepAbove(bool set)
{
    if (keepAbove == set) {
        return;
    }
    keepAbove = set;
    Q_EMIT q->keepAboveChanged();
}

void PlasmaWindow::Private::setKeepBelow(bool set)
{
    if (keepBelow == set) {
        return;
    }
    keepBelow = set;
    Q_EMIT q->keepBelowChanged();
}

void PlasmaWindow::Private::setMaximized(bool set)
{
    if (maximized == set) {
        return;
    }
    maximized = set;
    Q_EMIT q->maximizedChanged();
}

void PlasmaWindow::Private::setMinimized(bool set)
{
    if (minimized == set) {
        return;
    }
    minimized = set;
    Q_EMIT q->minimizedChanged();
}

void PlasmaWindow::Private::setOnAllDesktops(bool set)
{
    if (onAllDesktops == set) {
        return;
    }
    onAllDesktops = set;
    Q_EMIT q->onAllDesktopsChanged();
}

void PlasmaWindow::Private::setDemandsAttention(bool set)
{
    if (demandsAttention == set) {
        return;
    }
    demandsAttention = set;
    Q_EMIT q->demandsAttentionChanged();
}

void PlasmaWindow::Private::setCloseable(bool set)
{
    if (closeable == set) {
        return;
    }
    closeable = set;
    Q_EMIT q->closeableChanged();
}

void PlasmaWindow::Private::setFullscreenable(bool set)
{
    if (fullscreenable == set) {
        return;
    }
    fullscreenable = set;
    Q_EMIT q->fullscreenableChanged();
}

void PlasmaWindow::Private::setMaximizeable(bool set)
{
    if (maximizeable == set) {
        return;
    }
    maximizeable = set;
    Q_EMIT q->maximizeableChanged();
}

void PlasmaWindow::Private::setMinimizeable(bool set)
{
    if (minimizeable == set) {
        return;
    }
    minimizeable = set;
    Q_EMIT q->minimizeableChanged();
}

void PlasmaWindow::Private::setSkipTaskbar(bool skip)
{
    if (skipTaskbar == skip) {
        return;
    }
    skipTaskbar = skip;
    Q_EMIT q->skipTaskbarChanged();
}

void PlasmaWindow::Private::setSkipSwitcher(bool skip)
{
    if (skipSwitcher == skip) {
        return;
    }
    skipSwitcher = skip;
    Q_EMIT q->skipSwitcherChanged();
}

void PlasmaWindow::Private::setShadeable(bool set)
{
    if (shadeable == set) {
        return;
    }
    shadeable = set;
    Q_EMIT q->shadeableChanged();
}

void PlasmaWindow::Private::setShaded(bool set)
{
    if (shaded == set) {
        return;
    }
    shaded = set;
    Q_EMIT q->shadedChanged();
}

void PlasmaWindow::Private::setMovable(bool set)
{
    if (movable == set) {
        return;
    }
    movable = set;
    Q_EMIT q->movableChanged();
}

void PlasmaWindow::Private::setResizable(bool set)
{
    if (resizable == set) {
        return;
    }
    resizable = set;
    Q_EMIT q->resizableChanged();
}

void PlasmaWindow::Private::setVirtualDesktopChangeable(bool set)
{
    if (virtualDesktopChangeable == set) {
        return;
    }
    virtualDesktopChangeable = set;
    Q_EMIT q->virtualDesktopChangeableChanged();
}

PlasmaWindow::Private::Private(org_kde_plasma_window* w,
                               quint32 internalId,
                               std::string uuid,
                               PlasmaWindow* q)
    : internalId(internalId)
    , uuid(std::move(uuid))
    , q(q)
{
    window.setup(w);
    org_kde_plasma_window_add_listener(w, &s_listener, this);
}

PlasmaWindow::PlasmaWindow(PlasmaWindowManagement* parent,
                           org_kde_plasma_window* window,
                           quint32 internalId,
                           std::string const& uuid)
    : QObject(parent)
    , d(new Private(window, internalId, uuid, this))
{
}

PlasmaWindow::~PlasmaWindow()
{
    release();
}

void PlasmaWindow::release()
{
    d->window.release();
}

bool PlasmaWindow::isValid() const
{
    return d->window.isValid();
}

PlasmaWindow::operator org_kde_plasma_window*() const
{
    return d->window;
}

PlasmaWindow::operator org_kde_plasma_window*()
{
    return d->window;
}

QString PlasmaWindow::appId() const
{
    return d->appId;
}

quint32 PlasmaWindow::pid() const
{
    return d->pid;
}

QString PlasmaWindow::title() const
{
    return d->title;
}

bool PlasmaWindow::isActive() const
{
    return d->active;
}

bool PlasmaWindow::isFullscreen() const
{
    return d->fullscreen;
}

bool PlasmaWindow::isKeepAbove() const
{
    return d->keepAbove;
}

bool PlasmaWindow::isKeepBelow() const
{
    return d->keepBelow;
}

bool PlasmaWindow::isMaximized() const
{
    return d->maximized;
}

bool PlasmaWindow::isMinimized() const
{
    return d->minimized;
}

bool PlasmaWindow::isOnAllDesktops() const
{
    // from protocol version 8 virtual desktops are managed by plasmaVirtualDesktops
    if (org_kde_plasma_window_get_version(d->window) < 8) {
        return d->onAllDesktops;
    }

    return d->plasmaVirtualDesktops.isEmpty();
}

bool PlasmaWindow::isDemandingAttention() const
{
    return d->demandsAttention;
}

bool PlasmaWindow::isCloseable() const
{
    return d->closeable;
}

bool PlasmaWindow::isFullscreenable() const
{
    return d->fullscreenable;
}

bool PlasmaWindow::isMaximizeable() const
{
    return d->maximizeable;
}

bool PlasmaWindow::isMinimizeable() const
{
    return d->minimizeable;
}

bool PlasmaWindow::skipTaskbar() const
{
    return d->skipTaskbar;
}

bool PlasmaWindow::skipSwitcher() const
{
    return d->skipSwitcher;
}

QIcon PlasmaWindow::icon() const
{
    return d->icon;
}

bool PlasmaWindow::isShadeable() const
{
    return d->shadeable;
}

bool PlasmaWindow::isShaded() const
{
    return d->shaded;
}

bool PlasmaWindow::isResizable() const
{
    return d->resizable;
}

bool PlasmaWindow::isMovable() const
{
    return d->movable;
}

bool PlasmaWindow::isVirtualDesktopChangeable() const
{
    return d->virtualDesktopChangeable;
}

QString PlasmaWindow::applicationMenuObjectPath() const
{
    return d->application_menu.object_path;
}

QString PlasmaWindow::applicationMenuServiceName() const
{
    return d->application_menu.service_name;
}

void PlasmaWindow::requestActivate()
{
    org_kde_plasma_window_set_state(d->window,
                                    ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE,
                                    ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
}

void PlasmaWindow::requestClose()
{
    org_kde_plasma_window_close(d->window);
}

void PlasmaWindow::requestMove()
{
    org_kde_plasma_window_request_move(d->window);
}

void PlasmaWindow::requestResize()
{
    org_kde_plasma_window_request_resize(d->window);
}

void PlasmaWindow::requestToggleKeepAbove()
{
    if (d->keepAbove) {
        org_kde_plasma_window_set_state(
            d->window, ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE, 0);
    } else {
        org_kde_plasma_window_set_state(d->window,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE);
    }
}

void PlasmaWindow::requestToggleKeepBelow()
{
    if (d->keepBelow) {
        org_kde_plasma_window_set_state(
            d->window, ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW, 0);
    } else {
        org_kde_plasma_window_set_state(d->window,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW);
    }
}

void PlasmaWindow::requestToggleMinimized()
{
    if (d->minimized) {
        org_kde_plasma_window_set_state(
            d->window, ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED, 0);
    } else {
        org_kde_plasma_window_set_state(d->window,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
    }
}

void PlasmaWindow::requestToggleMaximized()
{
    if (d->maximized) {
        org_kde_plasma_window_set_state(
            d->window, ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED, 0);
    } else {
        org_kde_plasma_window_set_state(d->window,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED);
    }
}

void PlasmaWindow::setMinimizedGeometry(Surface* panel, QRect const& geom)
{
    org_kde_plasma_window_set_minimized_geometry(
        d->window, *panel, geom.x(), geom.y(), geom.width(), geom.height());
}

void PlasmaWindow::unsetMinimizedGeometry(Surface* panel)
{
    org_kde_plasma_window_unset_minimized_geometry(d->window, *panel);
}

void PlasmaWindow::requestToggleShaded()
{
    if (d->shaded) {
        org_kde_plasma_window_set_state(
            d->window, ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED, 0);
    } else {
        org_kde_plasma_window_set_state(d->window,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED,
                                        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED);
    }
}

quint32 PlasmaWindow::internalId() const
{
    return d->internalId;
}

QPointer<PlasmaWindow> PlasmaWindow::parentWindow() const
{
    return d->parentWindow;
}

QRect PlasmaWindow::geometry() const
{
    return d->geometry;
}

std::string const& PlasmaWindow::uuid() const
{
    return d->uuid;
}

std::string const& PlasmaWindow::resource_name() const
{
    return d->resource_name;
}

void PlasmaWindow::requestEnterVirtualDesktop(QString const& id)
{
    org_kde_plasma_window_request_enter_virtual_desktop(d->window, id.toUtf8());
}

void PlasmaWindow::requestEnterNewVirtualDesktop()
{
    org_kde_plasma_window_request_enter_new_virtual_desktop(d->window);
}

void PlasmaWindow::requestLeaveVirtualDesktop(QString const& id)
{
    org_kde_plasma_window_request_leave_virtual_desktop(d->window, id.toUtf8());
}

void PlasmaWindow::request_send_to_output(Output* output)
{
    org_kde_plasma_window_send_to_output(d->window, output->output());
}

QStringList PlasmaWindow::plasmaVirtualDesktops() const
{
    return d->plasmaVirtualDesktops;
}

}
