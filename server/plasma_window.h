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

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>
#include <string>
#include <vector>

class QSize;

namespace Wrapland::Server
{

class Display;
class PlasmaWindow;
class Surface;
class PlasmaVirtualDesktopManager;

class WRAPLANDSERVER_EXPORT PlasmaWindowManager : public QObject
{
    Q_OBJECT
public:
    explicit PlasmaWindowManager(Display* display);
    ~PlasmaWindowManager() override;

    enum class ShowingDesktopState { Disabled, Enabled };
    void setShowingDesktopState(ShowingDesktopState state);

    PlasmaWindow* createWindow(QObject* parent = nullptr);
    QList<PlasmaWindow*> windows() const;

    void unmapWindow(PlasmaWindow* window);

    void setVirtualDesktopManager(PlasmaVirtualDesktopManager* manager);

    PlasmaVirtualDesktopManager* virtualDesktopManager() const;

Q_SIGNALS:
    void requestChangeShowingDesktop(ShowingDesktopState requestedState);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT PlasmaWindow : public QObject
{
    Q_OBJECT
public:
    ~PlasmaWindow() override;

    void setTitle(const QString& title);
    void setAppId(const QString& appId);
    void setPid(uint32_t pid);
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
    void setSkipTaskbar(bool set);
    void setSkipSwitcher(bool set);
    void setShadeable(bool set);
    void setShaded(bool set);
    void setMovable(bool set);
    void setResizable(bool set);
    void setApplicationMenuPaths(const QString& serviceName, const QString& objectPath) const;

    /**
     * FIXME: still relevant with new desktops? Eike says yes in libtaskmanager code.
     */
    void setVirtualDesktopChangeable(bool set);

    void unmap();

    QHash<Surface*, QRect> minimizedGeometries() const;

    void setParentWindow(PlasmaWindow* parentWindow);
    void setGeometry(const QRect& geometry);
    void setIcon(const QIcon& icon);

    void addPlasmaVirtualDesktop(std::string const& id);
    void removePlasmaVirtualDesktop(std::string const& id);
    std::vector<std::string> const& plasmaVirtualDesktops() const;

Q_SIGNALS:
    void closeRequested();
    void moveRequested();
    void resizeRequested();
    void activeRequested(bool set);
    void minimizedRequested(bool set);
    void maximizedRequested(bool set);
    void fullscreenRequested(bool set);
    void keepAboveRequested(bool set);
    void keepBelowRequested(bool set);
    void demandsAttentionRequested(bool set);
    void closeableRequested(bool set);
    void minimizeableRequested(bool set);
    void maximizeableRequested(bool set);
    void fullscreenableRequested(bool set);
    void skipTaskbarRequested(bool set);
    void skipSwitcherRequested(bool set);
    QRect minimizedGeometriesChanged();
    void shadeableRequested(bool set);
    void shadedRequested(bool set);
    void movableRequested(bool set);
    void resizableRequested(bool set);
    void virtualDesktopChangeableRequested(bool set);

    void enterPlasmaVirtualDesktopRequested(const QString& desktop);
    void enterNewPlasmaVirtualDesktopRequested();
    void leavePlasmaVirtualDesktopRequested(const QString& desktop);

private:
    friend class PlasmaWindowManager;
    friend class PlasmaWindowRes;
    explicit PlasmaWindow(PlasmaWindowManager* manager, QObject* parent = nullptr);

    class Private;
    const std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::PlasmaWindowManager::ShowingDesktopState)
