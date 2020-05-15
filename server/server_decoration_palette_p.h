/****************************************************************************
Copyright 2020  Adrien Faveraux <ad1rie3@hotmail.fr>

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

#include "display.h"
#include "server_decoration_palette.h"
#include "surface.h"

#include "wayland/client.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-server_decoration_palette-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t ServerSideDecorationPaletteManagerVersion = 1;
using ServerSideDecorationPaletteManagerGlobal
    = Wayland::Global<ServerSideDecorationPaletteManager,
                      ServerSideDecorationPaletteManagerVersion>;

class ServerSideDecorationPaletteManager::Private : public ServerSideDecorationPaletteManagerGlobal
{
public:
    Private(D_isplay* display, ServerSideDecorationPaletteManager* qptr);
    ~Private() override;

    std::vector<ServerSideDecorationPalette*> palettes;

private:
    static void
    createCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id, wl_resource* surface);

    static const struct org_kde_kwin_server_decoration_palette_manager_interface s_interface;
};

class ServerSideDecorationPalette::Private : public Wayland::Resource<ServerSideDecorationPalette>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Surface* surface,
            ServerSideDecorationPalette* qptr);
    ~Private() override;

    Surface* surface;
    QString palette;

private:
    static void
    setPaletteCallback(wl_client* wlClient, wl_resource* wlResource, const char* palette);

    static ServerSideDecorationPalette* get(Surface* surface);

    static const struct org_kde_kwin_server_decoration_palette_interface s_interface;
};

}
