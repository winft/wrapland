/********************************************************************
Copyright 2020 Faveraux Adrien <ad1rie3@hotmail.fr>

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

struct wl_resource;

namespace Wrapland::Server
{

class Display;
class Client;
class Surface;
class PlasmaShellSurface;

class WRAPLANDSERVER_EXPORT PlasmaShell : public QObject
{
    Q_OBJECT
public:
    explicit PlasmaShell(Display* display);
    ~PlasmaShell() override;

Q_SIGNALS:
    void surfaceCreated(Wrapland::Server::PlasmaShellSurface*);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT PlasmaShellSurface : public QObject
{
    Q_OBJECT
public:
    Surface* surface() const;
    PlasmaShell* shell() const;
    wl_resource* resource() const;
    Client* client() const;
    QPoint position() const;
    bool isPositionSet() const;

    enum class Role {
        Normal,
        Desktop,
        Panel,
        OnScreenDisplay,
        Notification,
        ToolTip,
        CriticalNotification,
    };

    Role role() const;

    enum class PanelBehavior {
        AlwaysVisible,
        AutoHide,
        WindowsCanCover,
        WindowsGoBelow,
    };

    PanelBehavior panelBehavior() const;
    bool skipTaskbar() const;
    bool skipSwitcher() const;
    void hideAutoHidingPanel();
    void showAutoHidingPanel();

    // TODO(unknown author): KF6 rename to something generic
    bool panelTakesFocus() const;

    static PlasmaShellSurface* get(wl_resource* native);

Q_SIGNALS:
    void positionChanged();
    void roleChanged();
    void panelBehaviorChanged();
    void skipTaskbarChanged();
    void skipSwitcherChanged();
    void panelAutoHideHideRequested();
    void panelAutoHideShowRequested();
    void panelTakesFocusChanged();
    void resourceDestroyed();

private:
    friend class PlasmaShell;
    PlasmaShellSurface(Client* client,
                       uint32_t version,
                       uint32_t id,
                       Surface* surface,
                       PlasmaShell* shell);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::PlasmaShellSurface::Role)
Q_DECLARE_METATYPE(Wrapland::Server::PlasmaShellSurface::PanelBehavior)
