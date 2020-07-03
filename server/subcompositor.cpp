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
#include "subcompositor.h"

#include "buffer.h"
#include "display.h"
#include "subsurface_p.h"
#include "surface_p.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <cassert>
#include <wayland-server.h>

namespace Wrapland::Server
{

class Subcompositor::Private : public Wayland::Global<Subcompositor>
{
public:
    Private(Subcompositor* q, Display* display);

private:
    void subsurface(Client* client, uint32_t id, Surface* surface, Surface* parentSurface);

    static void destroyCallback(wl_client* wlClient, wl_resource* wlResource);
    static void subsurfaceCallback(wl_client* wlClient,
                                   wl_resource* wlResource,
                                   uint32_t id,
                                   wl_resource* wlSurface,
                                   wl_resource* wlParent);

    static const struct wl_subcompositor_interface s_interface;
};

const struct wl_subcompositor_interface Subcompositor::Private::s_interface = {
    resourceDestroyCallback,
    subsurfaceCallback,
};

Subcompositor::Private::Private(Subcompositor* q, Display* display)
    : Wayland::Global<Subcompositor>(q, display, &wl_subcompositor_interface, &s_interface)
{
}

void Subcompositor::Private::subsurfaceCallback([[maybe_unused]] wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t id,
                                                wl_resource* wlSurface,
                                                wl_resource* wlParent)
{
    auto subcompositor = handle(wlResource);
    auto client = subcompositor->d_ptr->display()->handle()->getClient(wlClient);

    auto surface = Wayland::Resource<Surface>::handle(wlSurface);
    auto parentSurface = Wayland::Resource<Surface>::handle(wlParent);

    subcompositor->d_ptr->subsurface(client, id, surface, parentSurface);
}

void Subcompositor::Private::subsurface(Client* client,
                                        uint32_t id,
                                        Surface* surface,
                                        Surface* parentSurface)
{
    if (!surface || !parentSurface) {
        // TODO(romangg): post error in backend
        wl_resource_post_error(getResources(client)[0],
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "Surface or parent surface not found");
        return;
    }
    if (surface == parentSurface) {
        // TODO(romangg): post error in backend
        wl_resource_post_error(getResources(client)[0],
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "Cannot become sub composite to same surface");
        return;
    }

    // TODO(romangg): add check that surface is not already used in an interface (e.g. Shell)
    // TODO(romangg): add check that parentSurface is not a child of surface
    // TODO(romangg): handle error
    auto subsurface
        = new Subsurface(client, handle()->d_ptr->version(), id, surface, parentSurface);

    Q_EMIT handle()->subsurfaceCreated(subsurface);
}

Subcompositor::Subcompositor(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

Subcompositor::~Subcompositor() = default;

Subsurface::Private::Private(Client* client,
                             uint32_t version,
                             uint32_t id,
                             Surface* surface,
                             Surface* parent,
                             Subsurface* q)
    : Wayland::Resource<Subsurface>(client, version, id, &wl_subsurface_interface, &s_interface, q)
    , surface{surface}
    , parent{parent}
{
    surface->d_ptr->subsurface = q;

    QObject::connect(surface, &Surface::resourceDestroyed, q, [this] {
        // From spec: "If the wl_surface associated with the wl_subsurface is destroyed,
        // the wl_subsurface object becomes inert. Note, that destroying either object
        // takes effect immediately."
        if (this->parent) {
            this->parent->d_ptr->removeChild(handle());
            this->parent = nullptr;
        }
        this->surface = nullptr;
    });
}

void Subsurface::Private::init()
{
    parent->d_ptr->addChild(handle());
}

const struct wl_subsurface_interface Subsurface::Private::s_interface = {
    destroyCallback,
    setPositionCallback,
    placeAboveCallback,
    placeBelowCallback,
    setSyncCallback,
    setDeSyncCallback,
};

void Subsurface::Private::applyCached(bool force)
{
    assert(surface);

    if (scheduledPosChange) {
        scheduledPosChange = false;
        pos = scheduledPos;
        scheduledPos = QPoint();
        Q_EMIT handle()->positionChanged(pos);
    }

    if (force || handle()->isSynchronized()) {
        surface->d_ptr->updateCurrentState(cached, true);
    }
}

void Subsurface::Private::commit()
{
    assert(surface);

    if (handle()->isSynchronized()) {
        // Sync mode. We cache the pending state and wait for the parent surface to commit.
        cached = std::move(surface->d_ptr->pending);
        surface->d_ptr->pending = SurfaceState();
        surface->d_ptr->pending.children = cached.children;
        if (cached.buffer) {
            cached.buffer->setCommitted();
        }
        return;
    }

    // Desync mode. We commit the surface directly.
    surface->d_ptr->updateCurrentState(false);
}

void Subsurface::Private::setPositionCallback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              int32_t x,
                                              int32_t y)
{
    auto priv = handle(wlResource)->d_ptr;

    // TODO(unknown author): is this a fixed position?
    priv->setPosition(QPoint(x, y));
}

void Subsurface::Private::setPosition(const QPoint& p)
{
    if (!handle()->isSynchronized()) {
        // Workaround for https://bugreports.qt.io/browse/QTBUG-52118,
        // apply directly as Qt doesn't commit the parent surface.
        pos = p;
        Q_EMIT handle()->positionChanged(pos);
        return;
    }
    if (scheduledPos == p) {
        return;
    }
    scheduledPos = p;
    scheduledPosChange = true;
}

void Subsurface::Private::placeAboveCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             wl_resource* wlSibling)
{
    auto priv = handle(wlResource)->d_ptr;
    auto sibling = Wayland::Resource<Surface>::handle(wlSibling);
    priv->placeAbove(sibling);
}

void Subsurface::Private::placeAbove(Surface* sibling)
{
    if (!parent) {
        // TODO(unknown author): raise error
        return;
    }
    if (!parent->d_ptr->raiseChild(handle(), sibling)) {
        postError(WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Incorrect sibling");
    }
}

void Subsurface::Private::placeBelowCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             wl_resource* wlSibling)
{
    auto priv = handle(wlResource)->d_ptr;
    auto sibling = Wayland::Resource<Surface>::handle(wlSibling);
    priv->placeBelow(sibling);
}

void Subsurface::Private::placeBelow(Surface* sibling)
{
    if (!parent) {
        // TODO(unknown author): raise error
        return;
    }
    if (!parent->d_ptr->lowerChild(handle(), sibling)) {
        postError(WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Incorrect sibling");
    }
}

void Subsurface::Private::setSyncCallback([[maybe_unused]] wl_client* wlClient,
                                          wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->setMode(Mode::Synchronized);
}

void Subsurface::Private::setDeSyncCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->setMode(Mode::Desynchronized);
}

void Subsurface::Private::setMode(Mode m)
{
    if (mode == m) {
        return;
    }
    mode = m;
    Q_EMIT handle()->modeChanged(m);

    if (m == Mode::Desynchronized
        && (!parent->subsurface() || !parent->subsurface()->isSynchronized())) {
        // Parent subsurface list must be updated immediately.
        auto& cc = parent->d_ptr->current.children;
        auto subsurface = handle();
        if (std::find(cc.cbegin(), cc.cend(), subsurface) == cc.cend()) {
            cc.push_back(subsurface);
        }
        // No longer synchronized, this is like calling commit.
        assert(surface);
        surface->d_ptr->updateCurrentState(cached, false);
    }
}

Subsurface::Subsurface(Client* client,
                       uint32_t version,
                       uint32_t id,
                       Surface* surface,
                       Surface* parent)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, surface, parent, this))
{
    d_ptr->init();
}

Subsurface::~Subsurface()
{
    if (d_ptr->surface) {
        d_ptr->surface->d_ptr->subsurface = nullptr;
    }
    d_ptr->surface = nullptr;

    if (d_ptr->parent) {
        d_ptr->parent->d_ptr->removeChild(this);
    }
    d_ptr->parent = nullptr;
}

QPoint Subsurface::position() const
{
    return d_ptr->pos;
}

Surface* Subsurface::surface() const
{
    return d_ptr->surface;
}

Surface* Subsurface::parentSurface() const
{
    return d_ptr->parent;
}

Subsurface::Mode Subsurface::mode() const
{
    return d_ptr->mode;
}

bool Subsurface::isSynchronized() const
{
    if (d_ptr->mode == Mode::Synchronized) {
        return true;
    }

    if (!d_ptr->parent) {
        // That shouldn't happen, but let's assume false.
        return false;
    }

    if (auto parentSub = d_ptr->parent->subsurface()) {
        // Follow parent's mode.
        return parentSub->isSynchronized();
    }

    // Parent is no subsurface, thus parent is in desync mode and this surface is in desync mode.
    return false;
}

Surface* Subsurface::mainSurface() const
{
    if (!d_ptr->parent) {
        return nullptr;
    }
    if (d_ptr->parent->d_ptr->subsurface) {
        return d_ptr->parent->d_ptr->subsurface->mainSurface();
    }
    return d_ptr->parent;
}

}
