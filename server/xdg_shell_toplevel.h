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

#include "xdg_shell_surface.h"

// For Qt metatype declaration.
#include "seat.h"
#include "xdg_shell.h"

#include <QObject>
#include <QSize>
#include <set>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class output;
class Seat;
class XdgShellSurface;

enum class xdg_shell_wm_capability : std::uint8_t {
    window_menu = 1,
    maximize,
    fullscreen,
    minimize,
};

class WRAPLANDSERVER_EXPORT XdgShellToplevel : public QObject
{
    Q_OBJECT
public:
    std::string title() const;
    std::string appId() const;

    XdgShellSurface* surface() const;
    Client* client() const;

    XdgShellToplevel* transientFor() const;

    void configure_bounds(QSize const& bounds);
    void unconfigure_bounds();

    uint32_t configure(XdgShellSurface::States states, QSize const& size = QSize(0, 0));
    bool configurePending() const;

    QSize minimumSize() const;
    QSize maximumSize() const;

    void set_capabilities(std::set<xdg_shell_wm_capability> const& caps) const;
    void close();

Q_SIGNALS:
    void titleChanged(std::string title);
    void appIdChanged(std::string appId);
    void minSizeChanged(QSize const& size);
    void maxSizeChanged(QSize const& size);
    void moveRequested(Seat* seat, uint32_t serial);
    void maximizedChanged(bool maximized);
    void fullscreenChanged(bool fullscreen, Wrapland::Server::output* output);
    void windowMenuRequested(Seat* seat, uint32_t serial, QPoint const& position);
    void resizeRequested(Seat* seat, uint32_t serial, Qt::Edges edges);
    void minimizeRequested();
    void configureAcknowledged(uint32_t serial);
    void transientForChanged();
    void resourceDestroyed();

private:
    friend class XdgShell;
    friend class XdgShellSurface;
    explicit XdgShellToplevel(uint32_t version, uint32_t id, XdgShellSurface* surface);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::XdgShellToplevel*)
