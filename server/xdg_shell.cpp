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
#include "xdg_shell.h"
#include "xdg_shell_p.h"

#include "display.h"
#include "surface_p.h"
#include "xdg_shell_positioner_p.h"
#include "xdg_shell_surface.h"
#include "xdg_shell_surface_p.h"
#include "xdg_shell_toplevel_p.h"

#include <cassert>
#include <map>

namespace Wrapland::Server
{

XdgShell::Private::Private(XdgShell* q, Display* display)
    : XdgShellGlobal(q, display, &xdg_wm_base_interface, &s_interface)
{
}

const struct xdg_wm_base_interface XdgShell::Private::s_interface = {
    resourceDestroyCallback,
    cb<createPositionerCallback>,
    cb<getXdgSurfaceCallback>,
    cb<pongCallback>,
};

void XdgShell::Private::prepareUnbind(XdgShellBind* bind)
{
    if (auto it = bindsObjects.find(bind); it != bindsObjects.end()) {
        auto& surfaces = it->second.surfaces;
        auto& positioners = it->second.positioners;
        for (auto surface : surfaces) {
            // We remove the bind, so disconnect destroy connection.
            QObject::disconnect(surface, &XdgShellSurface::resourceDestroyed, handle, nullptr);
        }
        for (auto positioner : positioners) {
            // We remove the bind, so disconnect destroy connection.
            QObject::disconnect(
                positioner, &XdgShellPositioner::resourceDestroyed, handle, nullptr);
        }
        if (!surfaces.empty()) {
            bind->post_error(XDG_WM_BASE_ERROR_DEFUNCT_SURFACES,
                             "xdg_wm_base destroyed before surfaces");
        }
        bindsObjects.erase(it);
    }
}

void XdgShell::Private::createPositionerCallback(XdgShellBind* bind, uint32_t id)
{
    auto priv = bind->global()->handle->d_ptr.get();

    auto positioner = new XdgShellPositioner(bind->client->handle, bind->version, id);

    auto bindsIt = priv->bindsObjects.find(bind);
    if (bindsIt == priv->bindsObjects.end()) {
        BindResources b;
        b.positioners.push_back(positioner);
        priv->bindsObjects[bind] = b;
    } else {
        (*bindsIt).second.positioners.push_back(positioner);
    }

    QObject::connect(
        positioner, &XdgShellPositioner::resourceDestroyed, priv->handle, [bindsIt, positioner] {
            auto& positioners = (*bindsIt).second.positioners;
            positioners.erase(std::remove(positioners.begin(), positioners.end(), positioner),
                              positioners.end());
        });
}

void XdgShell::Private::getXdgSurfaceCallback(XdgShellBind* bind,
                                              uint32_t id,
                                              wl_resource* wlSurface)
{
    auto priv = bind->global()->handle->d_ptr.get();

    auto surface = Surface::Private::get_handle(wlSurface);

    auto bindsIt = priv->bindsObjects.find(bind);
    if (bindsIt != priv->bindsObjects.end()) {
        auto const& surfaces = (*bindsIt).second.surfaces;
        auto surfaceIt = std::find_if(surfaces.cbegin(), surfaces.cend(), [surface](auto s) {
            return surface == s->surface();
        });
        if (surfaceIt != surfaces.cend()) {
            bind->post_error(XDG_WM_BASE_ERROR_ROLE, "XDG Surface already created");
            return;
        }
    }

    auto shellSurface
        = new XdgShellSurface(bind->client->handle, bind->version, id, priv->handle, surface);

    if (bindsIt == priv->bindsObjects.end()) {
        BindResources b;
        b.surfaces.push_back(shellSurface);
        priv->bindsObjects[bind] = b;
    } else {
        (*bindsIt).second.surfaces.push_back(shellSurface);
    }

    QObject::connect(shellSurface,
                     &XdgShellSurface::resourceDestroyed,
                     priv->handle,
                     [priv, bind, shellSurface] {
                         auto bindsIt = priv->bindsObjects.find(bind);

                         auto& surfaces = bindsIt->second.surfaces;
                         auto surfaceIt = std::find(surfaces.begin(), surfaces.end(), shellSurface);
                         assert(surfaceIt != surfaces.end());
                         surfaces.erase(surfaceIt);
                     });
}

void XdgShell::Private::pongCallback(XdgShellBind* bind, uint32_t serial)
{
    auto priv = bind->global()->handle->d_ptr.get();

    auto timerIt = priv->pingTimers.find(serial);
    if (timerIt != priv->pingTimers.end() && (*timerIt).second->isActive()) {
        delete (*timerIt).second;
        priv->pingTimers.erase(timerIt);
        Q_EMIT priv->handle->pongReceived(serial);
    }
}

constexpr int pingTime = 1000;

void XdgShell::Private::setupTimer(uint32_t serial)
{
    auto pingTimer = new QTimer();
    pingTimer->setSingleShot(false);
    pingTimer->setInterval(pingTime);

    int attempt = 0;

    connect(pingTimer, &QTimer::timeout, handle, [this, serial, attempt]() mutable {
        ++attempt;

        if (attempt == 1) {
            Q_EMIT handle->pingDelayed(serial);
            return;
        }
        Q_EMIT handle->pingTimeout(serial);
        auto timerIt = pingTimers.find(serial);
        if (timerIt != pingTimers.end()) {
            delete (*timerIt).second;
            pingTimers.erase(timerIt);
        }
    });

    pingTimers[serial] = pingTimer;
    pingTimer->start();
}

uint32_t XdgShell::Private::ping(Client* client)
{
    // Find bind for this surface:
    auto bindIt
        = std::find_if(bindsObjects.cbegin(), bindsObjects.cend(), [client](auto const& bind) {
              return bind.first->client->handle == client;
          });

    if (bindIt == bindsObjects.cend()) {
        return 0;
    }

    const uint32_t pingSerial = display()->handle->nextSerial();

    send<xdg_wm_base_send_ping>((*bindIt).first, pingSerial);

    setupTimer(pingSerial);
    return pingSerial;
}

XdgShellSurface* XdgShell::Private::getSurface(wl_resource* wlSurface)
{
    XdgShellSurface* foundSurface = nullptr;

    for (auto const& bind : bindsObjects) {
        auto const& surfaces = bind.second.surfaces;
        for (auto const& surface : surfaces) {
            if (surface->d_ptr->resource == wlSurface) {
                foundSurface = surface;
                break;
            }
        }
        if (foundSurface) {
            break;
        }
    }

    return foundSurface;
}

XdgShellToplevel* XdgShell::Private::getToplevel(wl_resource* wlToplevel)
{
    XdgShellSurface* foundSurface = nullptr;

    for (auto const& bind : bindsObjects) {
        auto const& surfaces = bind.second.surfaces;
        for (auto const& surface : surfaces) {
            if (surface->d_ptr->toplevel
                && surface->d_ptr->toplevel->d_ptr->resource == wlToplevel) {
                foundSurface = surface;
                break;
            }
        }
        if (foundSurface) {
            break;
        }
    }

    if (foundSurface && foundSurface->d_ptr->toplevel) {
        return foundSurface->d_ptr->toplevel;
    }
    return nullptr;
}

XdgShellPositioner* XdgShell::Private::getPositioner(wl_resource* wlPositioner)
{
    XdgShellPositioner* foundPositioner = nullptr;

    for (auto const& bind : bindsObjects) {
        auto const& positioners = bind.second.positioners;
        for (auto const& positioner : positioners) {
            if (positioner->d_ptr->resource == wlPositioner) {
                foundPositioner = positioner;
                break;
            }
        }
        if (foundPositioner) {
            break;
        }
    }

    return foundPositioner;
}

XdgShell::XdgShell(Display* display)
    : d_ptr(new Private(this, display))
{
    d_ptr->create();
}

XdgShell::~XdgShell() = default;

uint32_t XdgShell::ping(Client* client)
{
    return d_ptr->ping(client);
}

}
