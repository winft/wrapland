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
#include "xdg_decoration.h"

#include "display.h"
#include "xdg_shell_p.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include "wayland-xdg-decoration-server-protocol.h"

#include <map>

namespace Wrapland::Server
{

class XdgDecorationManager::Private : public Wayland::Global<XdgDecorationManager>
{
public:
    Private(XdgDecorationManager* q, D_isplay* display, XdgShell* shell);

    std::map<XdgShellToplevel*, XdgDecoration*> m_decorations;

private:
    static void getToplevelDecorationCallback(wl_client* wlClient,
                                              wl_resource* wlResource,
                                              uint32_t id,
                                              wl_resource* wlToplevel);

    XdgShell* m_shell;

    static const struct zxdg_decoration_manager_v1_interface s_interface;
    static const uint32_t s_version;
};

const uint32_t XdgDecorationManager::Private::s_version = 1;

XdgDecorationManager::Private::Private(XdgDecorationManager* q, D_isplay* display, XdgShell* shell)
    : Wayland::Global<XdgDecorationManager>(q,
                                            display,
                                            &zxdg_decoration_manager_v1_interface,
                                            &s_interface)
    , m_shell{shell}
{
}

const struct zxdg_decoration_manager_v1_interface XdgDecorationManager::Private::s_interface = {
    resourceDestroyCallback,
    getToplevelDecorationCallback,
};

void XdgDecorationManager::Private::getToplevelDecorationCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    uint32_t id,
    wl_resource* wlToplevel)
{
    auto priv = fromResource(wlResource)->d_ptr;
    auto bind = priv->getBind(wlResource);

    auto toplevel = priv->m_shell->d_ptr->getToplevel(wlToplevel);
    if (!toplevel) {
        bind->postError(ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED, "No xdg-toplevel found.");
        return;
    }
    if (priv->m_decorations.count(toplevel) > 0) {
        bind->postError(ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED,
                        "xdg decoration already created for this xdg-toplevel.");
        return;
    }

    auto deco = new XdgDecoration(bind->client()->handle(), priv->version(), id, toplevel);
    // TODO: check resource

    priv->m_decorations[toplevel] = deco;
    QObject::connect(deco, &XdgDecoration::resourceDestroyed, priv->handle(), [toplevel, priv]() {
        priv->m_decorations.erase(toplevel);
    });
    Q_EMIT priv->handle()->decorationCreated(deco);
}

XdgDecorationManager::XdgDecorationManager(D_isplay* display, XdgShell* shell, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display, shell))
{
    d_ptr->create();
}

XdgDecorationManager::~XdgDecorationManager() = default;

class XdgDecoration::Private : public Wayland::Resource<XdgDecoration>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            XdgShellToplevel* toplevel,
            XdgDecoration* q);
    ~Private() override;

    XdgDecoration::Mode m_requestedMode = XdgDecoration::Mode::Undefined;
    XdgShellToplevel* toplevel;

private:
    static void setModeCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t wlMode);
    static void unsetModeCallback(wl_client* wlClient, wl_resource* wlResource);

    static const struct zxdg_toplevel_decoration_v1_interface s_interface;
};

XdgDecoration::Private::Private(Client* client,
                                uint32_t version,
                                uint32_t id,
                                XdgShellToplevel* toplevel,
                                XdgDecoration* q)
    : Wayland::Resource<XdgDecoration>(client,
                                       version,
                                       id,
                                       &zxdg_toplevel_decoration_v1_interface,
                                       &s_interface,
                                       q)
    , toplevel{toplevel}
{
}

XdgDecoration::Private::~Private() = default;

const struct zxdg_toplevel_decoration_v1_interface XdgDecoration::Private::s_interface = {
    destroyCallback,
    setModeCallback,
    unsetModeCallback,
};

void XdgDecoration::Private::setModeCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             uint32_t wlMode)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));

    Mode mode = Mode::Undefined;
    switch (wlMode) {
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
        mode = Mode::ClientSide;
        break;
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
        mode = Mode::ServerSide;
        break;
    default:
        break;
    }

    priv->m_requestedMode = mode;
    Q_EMIT priv->handle()->modeRequested();
}

void XdgDecoration::Private::unsetModeCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));

    priv->m_requestedMode = Mode::Undefined;
    Q_EMIT priv->handle()->modeRequested();
}

XdgDecoration::XdgDecoration(Client* client,
                             uint32_t version,
                             uint32_t id,
                             XdgShellToplevel* toplevel)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, toplevel, this))
{
}

XdgDecoration::~XdgDecoration() = default;

void XdgDecoration::configure(XdgDecoration::Mode mode)
{
    switch (mode) {
    case Mode::ClientSide:
        d_ptr->send<zxdg_toplevel_decoration_v1_send_configure>(
            ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
        break;
    case Mode::ServerSide:
        d_ptr->send<zxdg_toplevel_decoration_v1_send_configure>(
            ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        break;
    default:
        break;
    }
}

XdgDecoration::Mode XdgDecoration::requestedMode() const
{
    return d_ptr->m_requestedMode;
}

XdgShellToplevel* XdgDecoration::toplevel() const
{
    return d_ptr->toplevel;
}

}
