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

#include "display.h"
#include "subsurface_p.h"
#include "surface_p.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{

class Subcompositor::Private : public Wayland::Global<Subcompositor>
{
public:
    Private(Subcompositor* q, D_isplay* display);

private:
    void subsurface(Client* client, uint32_t id, Surface* surface, Surface* parentSurface);

    static void destroyCallback(wl_client* wlClient, wl_resource* wlResource);
    static void subsurfaceCallback(wl_client* wlClient,
                                   wl_resource* wlResource,
                                   uint32_t id,
                                   wl_resource* wlSurface,
                                   wl_resource* wlParent);

    static const struct wl_subcompositor_interface s_interface;
    static const quint32 s_version;
};

const quint32 Subcompositor::Private::s_version = 1;

const struct wl_subcompositor_interface Subcompositor::Private::s_interface = {
    resourceDestroyCallback,
    subsurfaceCallback,
};

Subcompositor::Private::Private(Subcompositor* q, D_isplay* display)
    : Wayland::Global<Subcompositor>(q, display, &wl_subcompositor_interface, &s_interface)
{
}

void Subcompositor::Private::subsurfaceCallback([[maybe_unused]] wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t id,
                                                wl_resource* wlSurface,
                                                wl_resource* wlParent)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);

    auto surface = Wayland::Resource<Surface>::fromResource(wlSurface)->handle();
    auto parentSurface = Wayland::Resource<Surface>::fromResource(wlParent)->handle();

    handle->d_ptr->subsurface(client, id, surface, parentSurface);
}

void Subcompositor::Private::subsurface(Client* client,
                                        uint32_t id,
                                        Surface* surface,
                                        Surface* parentSurface)
{
    if (!surface || !parentSurface) {
        // TODO: post error in backend
        wl_resource_post_error(getResources(client)[0],
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "Surface or parent surface not found");
        return;
    }
    if (surface == parentSurface) {
        // TODO: post error in backend
        wl_resource_post_error(getResources(client)[0],
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "Cannot become sub composite to same surface");
        return;
    }

    // TODO: add check that surface is not already used in an interface (e.g. Shell)
    // TODO: add check that parentSurface is not a child of surface
    // TODO: handle error
    auto subsurface
        = new Subsurface(client, handle()->d_ptr->version(), id, surface, parentSurface);

    Q_EMIT handle()->subsurfaceCreated(subsurface);
}

Subcompositor::Subcompositor(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

Subcompositor::~Subcompositor()
{
    delete d_ptr;
}

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
    surface->d_ptr->subsurface = QPointer<Subsurface>(handle());

    // Copy current state to subsurfacePending state.
    // It's the reference for all new pending state which needs to be committed.
    surface->d_ptr->subsurfacePending = surface->d_ptr->current;
    surface->d_ptr->subsurfacePending.blurIsSet = false;
    surface->d_ptr->subsurfacePending.bufferIsSet = false;
    surface->d_ptr->subsurfacePending.childrenChanged = false;
    surface->d_ptr->subsurfacePending.contrastIsSet = false;
    surface->d_ptr->subsurfacePending.callbacks.clear();
    surface->d_ptr->subsurfacePending.inputIsSet = false;
    surface->d_ptr->subsurfacePending.inputIsInfinite = true;
    surface->d_ptr->subsurfacePending.opaqueIsSet = false;
    surface->d_ptr->subsurfacePending.shadowIsSet = false;
    surface->d_ptr->subsurfacePending.slideIsSet = false;
    parent->d_ptr->addChild(QPointer<Subsurface>(handle()));

    QObject::connect(surface, &Surface::resourceDestroyed, handle(), [this] {
        // From spec: "If the wl_surface associated with the wl_subsurface is destroyed,
        // the wl_subsurface object becomes inert. Note, that destroying either object
        // takes effect immediately."
        if (this->parent) {
            reinterpret_cast<Surface::Private*>(this->parent->d_ptr)
                ->removeChild(QPointer<Subsurface>(handle()));
        }
    });
}

Subsurface::Private::~Private()
{
    remove();
}

const struct wl_subsurface_interface Subsurface::Private::s_interface = {
    destroyCallback,
    setPositionCallback,
    placeAboveCallback,
    placeBelowCallback,
    setSyncCallback,
    setDeSyncCallback,
};

void Subsurface::Private::remove()
{
    // No need to notify the surface as it's tracking a QPointer which will be reset automatically.
    if (parent) {
        reinterpret_cast<Surface::Private*>(parent->d_ptr)
            ->removeChild(QPointer<Subsurface>(handle()));
    }
    parent = nullptr;
}

void Subsurface::Private::commit()
{
    if (scheduledPosChange) {
        scheduledPosChange = false;
        pos = scheduledPos;
        scheduledPos = QPoint();
        Q_EMIT handle()->positionChanged(pos);
    }
    if (surface) {
        surface->d_ptr->commitSubsurface();
    }
}

void Subsurface::Private::setPositionCallback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              int32_t x,
                                              int32_t y)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));

    // TODO: is this a fixed position?
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
    auto priv = static_cast<Private*>(fromResource(wlResource));
    auto sibling = Wayland::Resource<Surface>::fromResource(wlSibling)->handle();
    priv->placeAbove(sibling);
}

void Subsurface::Private::placeAbove(Surface* sibling)
{
    if (parent.isNull()) {
        // TODO: raise error
        return;
    }
    if (!parent->d_ptr->raiseChild(QPointer<Subsurface>(handle()), sibling)) {
        postError(WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Incorrect sibling");
    }
}

void Subsurface::Private::placeBelowCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             wl_resource* wlSibling)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    auto sibling = Wayland::Resource<Surface>::fromResource(wlSibling)->handle();
    priv->placeBelow(sibling);
}

void Subsurface::Private::placeBelow(Surface* sibling)
{
    if (parent.isNull()) {
        // TODO: raise error
        return;
    }
    if (!parent->d_ptr->lowerChild(QPointer<Subsurface>(handle()), sibling)) {
        postError(WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Incorrect sibling");
    }
}

void Subsurface::Private::setSyncCallback([[maybe_unused]] wl_client* wlClient,
                                          wl_resource* wlResource)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->setMode(Mode::Synchronized);
}

void Subsurface::Private::setDeSyncCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->setMode(Mode::Desynchronized);
}

void Subsurface::Private::setMode(Mode m)
{
    if (mode == m) {
        return;
    }
    if (m == Mode::Desynchronized
        && (!parent->subsurface() || !parent->subsurface()->isSynchronized())) {
        // no longer synchronized, this is like calling commit
        if (surface) {
            surface->d_ptr->commit();
            surface->d_ptr->commitSubsurface();
        }
    }
    mode = m;
    Q_EMIT handle()->modeChanged(m);
}

Subsurface::Subsurface(Client* client,
                       uint32_t version,
                       uint32_t id,
                       Surface* surface,
                       Surface* parent)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, surface, parent, this))
{
}

Subsurface::~Subsurface() = default;

QPoint Subsurface::position() const
{
    return d_ptr->pos;
}

QPointer<Surface> Subsurface::surface() const
{
    return d_ptr->surface;
}

QPointer<Surface> Subsurface::parentSurface() const
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

    if (d_ptr->parent.isNull()) {
        // That shouldn't happen, but let's assume false.
        return false;
    }

    if (!d_ptr->parent->subsurface().isNull()) {
        // Follow parent's mode.
        return d_ptr->parent->subsurface()->isSynchronized();
    }

    // Parent is no subsurface, thus parent is in desync mode and this surface is in desync mode.
    return false;
}

QPointer<Surface> Subsurface::mainSurface() const
{
    if (!d_ptr->parent) {
        return QPointer<Surface>();
    }
    if (d_ptr->parent->d_ptr->subsurface) {
        return d_ptr->parent->d_ptr->subsurface->mainSurface();
    }
    return d_ptr->parent;
}

}
}
