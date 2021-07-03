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

#include "client.h"
#include "compositor.h"
#include "idle_inhibit_v1.h"
#include "idle_inhibit_v1_p.h"
#include "layer_shell_v1_p.h"
#include "pointer_constraints_v1.h"
#include "pointer_constraints_v1_p.h"
#include "presentation_time.h"
#include "region.h"
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

Surface::Private::Private(Client* client, uint32_t version, uint32_t id, Surface* q)
    : Wayland::Resource<Surface>(client, version, id, &wl_surface_interface, &s_interface, q)
    , q_ptr{q}
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

    for (auto child : current.children) {
        child->d_ptr->parent = nullptr;
    }
    for (auto child : pending.children) {
        child->d_ptr->parent = nullptr;
    }
}

void Surface::Private::addChild(Subsurface* child)
{
    if (subsurface) {
        // We add it indiscriminately to the cached state. If the subsurface state is synchronized
        // on next parent commit it is added, if not it will be ignored here.
        subsurface->d_ptr->cached.children.push_back(child);
    }
    pending.children.push_back(child);
    pending.childrenChanged = true;

    // TODO(romangg): Should all below only be changed on commit?

    QObject::connect(
        child, &Subsurface::positionChanged, handle(), &Surface::subsurfaceTreeChanged);
    QObject::connect(
        child->surface(), &Surface::unmapped, handle(), &Surface::subsurfaceTreeChanged);
    QObject::connect(child->surface(),
                     &Surface::subsurfaceTreeChanged,
                     handle(),
                     &Surface::subsurfaceTreeChanged);

    Q_EMIT handle()->subsurfaceTreeChanged();
}

void Surface::Private::removeChild(Subsurface* child)
{
    if (subsurface) {
        auto& cached = subsurface->d_ptr->cached;
        cached.children.erase(std::remove(cached.children.begin(), cached.children.end(), child),
                              cached.children.end());
    }
    pending.children.erase(std::remove(pending.children.begin(), pending.children.end(), child),
                           pending.children.end());
    current.children.erase(std::remove(current.children.begin(), current.children.end(), child),
                           current.children.end());

    Q_EMIT handle()->subsurfaceTreeChanged();

    QObject::disconnect(
        child, &Subsurface::positionChanged, handle(), &Surface::subsurfaceTreeChanged);

    if (child->surface()) {
        QObject::disconnect(
            child->surface(), &Surface::unmapped, handle(), &Surface::subsurfaceTreeChanged);
        QObject::disconnect(child->surface(),
                            &Surface::subsurfaceTreeChanged,
                            handle(),
                            &Surface::subsurfaceTreeChanged);
    }
}

bool Surface::Private::raiseChild(Subsurface* subsurface, Surface* sibling)
{
    auto it = std::find(pending.children.begin(), pending.children.end(), subsurface);

    if (it == pending.children.end()) {
        return false;
    }

    if (pending.children.size() == 1) {
        // Nothing to do.
        return true;
    }

    if (sibling == handle()) {
        // It's sibling to the parent, so needs to become last item.
        pending.children.erase(it);
        pending.children.push_back(subsurface);
        pending.childrenChanged = true;
        return true;
    }

    if (!sibling->subsurface()) {
        // Not a sub surface.
        return false;
    }

    auto siblingIt
        = std::find(pending.children.begin(), pending.children.end(), sibling->subsurface());
    if (siblingIt == pending.children.end() || siblingIt == it) {
        // Not a sibling.
        return false;
    }

    auto value = (*it);
    pending.children.erase(it);

    // Find the iterator again.
    siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subsurface());
    pending.children.insert(++siblingIt, value);
    pending.childrenChanged = true;
    return true;
}

bool Surface::Private::lowerChild(Subsurface* subsurface, Surface* sibling)
{
    auto it = std::find(pending.children.begin(), pending.children.end(), subsurface);
    if (it == pending.children.end()) {
        return false;
    }
    if (pending.children.size() == 1) {
        // nothing to do
        return true;
    }
    if (sibling == handle()) {
        // it's to the parent, so needs to become first item
        auto value = *it;
        pending.children.erase(it);
        pending.children.insert(pending.children.begin(), value);
        pending.childrenChanged = true;
        return true;
    }
    if (!sibling->subsurface()) {
        // not a sub surface
        return false;
    }
    auto siblingIt
        = std::find(pending.children.begin(), pending.children.end(), sibling->subsurface());
    if (siblingIt == pending.children.end() || siblingIt == it) {
        // not a sibling
        return false;
    }
    auto value = (*it);
    pending.children.erase(it);
    // find the iterator again
    siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subsurface());
    pending.children.insert(siblingIt, value);
    pending.childrenChanged = true;
    return true;
}

void Surface::Private::setShadow(const QPointer<Shadow>& shadow)
{
    pending.shadow = shadow;
    pending.shadowIsSet = true;
}

void Surface::Private::setBlur(const QPointer<Blur>& blur)
{
    pending.blur = blur;
    pending.blurIsSet = true;
}

void Surface::Private::setSlide(const QPointer<Slide>& slide)
{
    pending.slide = slide;
    pending.slideIsSet = true;
}

void Surface::Private::setContrast(const QPointer<Contrast>& contrast)
{
    pending.contrast = contrast;
    pending.contrastIsSet = true;
}

void Surface::Private::setSourceRectangle(const QRectF& source)
{
    pending.sourceRectangle = source;
    pending.sourceRectangleIsSet = true;
}

void Surface::Private::setDestinationSize(const QSize& dest)
{
    pending.destinationSize = dest;
    pending.destinationSizeIsSet = true;
}

void Surface::Private::installViewport(Viewport* vp)
{
    Q_ASSERT(viewport.isNull());
    viewport = QPointer<Viewport>(vp);
    connect(viewport, &Viewport::destinationSizeSet, handle(), [this](const QSize& size) {
        setDestinationSize(size);
    });
    connect(viewport, &Viewport::sourceRectangleSet, handle(), [this](const QRectF& rect) {
        setSourceRectangle(rect);
    });
    connect(viewport, &Viewport::resourceDestroyed, handle(), [this] {
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
    lockedPointer = QPointer<LockedPointerV1>(lock);

    auto cleanUp = [this]() {
        lockedPointer.clear();
        disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        Q_EMIT handle()->pointerConstraintsChanged();
    };

    if (lock->lifeTime() == LockedPointerV1::LifeTime::OneShot) {
        constrainsOneShotConnection
            = QObject::connect(lock, &LockedPointerV1::lockedChanged, handle(), [this, cleanUp] {
                  if (lockedPointer.isNull() || lockedPointer->isLocked()) {
                      return;
                  }
                  cleanUp();
              });
    }
    constrainsUnboundConnection
        = QObject::connect(lock, &LockedPointerV1::resourceDestroyed, handle(), [this, cleanUp] {
              if (lockedPointer.isNull()) {
                  return;
              }
              cleanUp();
          });
    Q_EMIT handle()->pointerConstraintsChanged();
}

void Surface::Private::installPointerConstraint(ConfinedPointerV1* confinement)
{
    Q_ASSERT(lockedPointer.isNull());
    Q_ASSERT(confinedPointer.isNull());
    confinedPointer = QPointer<ConfinedPointerV1>(confinement);

    auto cleanUp = [this]() {
        confinedPointer.clear();
        disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        Q_EMIT handle()->pointerConstraintsChanged();
    };

    if (confinement->lifeTime() == ConfinedPointerV1::LifeTime::OneShot) {
        constrainsOneShotConnection = QObject::connect(
            confinement, &ConfinedPointerV1::confinedChanged, handle(), [this, cleanUp] {
                if (confinedPointer.isNull() || confinedPointer->isConfined()) {
                    return;
                }
                cleanUp();
            });
    }
    constrainsUnboundConnection = QObject::connect(
        confinement, &ConfinedPointerV1::resourceDestroyed, handle(), [this, cleanUp] {
            if (confinedPointer.isNull()) {
                return;
            }
            cleanUp();
        });
    Q_EMIT handle()->pointerConstraintsChanged();
}

void Surface::Private::installIdleInhibitor(IdleInhibitor* inhibitor)
{
    idleInhibitors << inhibitor;
    QObject::connect(inhibitor, &IdleInhibitor::resourceDestroyed, handle(), [this, inhibitor] {
        idleInhibitors.removeOne(inhibitor);
        if (idleInhibitors.isEmpty()) {
            Q_EMIT handle()->inhibitsIdleChanged();
        }
    });
    if (idleInhibitors.count() == 1) {
        Q_EMIT handle()->inhibitsIdleChanged();
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
};

Surface::Surface(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

void Surface::frameRendered(quint32 msec)
{
    // Notify all callbacks.
    const bool needsFlush = !d_ptr->current.callbacks.empty();
    while (!d_ptr->current.callbacks.empty()) {
        auto resource = d_ptr->current.callbacks.front();
        d_ptr->current.callbacks.pop_front();
        wl_callback_send_done(resource, msec);
        wl_resource_destroy(resource);
    }
    for (auto& subsurface : d_ptr->current.children) {
        subsurface->d_ptr->surface->frameRendered(msec);
    }
    if (needsFlush) {
        d_ptr->client()->flush();
    }
}

bool Surface::Private::has_role() const
{
    auto const has_xdg_shell_role
        = shellSurface && (shellSurface->d_ptr->toplevel || shellSurface->d_ptr->popup);
    return has_xdg_shell_role || subsurface || layer_surface;
}

void Surface::Private::soureRectangleIntegerCheck(const QSize& destinationSize,
                                                  const QRectF& sourceRectangle) const
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

    const double width = sourceRectangle.width();
    const double height = sourceRectangle.height();

    if (!qFuzzyCompare(width, static_cast<int>(width))
        || !qFuzzyCompare(height, static_cast<int>(height))) {
        viewport->d_ptr->postError(WP_VIEWPORT_ERROR_BAD_SIZE,
                                   "Source rectangle not integer valued");
    }
}

void Surface::Private::soureRectangleContainCheck(const Buffer* buffer,
                                                  Output::Transform transform,
                                                  qint32 scale,
                                                  const QRectF& sourceRectangle) const
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

void Surface::Private::update_buffer(SurfaceState const& source, bool& damaged, bool& resized)
{
    if (!source.bufferIsSet) {
        return;
    }

    QSize oldSize;

    auto const wasMapped = current.buffer != nullptr;
    if (wasMapped) {
        oldSize = current.buffer->size();

        QObject::disconnect(
            current.buffer.get(), &Buffer::sizeChanged, handle(), &Surface::sizeChanged);
    }

    current.buffer = source.buffer;

    if (!current.buffer) {
        if (wasMapped) {
            Q_EMIT handle()->unmapped();
        }
        return;
    }

    current.buffer->setCommitted();
    QObject::connect(current.buffer.get(), &Buffer::sizeChanged, handle(), &Surface::sizeChanged);

    current.offset = source.offset;
    current.damage = source.damage;
    current.bufferDamage = source.bufferDamage;

    auto const newSize = current.buffer->size();
    resized = newSize.isValid() && newSize != oldSize;

    if (current.damage.isEmpty() && current.bufferDamage.isEmpty()) {
        // No damage submitted yet for the new buffer.

        // TODO(romangg): Does this mean size is not change, i.e. return false always?
        return;
    }

    auto const surfaceSize = handle()->size();
    auto const surfaceRegion = QRegion(0, 0, surfaceSize.width(), surfaceSize.height());
    if (surfaceRegion.isEmpty()) {
        return;
    }

    auto bufferDamage = QRegion();

    if (!current.bufferDamage.isEmpty()) {
        auto const tr = current.transform;
        auto const sc = current.scale;

        using Tr = Output::Transform;
        if (tr == Tr::Rotated90 || tr == Tr::Rotated270 || tr == Tr::Flipped90
            || tr == Tr::Flipped270) {

            // Calculate transformed + scaled buffer damage.
            for (const auto& rect : current.bufferDamage) {
                const auto add
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

    current.damage = surfaceRegion.intersected(current.damage.united(bufferDamage));
    trackedDamage = trackedDamage.united(current.damage);
    damaged = true;
}

void Surface::Private::copy_to_current(SurfaceState const& source, bool& resized)
{
    if (source.childrenChanged) {
        current.children = source.children;
    }
    current.callbacks.insert(
        current.callbacks.end(), source.callbacks.begin(), source.callbacks.end());

    if (source.shadowIsSet) {
        current.shadow = source.shadow;
    }
    if (source.blurIsSet) {
        current.blur = source.blur;
    }
    if (source.contrastIsSet) {
        current.contrast = source.contrast;
    }
    if (source.slideIsSet) {
        current.slide = source.slide;
    }
    if (source.inputIsSet) {
        current.input = source.input;
        current.inputIsInfinite = source.inputIsInfinite;
    }
    if (source.opaqueIsSet) {
        current.opaque = source.opaque;
    }
    if (source.scaleIsSet) {
        current.scale = source.scale;
    }
    if (source.transformIsSet) {
        current.transform = source.transform;
    }

    if (source.destinationSizeIsSet) {
        current.destinationSize = source.destinationSize;
        resized = current.buffer != nullptr;
    }

    if (source.sourceRectangleIsSet) {
        if (current.buffer && !source.destinationSize.isValid()
            && source.sourceRectangle.isValid()) {
            // TODO(unknown author): We should make this dependent on the previous size being
            //      different. But looking at above resized calculation when setting the buffer
            //      we need to do fix this there as well (does not look at buffer transform
            //      and destination size).
            resized = true;
        }
        current.sourceRectangle = source.sourceRectangle;
    }
}

void Surface::Private::updateCurrentState(bool forceChildren)
{
    updateCurrentState(pending, forceChildren);
}

void Surface::Private::updateCurrentState(SurfaceState& source, bool forceChildren)
{
    auto const scaleFactorChanged = source.scaleIsSet && (current.scale != source.scale);
    auto const transformChanged = source.transformIsSet && (current.transform != source.transform);

    auto damaged = false;
    auto resized = false;

    update_buffer(source, damaged, resized);
    copy_to_current(source, resized);

    // Now check that source rectangle is (still) well defined.
    soureRectangleIntegerCheck(current.destinationSize, current.sourceRectangle);
    soureRectangleContainCheck(
        current.buffer.get(), current.transform, current.scale, current.sourceRectangle);

    if (!lockedPointer.isNull()) {
        lockedPointer->d_ptr->commit();
    }
    if (!confinedPointer.isNull()) {
        confinedPointer->d_ptr->commit();
    }

    if (source.opaqueIsSet) {
        Q_EMIT handle()->opaqueChanged(current.opaque);
    }
    if (source.inputIsSet) {
        Q_EMIT handle()->inputChanged(current.input);
    }

    if (scaleFactorChanged) {
        Q_EMIT handle()->scaleChanged(current.scale);
        resized = current.buffer != nullptr;
    }
    if (transformChanged) {
        Q_EMIT handle()->transformChanged(current.transform);
    }
    if (resized) {
        Q_EMIT handle()->sizeChanged();
    }

    if (source.shadowIsSet) {
        Q_EMIT handle()->shadowChanged();
    }
    if (source.blurIsSet) {
        Q_EMIT handle()->blurChanged();
    }
    if (source.contrastIsSet) {
        Q_EMIT handle()->contrastChanged();
    }
    if (source.slideIsSet) {
        Q_EMIT handle()->slideOnShowHideChanged();
    }
    if (source.sourceRectangleIsSet) {
        Q_EMIT handle()->sourceRectangleChanged();
    }
    if (source.childrenChanged) {
        Q_EMIT handle()->subsurfaceTreeChanged();
    }

    current.feedbacks = std::move(source.feedbacks);

    if (damaged) {
        Q_EMIT handle()->damaged(current.damage);
    }

    source = SurfaceState();
    source.children = current.children;

    for (auto& subsurface : current.children) {
        subsurface->d_ptr->applyCached(forceChildren);
    }
}

void Surface::Private::commit()
{
    if (subsurface) {
        // Surface has associated subsurface. We delegate committing to there.
        subsurface->d_ptr->commit();
        Q_EMIT handle()->committed();
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

    Q_EMIT handle()->committed();
}

void Surface::Private::damage(const QRect& rect)
{
    pending.damage = pending.damage.united(rect);
}

void Surface::Private::damageBuffer(const QRect& rect)
{
    if (!pending.bufferIsSet || (pending.bufferIsSet && !pending.buffer)) {
        // TODO(unknown author): should we send an error?
        return;
    }
    pending.bufferDamage = pending.bufferDamage.united(rect);
}

void Surface::Private::setScale(qint32 scale)
{
    pending.scale = scale;
    pending.scaleIsSet = true;
}

void Surface::Private::setTransform(Output::Transform transform)
{
    pending.transform = transform;
}

void Surface::Private::addFrameCallback(uint32_t callback)
{
    // TODO(unknown author): put the frame callback in a separate class inheriting Resource.
    wl_resource* frameCallback = client()->createResource(&wl_callback_interface, 1, callback);
    if (!frameCallback) {
        wl_resource_post_no_memory(resource());
        return;
    }
    wl_resource_set_implementation(frameCallback, nullptr, this, destroyFrameCallback);
    pending.callbacks.push_back(frameCallback);
}

void Surface::Private::attachBuffer(wl_resource* wlBuffer, const QPoint& offset)
{
    had_buffer_attached = true;

    pending.bufferIsSet = true;
    pending.offset = offset;

    if (!wlBuffer) {
        // Got a null buffer, deletes content in next frame.
        pending.buffer.reset();
        pending.damage = QRegion();
        pending.bufferDamage = QRegion();
        return;
    }

    pending.buffer = Buffer::make(wlBuffer, q_ptr);

    QObject::connect(pending.buffer.get(),
                     &Buffer::resourceDestroyed,
                     handle(),
                     [this, buffer = pending.buffer.get()]() {
                         if (pending.buffer.get() == buffer) {
                             pending.buffer.reset();
                         } else if (current.buffer.get() == buffer) {
                             current.buffer.reset();
                         } else if (subsurface
                                    && subsurface->d_ptr->cached.buffer.get() == buffer) {
                             subsurface->d_ptr->cached.buffer.reset();
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
                                      int32_t sx,
                                      int32_t sy)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->attachBuffer(buffer, QPoint(sx, sy));
}

void Surface::Private::damageCallback([[maybe_unused]] wl_client* wlClient,
                                      wl_resource* wlResource,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->damage(QRect(x, y, width, height));
}

void Surface::Private::damageBufferCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->damageBuffer(QRect(x, y, width, height));
}

void Surface::Private::frameCallback([[maybe_unused]] wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t callback)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->addFrameCallback(callback);
}

void Surface::Private::opaqueRegionCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            wl_resource* wlRegion)
{
    auto priv = handle(wlResource)->d_ptr;
    auto region = wlRegion ? Wayland::Resource<Region>::handle(wlRegion) : nullptr;
    priv->setOpaque(region ? region->region() : QRegion());
}

void Surface::Private::setOpaque(const QRegion& region)
{
    pending.opaqueIsSet = true;
    pending.opaque = region;
}

void Surface::Private::inputRegionCallback([[maybe_unused]] wl_client* wlClient,
                                           wl_resource* wlResource,
                                           wl_resource* wlRegion)
{
    auto priv = handle(wlResource)->d_ptr;
    auto region = wlRegion ? Wayland::Resource<Region>::handle(wlRegion) : nullptr;
    priv->setInput(region ? region->region() : QRegion(), !region);
}

void Surface::Private::setInput(const QRegion& region, bool isInfinite)
{
    pending.inputIsSet = true;
    pending.inputIsInfinite = isInfinite;
    pending.input = region;
}

void Surface::Private::commitCallback([[maybe_unused]] wl_client* wlClient, wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->commit();
}

void Surface::Private::bufferTransformCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               int32_t transform)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->setTransform(Output::Transform(transform));
}

void Surface::Private::bufferScaleCallback([[maybe_unused]] wl_client* wlClient,
                                           wl_resource* wlResource,
                                           int32_t scale)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->setScale(scale);
}

QRegion Surface::damage() const
{
    return d_ptr->current.damage;
}

QRegion Surface::opaque() const
{
    return d_ptr->current.opaque;
}

QRegion Surface::input() const
{
    return d_ptr->current.input;
}

bool Surface::inputIsInfinite() const
{
    return d_ptr->current.inputIsInfinite;
}

qint32 Surface::scale() const
{
    return d_ptr->current.scale;
}

Output::Transform Surface::transform() const
{
    return d_ptr->current.transform;
}

std::shared_ptr<Buffer> Surface::buffer() const
{
    return d_ptr->current.buffer;
}

QPoint Surface::offset() const
{
    return d_ptr->current.offset;
}

QRectF Surface::sourceRectangle() const
{
    return d_ptr->current.sourceRectangle;
}

std::vector<Subsurface*> Surface::childSubsurfaces() const
{
    return d_ptr->current.children;
}

Subsurface* Surface::subsurface() const
{
    return d_ptr->subsurface;
}

QSize Surface::size() const
{
    if (!d_ptr->current.buffer) {
        return QSize();
    }
    if (d_ptr->current.destinationSize.isValid()) {
        return d_ptr->current.destinationSize;
    }
    if (d_ptr->current.sourceRectangle.isValid()) {
        return d_ptr->current.sourceRectangle.size().toSize();
    }
    // TODO(unknown author): Apply transform to the buffer size.
    return d_ptr->current.buffer->size() / scale();
}

QRect Surface::expanse() const
{
    auto ret = QRect(QPoint(), size());

    for (auto const& sub : childSubsurfaces()) {
        ret = ret.united(sub->surface()->expanse().translated(sub->position()));
    }
    return ret;
}

QPointer<Shadow> Surface::shadow() const
{
    return d_ptr->current.shadow;
}

QPointer<Blur> Surface::blur() const
{
    return d_ptr->current.blur;
}

QPointer<Contrast> Surface::contrast() const
{
    return d_ptr->current.contrast;
}

QPointer<Slide> Surface::slideOnShowHide() const
{
    return d_ptr->current.slide;
}

bool Surface::isMapped() const
{
    if (d_ptr->subsurface) {
        // From the spec: "A sub-surface becomes mapped, when a non-NULL wl_buffer is applied and
        // the parent surface is mapped."
        return d_ptr->current.buffer && d_ptr->subsurface->parentSurface()
            && d_ptr->subsurface->parentSurface()->isMapped();
    }
    return d_ptr->current.buffer != nullptr;
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
        auto const binds = output->d_ptr->getBinds(d_ptr->client()->handle());
        for (auto bind : binds) {
            d_ptr->send<wl_surface_send_leave>(bind->resource());
        }
        disconnect(d_ptr->outputDestroyedConnections.take(output));
    }

    auto added_outputs = outputs;
    for (auto keeping : d_ptr->outputs) {
        added_outputs.erase(std::remove(added_outputs.begin(), added_outputs.end(), keeping),
                            added_outputs.end());
    }

    for (auto output : added_outputs) {
        auto const binds = output->d_ptr->getBinds(d_ptr->client()->handle());
        for (auto bind : binds) {
            d_ptr->send<wl_surface_send_enter>(bind->resource());
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

void Surface::setDataProxy(Surface* surface)
{
    d_ptr->dataProxy = surface;
}

Surface* Surface::dataProxy() const
{
    return d_ptr->dataProxy;
}

Client* Surface::client() const
{
    return d_ptr->client()->handle();
}

wl_resource* Surface::resource() const
{
    return d_ptr->resource();
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
