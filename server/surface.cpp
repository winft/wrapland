/********************************************************************
Copyright © 2014  Martin Gräßlin <mgraesslin@kde.org>
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
#include "surface.h"
#include "surface_p.h"

#include "buffer.h"

#include "blur.h"
#include "client.h"
#include "compositor.h"
#include "contrast.h"
#include "idle_inhibit_v1.h"
#include "idle_inhibit_v1_p.h"
#include "layer_shell_v1_p.h"
#include "pointer_constraints_v1.h"
#include "pointer_constraints_v1_p.h"
#include "presentation_time.h"
#include "region.h"
#include "shadow.h"
#include "slide.h"
#include "subcompositor.h"
#include "subsurface_p.h"
#include "viewporter_p.h"
#include "wl_output_p.h"
#include "xdg_shell_surface_p.h"

#include <QListIterator>

#include <algorithm>
#include <cassert>
#include <wayland-server.h>
#include <wayland-viewporter-server-protocol.h>

namespace Wrapland::Server
{

Surface::Private::Private(Client* client, uint32_t version, uint32_t id, Surface* q_ptr)
    : Wayland::Resource<Surface>(client, version, id, &wl_surface_interface, &s_interface, q_ptr)
    , q_ptr{q_ptr}
{
}

Surface::Private::~Private()
{
    // Copy all existing callbacks to new list and clear existing lists.
    // The wl_resource_destroy on the callback resource goes into destroyFrameCallback which would
    // modify the list we are iterating on.
    std::vector<wl_resource*> callbacksToDestroy;
    callbacksToDestroy.insert(
        callbacksToDestroy.end(), current.callbacks.begin(), current.callbacks.end());
    current.callbacks.clear();

    callbacksToDestroy.insert(
        callbacksToDestroy.end(), pending.callbacks.begin(), pending.callbacks.end());
    pending.callbacks.clear();

    for (auto callback : callbacksToDestroy) {
        wl_resource_destroy(callback);
    }

    if (subsurface) {
        subsurface->d_ptr->surface = nullptr;
        subsurface = nullptr;
    }

    for (auto child : current.pub.children) {
        child->d_ptr->parent = nullptr;
    }
    for (auto child : pending.pub.children) {
        child->d_ptr->parent = nullptr;
    }
}

void Surface::Private::addChild(Subsurface* child)
{
    if (subsurface) {
        // We add it indiscriminately to the cached state. If the subsurface state is synchronized
        // on next parent commit it is added, if not it will be ignored here.
        subsurface->d_ptr->cached.pub.children.push_back(child);
    }
    pending.pub.children.push_back(child);
    pending.pub.updates |= surface_change::children;

    QObject::connect(
        child->surface(), &Surface::subsurfaceTreeChanged, handle, &Surface::subsurfaceTreeChanged);
}

void Surface::Private::removeChild(Subsurface* child)
{
    if (subsurface) {
        auto& cached = subsurface->d_ptr->cached;
        cached.pub.children.erase(
            std::remove(cached.pub.children.begin(), cached.pub.children.end(), child),
            cached.pub.children.end());
    }
    pending.pub.children.erase(
        std::remove(pending.pub.children.begin(), pending.pub.children.end(), child),
        pending.pub.children.end());
    current.pub.children.erase(
        std::remove(current.pub.children.begin(), current.pub.children.end(), child),
        current.pub.children.end());

    // TODO(romangg): only emit that if the child was mapped.
    Q_EMIT handle->subsurfaceTreeChanged();

    if (child->surface()) {
        QObject::disconnect(child->surface(),
                            &Surface::subsurfaceTreeChanged,
                            handle,
                            &Surface::subsurfaceTreeChanged);
    }
}

bool Surface::Private::raiseChild(Subsurface* subsurface, Surface* sibling)
{
    auto it = std::find(pending.pub.children.begin(), pending.pub.children.end(), subsurface);

    if (it == pending.pub.children.end()) {
        return false;
    }

    if (pending.pub.children.size() == 1) {
        // Nothing to do.
        return true;
    }

    if (sibling == handle) {
        // It's sibling to the parent, so needs to become last item.
        pending.pub.children.erase(it);
        pending.pub.children.push_back(subsurface);
        pending.pub.updates |= surface_change::children;
        return true;
    }

    if (!sibling->subsurface()) {
        // Not a sub surface.
        return false;
    }

    auto siblingIt = std::find(
        pending.pub.children.begin(), pending.pub.children.end(), sibling->subsurface());
    if (siblingIt == pending.pub.children.end() || siblingIt == it) {
        // Not a sibling.
        return false;
    }

    auto value = (*it);
    pending.pub.children.erase(it);

    // Find the iterator again.
    siblingIt = std::find(
        pending.pub.children.begin(), pending.pub.children.end(), sibling->subsurface());
    pending.pub.children.insert(++siblingIt, value);
    pending.pub.updates |= surface_change::children;
    return true;
}

bool Surface::Private::lowerChild(Subsurface* subsurface, Surface* sibling)
{
    auto it = std::find(pending.pub.children.begin(), pending.pub.children.end(), subsurface);
    if (it == pending.pub.children.end()) {
        return false;
    }
    if (pending.pub.children.size() == 1) {
        // nothing to do
        return true;
    }
    if (sibling == handle) {
        // it's to the parent, so needs to become first item
        auto value = *it;
        pending.pub.children.erase(it);
        pending.pub.children.insert(pending.pub.children.begin(), value);
        pending.pub.updates |= surface_change::children;
        return true;
    }
    if (!sibling->subsurface()) {
        // not a sub surface
        return false;
    }
    auto siblingIt = std::find(
        pending.pub.children.begin(), pending.pub.children.end(), sibling->subsurface());
    if (siblingIt == pending.pub.children.end() || siblingIt == it) {
        // not a sibling
        return false;
    }
    auto value = (*it);
    pending.pub.children.erase(it);
    // find the iterator again
    siblingIt = std::find(
        pending.pub.children.begin(), pending.pub.children.end(), sibling->subsurface());
    pending.pub.children.insert(siblingIt, value);
    pending.pub.updates |= surface_change::children;
    return true;
}

void Surface::Private::setShadow(Shadow* shadow)
{
    pending.pub.shadow = shadow;
    pending.pub.updates |= surface_change::shadow;
}

void Surface::Private::setBlur(Blur* blur)
{
    pending.pub.blur = blur;
    pending.pub.updates |= surface_change::blur;
}

void Surface::Private::setSlide(Slide* slide)
{
    pending.pub.slide = slide;
    pending.pub.updates |= surface_change::slide;
}

void Surface::Private::setContrast(Contrast* contrast)
{
    pending.pub.contrast = contrast;
    pending.pub.updates |= surface_change::contrast;
}

void Surface::Private::setSourceRectangle(QRectF const& source)
{
    pending.pub.source_rectangle = source;
    pending.pub.updates |= surface_change::source_rectangle;
}

void Surface::Private::setDestinationSize(QSize const& dest)
{
    pending.destinationSize = dest;
    pending.destinationSizeIsSet = true;
}

void Surface::Private::installViewport(Viewport* vp)
{
    Q_ASSERT(viewport.isNull());
    viewport = vp;
    connect(viewport, &Viewport::destinationSizeSet, handle, [this](QSize const& size) {
        setDestinationSize(size);
    });
    connect(viewport, &Viewport::sourceRectangleSet, handle, [this](QRectF const& rect) {
        setSourceRectangle(rect);
    });
    connect(viewport, &Viewport::resourceDestroyed, handle, [this] {
        setDestinationSize(QSize());
        setSourceRectangle(QRectF());
    });
}

void Surface::Private::addPresentationFeedback(PresentationFeedback* feedback) const
{
    pending.feedbacks->add(feedback);
}

void Surface::Private::installPointerConstraint(LockedPointerV1* lock)
{
    Q_ASSERT(lockedPointer.isNull());
    Q_ASSERT(confinedPointer.isNull());
    lockedPointer = lock;

    auto cleanUp = [this]() {
        lockedPointer.clear();
        disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        Q_EMIT handle->pointerConstraintsChanged();
    };

    if (lock->lifeTime() == LockedPointerV1::LifeTime::OneShot) {
        constrainsOneShotConnection
            = QObject::connect(lock, &LockedPointerV1::lockedChanged, handle, [this, cleanUp] {
                  if (lockedPointer.isNull() || lockedPointer->isLocked()) {
                      return;
                  }
                  cleanUp();
              });
    }
    constrainsUnboundConnection
        = QObject::connect(lock, &LockedPointerV1::resourceDestroyed, handle, [this, cleanUp] {
              if (lockedPointer.isNull()) {
                  return;
              }
              cleanUp();
          });
    Q_EMIT handle->pointerConstraintsChanged();
}

void Surface::Private::installPointerConstraint(ConfinedPointerV1* confinement)
{
    Q_ASSERT(lockedPointer.isNull());
    Q_ASSERT(confinedPointer.isNull());
    confinedPointer = confinement;

    auto cleanUp = [this]() {
        confinedPointer.clear();
        disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        Q_EMIT handle->pointerConstraintsChanged();
    };

    if (confinement->lifeTime() == ConfinedPointerV1::LifeTime::OneShot) {
        constrainsOneShotConnection = QObject::connect(
            confinement, &ConfinedPointerV1::confinedChanged, handle, [this, cleanUp] {
                if (confinedPointer.isNull() || confinedPointer->isConfined()) {
                    return;
                }
                cleanUp();
            });
    }
    constrainsUnboundConnection = QObject::connect(
        confinement, &ConfinedPointerV1::resourceDestroyed, handle, [this, cleanUp] {
            if (confinedPointer.isNull()) {
                return;
            }
            cleanUp();
        });
    Q_EMIT handle->pointerConstraintsChanged();
}

void Surface::Private::installIdleInhibitor(IdleInhibitor* inhibitor)
{
    idleInhibitors << inhibitor;
    QObject::connect(inhibitor, &IdleInhibitor::resourceDestroyed, handle, [this, inhibitor] {
        idleInhibitors.removeOne(inhibitor);
        if (idleInhibitors.isEmpty()) {
            Q_EMIT handle->inhibitsIdleChanged();
        }
    });
    if (idleInhibitors.count() == 1) {
        Q_EMIT handle->inhibitsIdleChanged();
    }
}

const struct wl_surface_interface Surface::Private::s_interface = {
    destroyCallback,
    attachCallback,
    damageCallback,
    frameCallback,
    opaqueRegionCallback,
    inputRegionCallback,
    commitCallback,
    bufferTransformCallback,
    bufferScaleCallback,
    damageBufferCallback,
    // TODO(romangg): Update protocol version for offset callback (currently at 4).
    // NOLINTNEXTLINE(clang-diagnostic-missing-field-initializers)
};

Surface::Surface(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

surface_state const& Surface::state() const
{
    return d_ptr->current.pub;
}

void Surface::frameRendered(quint32 msec)
{
    while (!d_ptr->current.callbacks.empty()) {
        auto resource = d_ptr->current.callbacks.front();
        d_ptr->current.callbacks.pop_front();
        wl_callback_send_done(resource, msec);
        wl_resource_destroy(resource);
    }
    for (auto& subsurface : d_ptr->current.pub.children) {
        subsurface->d_ptr->surface->frameRendered(msec);
    }
}

bool Surface::Private::has_role() const
{
    auto const has_xdg_shell_role
        = shellSurface && (shellSurface->d_ptr->toplevel || shellSurface->d_ptr->popup);
    return has_xdg_shell_role || subsurface || layer_surface;
}

void Surface::Private::soureRectangleIntegerCheck(QSize const& destinationSize,
                                                  QRectF const& sourceRectangle) const
{
    if (destinationSize.isValid()) {
        // Source rectangle must be integer valued only when the destination size is not set.
        return;
    }
    if (!sourceRectangle.isValid()) {
        // When the source rectangle is unset there is no integer condition.
        return;
    }
    Q_ASSERT(viewport);

    double const width = sourceRectangle.width();
    double const height = sourceRectangle.height();

    if (!qFuzzyCompare(width, static_cast<int>(width))
        || !qFuzzyCompare(height, static_cast<int>(height))) {
        viewport->d_ptr->postError(WP_VIEWPORT_ERROR_BAD_SIZE,
                                   "Source rectangle not integer valued");
    }
}

void Surface::Private::soureRectangleContainCheck(Buffer const* buffer,
                                                  Output::Transform transform,
                                                  qint32 scale,
                                                  QRectF const& sourceRectangle) const
{
    if (!buffer || !viewport || !sourceRectangle.isValid()) {
        return;
    }
    QSizeF bufferSize = buffer->size() / scale;

    if (transform == Output::Transform::Rotated90 || transform == Output::Transform::Rotated270
        || transform == Output::Transform::Flipped90
        || transform == Output::Transform::Flipped270) {
        bufferSize.transpose();
    }

    if (!QRectF(QPointF(), bufferSize).contains(sourceRectangle)) {
        viewport->d_ptr->postError(WP_VIEWPORT_ERROR_OUT_OF_BUFFER,
                                   "Source rectangle not contained in buffer");
    }
}

void Surface::Private::synced_child_update()
{
    current.pub.updates |= surface_change::children;

    if (subsurface && subsurface->isSynchronized() && subsurface->parentSurface()) {
        subsurface->parentSurface()->d_ptr->synced_child_update();
    }
}

void Surface::Private::update_buffer(SurfaceState const& source, bool& resized)
{
    if (!(source.pub.updates & surface_change::buffer)) {
        // TODO(romangg): Should we set the pending damage even when no new buffer got attached?
        current.pub.damage = {};
        current.bufferDamage = {};
        return;
    }

    QSize oldSize;

    auto const was_mapped = current.pub.buffer != nullptr;
    auto const now_mapped = source.pub.buffer != nullptr;

    if (was_mapped) {
        oldSize = current.pub.buffer->size();
    }

    current.pub.buffer = source.pub.buffer;

    if (was_mapped != now_mapped) {
        current.pub.updates |= surface_change::mapped;
    }

    if (!now_mapped) {
        if (subsurface && subsurface->isSynchronized() && subsurface->parentSurface()) {
            subsurface->parentSurface()->d_ptr->synced_child_update();
        }
        return;
    }

    current.pub.buffer->setCommitted();

    current.pub.offset = source.pub.offset;
    current.pub.damage = source.pub.damage;
    current.bufferDamage = source.bufferDamage;

    auto const newSize = current.pub.buffer->size();
    resized = newSize.isValid() && newSize != oldSize;

    if (current.pub.damage.isEmpty() && current.bufferDamage.isEmpty()) {
        // No damage submitted yet for the new buffer.

        // TODO(romangg): Does this mean size is not change, i.e. return false always?
        return;
    }

    auto const surfaceSize = handle->size();
    auto const surfaceRegion = QRegion(0, 0, surfaceSize.width(), surfaceSize.height());
    if (surfaceRegion.isEmpty()) {
        return;
    }

    auto bufferDamage = QRegion();

    if (!current.bufferDamage.isEmpty()) {
        auto const tr = current.pub.transform;
        auto const sc = current.pub.scale;

        using Tr = Output::Transform;
        if (tr == Tr::Rotated90 || tr == Tr::Rotated270 || tr == Tr::Flipped90
            || tr == Tr::Flipped270) {

            // Calculate transformed + scaled buffer damage.
            for (auto const& rect : current.bufferDamage) {
                auto const add
                    = QRegion(rect.x() / sc, rect.y() / sc, rect.height() / sc, rect.width() / sc);
                bufferDamage = bufferDamage.united(add);
            }

        } else if (sc != 1) {

            // Calculate scaled buffer damage.
            for (auto const& rect : current.bufferDamage) {
                auto const add
                    = QRegion(rect.x() / sc, rect.y() / sc, rect.width() / sc, rect.height() / sc);
                bufferDamage = bufferDamage.united(add);
            }

        } else {
            bufferDamage = current.bufferDamage;
        }
    }

    current.pub.damage = surfaceRegion.intersected(current.pub.damage.united(bufferDamage));
    trackedDamage = trackedDamage.united(current.pub.damage);
}

void Surface::Private::copy_to_current(SurfaceState const& source, bool& resized)
{
    if (source.pub.updates & surface_change::children) {
        current.pub.children = source.pub.children;
    }
    current.callbacks.insert(
        current.callbacks.end(), source.callbacks.begin(), source.callbacks.end());
    if (!current.callbacks.empty()) {
        current.pub.updates |= surface_change::frame;
    }

    if (source.pub.updates & surface_change::shadow) {
        current.pub.shadow = source.pub.shadow;
    }
    if (source.pub.updates & surface_change::blur) {
        current.pub.blur = source.pub.blur;
    }
    if (source.pub.updates & surface_change::contrast) {
        current.pub.contrast = source.pub.contrast;
    }
    if (source.pub.updates & surface_change::slide) {
        current.pub.slide = source.pub.slide;
    }
    if (source.pub.updates & surface_change::input) {
        current.pub.input = source.pub.input;
        current.pub.input_is_infinite = source.pub.input_is_infinite;
    }
    if (source.pub.updates & surface_change::opaque) {
        current.pub.opaque = source.pub.opaque;
    }
    if (source.pub.updates & surface_change::scale) {
        current.pub.scale = source.pub.scale;
    }
    if (source.pub.updates & surface_change::transform) {
        current.pub.transform = source.pub.transform;
    }

    if (source.destinationSizeIsSet) {
        current.destinationSize = source.destinationSize;
        resized = current.pub.buffer != nullptr;
    }

    if (source.pub.updates & surface_change::source_rectangle) {
        if (current.pub.buffer && !source.destinationSize.isValid()
            && source.pub.source_rectangle.isValid()) {
            // TODO(unknown author): We should make this dependent on the previous size being
            //      different. But looking at above resized calculation when setting the buffer
            //      we need to do fix this there as well (does not look at buffer transform
            //      and destination size).
            resized = true;
        }
        current.pub.source_rectangle = source.pub.source_rectangle;
    }
}

void Surface::Private::updateCurrentState(bool forceChildren)
{
    updateCurrentState(pending, forceChildren);
}

void Surface::Private::updateCurrentState(SurfaceState& source, bool forceChildren)
{
    auto const scaleFactorChanged
        = (source.pub.updates & surface_change::scale) && (current.pub.scale != source.pub.scale);

    auto resized = false;
    current.pub.updates = source.pub.updates;

    update_buffer(source, resized);
    copy_to_current(source, resized);

    // Now check that source rectangle is (still) well defined.
    soureRectangleIntegerCheck(current.destinationSize, current.pub.source_rectangle);
    soureRectangleContainCheck(current.pub.buffer.get(),
                               current.pub.transform,
                               current.pub.scale,
                               current.pub.source_rectangle);

    if (!lockedPointer.isNull()) {
        lockedPointer->d_ptr->commit();
    }
    if (!confinedPointer.isNull()) {
        confinedPointer->d_ptr->commit();
    }

    if (scaleFactorChanged) {
        resized = current.pub.buffer != nullptr;
    }
    if (resized) {
        current.pub.updates |= surface_change::size;
    }

    current.feedbacks = std::move(source.feedbacks);

    source = SurfaceState();
    source.pub.children = current.pub.children;

    for (auto& subsurface : current.pub.children) {
        subsurface->d_ptr->applyCached(forceChildren);
    }
}

void Surface::Private::commit()
{
    if (subsurface) {
        // Surface has associated subsurface. We delegate committing to there.
        subsurface->d_ptr->commit();
        return;
    }

    updateCurrentState(false);

    if (shellSurface) {
        shellSurface->commit();
    }

    if (layer_surface && !layer_surface->d_ptr->commit()) {
        // Error on layer-surface commit.
        return;
    }

    Q_EMIT handle->committed();
}

void Surface::Private::damage(QRect const& rect)
{
    pending.pub.damage = pending.pub.damage.united(rect);
}

void Surface::Private::damageBuffer(QRect const& rect)
{
    pending.bufferDamage = pending.bufferDamage.united(rect);
}

void Surface::Private::setScale(qint32 scale)
{
    pending.pub.scale = scale;
    pending.pub.updates |= surface_change::scale;
}

void Surface::Private::setTransform(Output::Transform transform)
{
    pending.pub.transform = transform;
}

void Surface::Private::addFrameCallback(uint32_t callback)
{
    // TODO(unknown author): put the frame callback in a separate class inheriting Resource.
    wl_resource* frameCallback = client->createResource(&wl_callback_interface, 1, callback);
    if (!frameCallback) {
        wl_resource_post_no_memory(resource);
        return;
    }
    wl_resource_set_implementation(frameCallback, nullptr, this, destroyFrameCallback);
    pending.callbacks.push_back(frameCallback);
}

void Surface::Private::attachBuffer(wl_resource* wlBuffer, QPoint const& offset)
{
    had_buffer_attached = true;

    pending.pub.updates |= surface_change::buffer;
    pending.pub.offset = offset;

    if (!wlBuffer) {
        // Got a null buffer, deletes content in next frame.
        pending.pub.buffer.reset();
        pending.pub.damage = QRegion();
        pending.bufferDamage = QRegion();
        return;
    }

    pending.pub.buffer = Buffer::make(wlBuffer, q_ptr);

    QObject::connect(pending.pub.buffer.get(),
                     &Buffer::resourceDestroyed,
                     handle,
                     [this, buffer = pending.pub.buffer.get()]() {
                         if (pending.pub.buffer.get() == buffer) {
                             pending.pub.buffer.reset();
                         } else if (current.pub.buffer.get() == buffer) {
                             current.pub.buffer.reset();
                         } else if (subsurface
                                    && subsurface->d_ptr->cached.pub.buffer.get() == buffer) {
                             subsurface->d_ptr->cached.pub.buffer.reset();
                         }
                     });
}

void Surface::Private::destroyFrameCallback(wl_resource* wlResource)
{
    auto priv = static_cast<Private*>(wl_resource_get_user_data(wlResource));

    auto removeCallback = [wlResource](SurfaceState& state) {
        auto it = std::find(state.callbacks.begin(), state.callbacks.end(), wlResource);
        if (it != state.callbacks.end()) {
            state.callbacks.erase(it);
        }
    };

    removeCallback(priv->current);
    removeCallback(priv->pending);
    if (priv->subsurface) {
        removeCallback(priv->subsurface->d_ptr->cached);
    }
}

void Surface::Private::attachCallback([[maybe_unused]] wl_client* wlClient,
                                      wl_resource* wlResource,
                                      wl_resource* buffer,
                                      int32_t pos_x,
                                      int32_t pos_y)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->attachBuffer(buffer, QPoint(pos_x, pos_y));
}

void Surface::Private::damageCallback([[maybe_unused]] wl_client* wlClient,
                                      wl_resource* wlResource,
                                      int32_t pos_x,
                                      int32_t pos_y,
                                      int32_t width,
                                      int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->damage(QRect(pos_x, pos_y, width, height));
}

void Surface::Private::damageBufferCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            int32_t pos_x,
                                            int32_t pos_y,
                                            int32_t width,
                                            int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->damageBuffer(QRect(pos_x, pos_y, width, height));
}

void Surface::Private::frameCallback([[maybe_unused]] wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t callback)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->addFrameCallback(callback);
}

void Surface::Private::opaqueRegionCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            wl_resource* wlRegion)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto region = wlRegion ? Wayland::Resource<Region>::get_handle(wlRegion) : nullptr;
    priv->setOpaque(region ? region->region() : QRegion());
}

void Surface::Private::setOpaque(QRegion const& region)
{
    pending.pub.opaque = region;
    pending.pub.updates |= surface_change::opaque;
}

void Surface::Private::inputRegionCallback([[maybe_unused]] wl_client* wlClient,
                                           wl_resource* wlResource,
                                           wl_resource* wlRegion)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto region = wlRegion ? Wayland::Resource<Region>::get_handle(wlRegion) : nullptr;
    priv->setInput(region ? region->region() : QRegion(), !region);
}

void Surface::Private::setInput(QRegion const& region, bool isInfinite)
{
    pending.pub.input_is_infinite = isInfinite;
    pending.pub.input = region;
    pending.pub.updates |= surface_change::input;
}

void Surface::Private::commitCallback([[maybe_unused]] wl_client* wlClient, wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->commit();
}

void Surface::Private::bufferTransformCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               int32_t transform)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->setTransform(static_cast<Output::Transform>(transform));
}

void Surface::Private::bufferScaleCallback([[maybe_unused]] wl_client* wlClient,
                                           wl_resource* wlResource,
                                           int32_t scale)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->setScale(scale);
}

Subsurface* Surface::subsurface() const
{
    return d_ptr->subsurface;
}

QSize Surface::size() const
{
    if (!d_ptr->current.pub.buffer) {
        return {};
    }
    if (d_ptr->current.destinationSize.isValid()) {
        return d_ptr->current.destinationSize;
    }
    if (d_ptr->current.pub.source_rectangle.isValid()) {
        return d_ptr->current.pub.source_rectangle.size().toSize();
    }
    // TODO(unknown author): Apply transform to the buffer size.
    return d_ptr->current.pub.buffer->size() / d_ptr->current.pub.scale;
}

QRect Surface::expanse() const
{
    auto ret = QRect(QPoint(), size());

    for (auto const& sub : state().children) {
        ret = ret.united(sub->surface()->expanse().translated(sub->position()));
    }
    return ret;
}

bool Surface::isMapped() const
{
    if (d_ptr->subsurface) {
        // From the spec: "A sub-surface becomes mapped, when a non-NULL wl_buffer is applied and
        // the parent surface is mapped."
        return d_ptr->current.pub.buffer && d_ptr->subsurface->parentSurface()
            && d_ptr->subsurface->parentSurface()->isMapped();
    }
    return d_ptr->current.pub.buffer != nullptr;
}

QRegion Surface::trackedDamage() const
{
    return d_ptr->trackedDamage;
}

void Surface::resetTrackedDamage()
{
    d_ptr->trackedDamage = QRegion();
}

std::vector<WlOutput*> Surface::outputs() const
{
    return d_ptr->outputs;
}

void Surface::setOutputs(std::vector<Output*> const& outputs)
{
    std::vector<WlOutput*> wayland_outputs;
    wayland_outputs.reserve(outputs.size());

    for (auto const& output : outputs) {
        wayland_outputs.push_back(output->wayland_output());
    }
    setOutputs(wayland_outputs);
}

void Surface::setOutputs(std::vector<WlOutput*> const& outputs)
{
    auto removed_outputs = d_ptr->outputs;

    for (auto stays : outputs) {
        removed_outputs.erase(std::remove(removed_outputs.begin(), removed_outputs.end(), stays),
                              removed_outputs.end());
    }

    for (auto output : removed_outputs) {
        auto const binds = output->d_ptr->getBinds(d_ptr->client->handle);
        for (auto bind : binds) {
            d_ptr->send<wl_surface_send_leave>(bind->resource);
        }
        disconnect(d_ptr->outputDestroyedConnections.take(output));
    }

    auto added_outputs = outputs;
    for (auto keeping : d_ptr->outputs) {
        added_outputs.erase(std::remove(added_outputs.begin(), added_outputs.end(), keeping),
                            added_outputs.end());
    }

    for (auto output : added_outputs) {
        auto const binds = output->d_ptr->getBinds(d_ptr->client->handle);
        for (auto bind : binds) {
            d_ptr->send<wl_surface_send_enter>(bind->resource);
        }

        d_ptr->outputDestroyedConnections[output]
            = connect(output, &WlOutput::removed, this, [this, output] {
                  auto outputs = d_ptr->outputs;
                  bool removed = false;
                  outputs.erase(std::remove_if(outputs.begin(),
                                               outputs.end(),
                                               [&removed, output](WlOutput* out) {
                                                   if (output == out) {
                                                       removed = true;
                                                       return true;
                                                   }
                                                   return false;
                                               }),
                                outputs.end());

                  if (removed) {
                      setOutputs(outputs);
                  }
              });
    }
    // TODO(unknown author): send enter when the client binds the Output another time

    d_ptr->outputs = outputs;
}

QPointer<LockedPointerV1> Surface::lockedPointer() const
{
    return d_ptr->lockedPointer;
}

QPointer<ConfinedPointerV1> Surface::confinedPointer() const
{
    return d_ptr->confinedPointer;
}

bool Surface::inhibitsIdle() const
{
    return !d_ptr->idleInhibitors.isEmpty();
}

Client* Surface::client() const
{
    return d_ptr->client->handle;
}

wl_resource* Surface::resource() const
{
    return d_ptr->resource;
}

uint32_t Surface::id() const
{
    return d_ptr->id();
}

uint32_t Surface::lockPresentation(Output* output)
{
    if (!d_ptr->current.feedbacks) {
        return 0;
    }
    if (!d_ptr->current.feedbacks->active()) {
        return 0;
    }
    d_ptr->current.feedbacks->setOutput(output);

    if (++d_ptr->feedbackId == 0) {
        d_ptr->feedbackId++;
    }

    d_ptr->waitingFeedbacks[d_ptr->feedbackId] = std::move(d_ptr->current.feedbacks);
    return d_ptr->feedbackId;
}

void Surface::presentationFeedback(uint32_t presentationId,
                                   uint32_t tvSecHi,
                                   uint32_t tvSecLo,
                                   uint32_t tvNsec,
                                   uint32_t refresh,
                                   uint32_t seqHi,
                                   uint32_t seqLo,
                                   PresentationKinds kinds)
{
    auto feedbacksIt = d_ptr->waitingFeedbacks.find(presentationId);
    assert(feedbacksIt != d_ptr->waitingFeedbacks.end());

    feedbacksIt->second->presented(tvSecHi, tvSecLo, tvNsec, refresh, seqHi, seqLo, kinds);
    d_ptr->waitingFeedbacks.erase(feedbacksIt);
}

void Surface::presentationDiscarded(uint32_t presentationId)
{
    auto feedbacksIt = d_ptr->waitingFeedbacks.find(presentationId);
    assert(feedbacksIt != d_ptr->waitingFeedbacks.end());
    d_ptr->waitingFeedbacks.erase(feedbacksIt);
}

Feedbacks::Feedbacks(QObject* parent)
    : QObject(parent)
{
}

Feedbacks::~Feedbacks()
{
    discard();
}

bool Feedbacks::active()
{
    return !m_feedbacks.empty();
}

void Feedbacks::add(PresentationFeedback* feedback)
{
    connect(feedback, &PresentationFeedback::resourceDestroyed, this, [this, feedback] {
        m_feedbacks.erase(std::find(m_feedbacks.begin(), m_feedbacks.end(), feedback));
    });
    m_feedbacks.push_back(feedback);
}

void Feedbacks::setOutput(Output* output)
{
    assert(!m_output);
    m_output = output;
    QObject::connect(
        output->wayland_output(), &WlOutput::removed, this, &Feedbacks::handleOutputRemoval);
}

void Feedbacks::handleOutputRemoval()
{
    assert(m_output);
    m_output = nullptr;
    discard();
}

PresentationFeedback::Kinds toKinds(Surface::PresentationKinds kinds)
{
    using PresentationKind = Surface::PresentationKind;
    using FeedbackKind = PresentationFeedback::Kind;

    PresentationFeedback::Kinds ret;
    if (kinds.testFlag(PresentationKind::Vsync)) {
        ret |= FeedbackKind::Vsync;
    }
    if (kinds.testFlag(PresentationKind::HwClock)) {
        ret |= FeedbackKind::HwClock;
    }
    if (kinds.testFlag(PresentationKind::HwCompletion)) {
        ret |= FeedbackKind::HwCompletion;
    }
    if (kinds.testFlag(PresentationKind::ZeroCopy)) {
        ret |= FeedbackKind::ZeroCopy;
    }
    return ret;
}

void Feedbacks::presented(uint32_t tvSecHi,
                          uint32_t tvSecLo,
                          uint32_t tvNsec,
                          uint32_t refresh,
                          uint32_t seqHi,
                          uint32_t seqLo,
                          Surface::PresentationKinds kinds)
{
    std::for_each(m_feedbacks.begin(), m_feedbacks.end(), [=](PresentationFeedback* fb) {
        fb->sync(m_output);
        fb->presented(tvSecHi, tvSecLo, tvNsec, refresh, seqHi, seqLo, toKinds(kinds));
        delete fb;
    });
    m_feedbacks.clear();
}

void Feedbacks::discard()
{
    std::for_each(m_feedbacks.begin(), m_feedbacks.end(), [=](PresentationFeedback* feedback) {
        feedback->discarded();
        delete feedback;
    });
    m_feedbacks.clear();
}

}
