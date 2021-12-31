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
#include "server_decoration_palette_p.h"

#include <QtGlobal>

namespace Wrapland::Server
{

const struct org_kde_kwin_server_decoration_palette_manager_interface
    ServerSideDecorationPaletteManager::Private::s_interface
    = {
        cb<createCallback>,
};

ServerSideDecorationPaletteManager::Private::Private(Display* display,
                                                     ServerSideDecorationPaletteManager* qptr)
    : ServerSideDecorationPaletteManagerGlobal(
        qptr,
        display,
        &org_kde_kwin_server_decoration_palette_manager_interface,
        &s_interface)
{
    create();
}

void ServerSideDecorationPaletteManager::Private::createCallback(
    ServerSideDecorationPaletteManagerBind* bind,
    uint32_t id,
    wl_resource* wlSurface)
{
    auto priv = bind->global()->handle->d_ptr.get();
    auto surface = Wayland::Resource<Surface>::get_handle(wlSurface);

    auto palette
        = new ServerSideDecorationPalette(bind->client->handle, bind->version, id, surface);

    if (!palette->d_ptr->resource) {
        bind->post_no_memory();
        delete palette;
        return;
    }

    priv->palettes.push_back(palette);

    QObject::connect(palette, &ServerSideDecorationPalette::resourceDestroyed, priv->handle, [=]() {
        priv->palettes.erase(std::remove(priv->palettes.begin(), priv->palettes.end(), palette),
                             priv->palettes.end());
    });

    Q_EMIT priv->handle->paletteCreated(palette);
}

ServerSideDecorationPaletteManager::ServerSideDecorationPaletteManager(Display* display)
    : d_ptr(new Private(display, this))
{
}

ServerSideDecorationPaletteManager::~ServerSideDecorationPaletteManager() = default;

ServerSideDecorationPalette* ServerSideDecorationPaletteManager::paletteForSurface(Surface* surface)
{
    for (ServerSideDecorationPalette* menu : d_ptr->palettes) {
        if (menu->surface() == surface) {
            return menu;
        }
    }
    return nullptr;
}

const struct org_kde_kwin_server_decoration_palette_interface
    ServerSideDecorationPalette::Private::s_interface
    = {
        setPaletteCallback,
        destroyCallback,
};

void ServerSideDecorationPalette::Private::setPaletteCallback([[maybe_unused]] wl_client* wlClient,
                                                              wl_resource* wlResource,
                                                              const char* palette)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (priv->palette == QLatin1String(palette)) {
        return;
    }
    priv->palette = QString::fromUtf8(palette);
    Q_EMIT priv->handle->paletteChanged(priv->palette);
}

ServerSideDecorationPalette::Private::Private(Client* client,
                                              uint32_t version,
                                              uint32_t id,
                                              Surface* surface,
                                              ServerSideDecorationPalette* qptr)
    : Wayland::Resource<ServerSideDecorationPalette>(
        client,
        version,
        id,
        &org_kde_kwin_server_decoration_palette_interface,
        &s_interface,
        qptr)
    , surface(surface)
{
}

ServerSideDecorationPalette::ServerSideDecorationPalette(Client* client,
                                                         uint32_t version,
                                                         uint32_t id,
                                                         Surface* surface)
    : d_ptr(new Private(client, version, id, surface, this))
{
}

QString ServerSideDecorationPalette::palette() const
{
    return d_ptr->palette;
}

Surface* ServerSideDecorationPalette::surface() const
{
    return d_ptr->surface;
}

}
