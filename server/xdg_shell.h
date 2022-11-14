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

// For Qt metatype declaration.
#include "display.h"
#include "xdg_shell_popup.h"
#include "xdg_shell_toplevel.h"

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>

namespace Wrapland::Server
{
class Client;
class Display;
class XdgShellPopup;
class XdgShellToplevel;

class WRAPLANDSERVER_EXPORT XdgShell : public QObject
{
    Q_OBJECT
public:
    explicit XdgShell(Display* display);
    ~XdgShell() override;

    uint32_t ping(Client* client);

Q_SIGNALS:
    void toplevelCreated(XdgShellToplevel* toplevel);
    void popupCreated(XdgShellPopup* popup);

    void pongReceived(uint32_t serial);
    void pingDelayed(uint32_t serial);
    void pingTimeout(uint32_t serial);

private:
    friend class XdgShellPopup;
    friend class XdgShellSurface;
    friend class XdgDecorationManager;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
