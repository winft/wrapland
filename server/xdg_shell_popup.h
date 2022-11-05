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
#include "xdg_shell_positioner.h"

#include <QObject>
#include <QSize>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class Client;
class Seat;
class Surface;
class XdgShellPopup;
class XdgShellSurface;

class WRAPLANDSERVER_EXPORT XdgShellPopup : public QObject
{
    Q_OBJECT
public:
    XdgShellSurface* surface() const;
    Client* client() const;

    uint32_t configure(QRect const& rect);

    XdgShellSurface* transientFor() const;
    QPoint transientOffset() const;

    xdg_shell_positioner const& get_positioner() const;

    void popupDone();
    void repositioned(uint32_t token);

Q_SIGNALS:
    void configureAcknowledged(uint32_t serial);
    void grabRequested(Seat* seat, uint32_t serial);
    void reposition(uint32_t token);
    void resourceDestroyed();

private:
    friend class XdgShellSurface;
    explicit XdgShellPopup(uint32_t version,
                           uint32_t id,
                           XdgShellSurface* surface,
                           XdgShellSurface* parent);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::XdgShellPopup*)
