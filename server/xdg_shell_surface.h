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

#include <QObject>
#include <QSize>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class Client;
class Seat;
class Surface;
class XdgShell;

class WRAPLANDSERVER_EXPORT XdgShellSurface : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Maximized = 1 << 0,
        Fullscreen = 1 << 1,
        Resizing = 1 << 2,
        Activated = 1 << 3,
        TiledLeft = 1 << 4,
        TiledRight = 1 << 5,
        TiledTop = 1 << 6,
        TiledBottom = 1 << 7,
    };
    Q_DECLARE_FLAGS(States, State)

    enum class ConstraintAdjustment {
        SlideX = 1 << 0,
        SlideY = 1 << 1,
        FlipX = 1 << 2,
        FlipY = 1 << 3,
        ResizeX = 1 << 4,
        ResizeY = 1 << 5,
    };
    Q_DECLARE_FLAGS(ConstraintAdjustments, ConstraintAdjustment)

    bool configurePending() const;

    Surface* surface() const;
    void commit();

    QRect window_geometry() const;

    /**
     * Relative to main surface even when no explicit window geometry is set. Then describes the
     * margins between main surface and its expanse, i.e. it's union with all subsurfaces.
     */
    QMargins window_margins() const;

Q_SIGNALS:
    void window_geometry_changed(QRect const& windowGeometry);
    void resourceDestroyed();

private:
    friend class XdgShell;
    friend class XdgShellToplevel;
    friend class XdgShellPopup;
    XdgShellSurface(Client* client,
                    uint32_t version,
                    uint32_t id,
                    XdgShell* shell,
                    Surface* surface);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::XdgShellSurface*)
Q_DECLARE_METATYPE(Wrapland::Server::XdgShellSurface::State)
Q_DECLARE_METATYPE(Wrapland::Server::XdgShellSurface::States)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::XdgShellSurface::ConstraintAdjustments)
