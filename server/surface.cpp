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

#include "compositor.h"
#include "idle_inhibit_v1.h"
#include "idle_inhibit_v1_p.h"
#include "output_p.h"
#include "pointer_constraints_v1.h"
#include "pointer_constraints_v1_p.h"
#include "region.h"
#include "subcompositor.h"
#include "subsurface_p.h"
#include "viewporter_p.h"
#include "xdg_shell_surface.h"

#include <QListIterator>

#include <algorithm>
#include <wayland-server.h>
#include <wayland-viewporter-server-protocol.h>

namespace Wrapland
{
namespace Server
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
    QList<wl_resource*> callbacksToDestroy;
    callbacksToDestroy << current.callbacks;
    current.callbacks.clear();

    callbacksToDestroy << pending.callbacks;
    pending.callbacks.clear();

    callbacksToDestroy << subsurfacePending.callbacks;
    subsurfacePending.callbacks.clear();

    for (auto it = callbacksToDestroy.cbegin(), end = callbacksToDestroy.cend(); it != end; it++) {
        wl_resource_destroy(*it);
    }

    if (current.buffer) {
        current.buffer->unref();
    }

    if (subsurface) {
        subsurface->d_ptr->surface = nullptr;
    }

    for (auto child : current.children) {
        child->d_ptr->parent = nullptr;
    }
    for (auto child : pending.children) {
        child->d_ptr->parent = nullptr;
    }
    for (auto child : subsurfacePending.children) {
        child->d_ptr->parent = nullptr;
    }
}

void Surface::Private::addChild(Subsurface* child)
{
    // Protocol is not precise on how to handle the addition of new subsurfaces.
    pending.children.push_back(child);
    subsurfacePending.children.push_back(child);
    current.children.push_back(child);

    Q_EMIT handle()->subsurfaceTreeChanged();

    QObject::connect(
        child, &Subsurface::positionChanged, handle(), &Surface::subsurfaceTreeChanged);

    QObject::connect(
        child->surface(), &Surface::damaged, handle(), &Surface::subsurfaceTreeChanged);
    QObject::connect(
        child->surface(), &Surface::unmapped, handle(), &Surface::subsurfaceTreeChanged);
    QObject::connect(child->surface(),
                     &Surface::subsurfaceTreeChanged,
                     handle(),
                     &Surface::subsurfaceTreeChanged);
}

void Surface::Private::removeChild(Subsurface* child)
{
    // Protocol is not precise on how to handle the removal of new subsurfaces.
    pending.children.erase(std::remove(pending.children.begin(), pending.children.end(), child),
                           pending.children.end());
    subsurfacePending.children.erase(
        std::remove(subsurfacePending.children.begin(), subsurfacePending.children.end(), child),
        subsurfacePending.children.end());
    current.children.erase(std::remove(current.children.begin(), current.children.end(), child),
                           current.children.end());

    Q_EMIT handle()->subsurfaceTreeChanged();

    QObject::disconnect(
        child, &Subsurface::positionChanged, handle(), &Surface::subsurfaceTreeChanged);

    if (child->surface()) {
        QObject::disconnect(
            child->surface(), &Surface::damaged, handle(), &Surface::subsurfaceTreeChanged);
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

void Surface::Private::setBlur(const QPointer<BlurInterface>& blur)
{
    pending.blur = blur;
    pending.blurIsSet = true;
}

void Surface::Private::setSlide(const QPointer<SlideInterface>& slide)
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

Surface::~Surface() = default;

void Surface::frameRendered(quint32 msec)
{

    // notify all callbacks
    const bool needsFlush = !d_ptr->current.callbacks.isEmpty();
    while (!d_ptr->current.callbacks.isEmpty()) {
        wl_resource* r = d_ptr->current.callbacks.takeFirst();
        wl_callback_send_done(r, msec);
        wl_resource_destroy(r);
    }
    for (auto it = d_ptr->current.children.cbegin(); it != d_ptr->current.children.cend(); ++it) {
        const auto& subsurface = *it;
        if (!subsurface || !subsurface->d_ptr->surface) {
            continue;
        }
        subsurface->d_ptr->surface->frameRendered(msec);
    }
    if (needsFlush) {
        d_ptr->client()->flush();
    }
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

    if (!qFuzzyCompare(width, (int)width) || !qFuzzyCompare(height, (int)height)) {
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

void Surface::Private::swapStates(State* source, State* target, bool emitChanged)
{
    bool bufferChanged = source->bufferIsSet;

    const bool opaqueRegionChanged = source->opaqueIsSet;
    const bool inputRegionChanged = source->inputIsSet;

    const bool scaleFactorChanged = source->scaleIsSet && (target->scale != source->scale);
    const bool transformChanged
        = source->transformIsSet && (target->transform != source->transform);

    const bool shadowChanged = source->shadowIsSet;
    const bool blurChanged = source->blurIsSet;
    const bool contrastChanged = source->contrastIsSet;
    const bool slideChanged = source->slideIsSet;

    const bool sourceRectangleChanged = source->sourceRectangleIsSet;
    const bool destinationSizeChanged = source->destinationSizeIsSet;

    const bool childrenChanged = source->childrenChanged;

    bool sizeChanged = false;
    auto buffer = target->buffer;

    if (bufferChanged) {
        // TODO: is the reffing correct for subsurfaces?

        QSize oldSize;
        if (target->buffer) {
            oldSize = target->buffer->size();

            if (emitChanged) {
                target->buffer->unref();
                QObject::disconnect(
                    target->buffer, &Buffer::sizeChanged, handle(), &Surface::sizeChanged);
            } else {
                delete target->buffer;
                target->buffer = nullptr;
            }
        }

        if (source->buffer) {
            if (emitChanged) {
                source->buffer->ref();
                QObject::connect(
                    source->buffer, &Buffer::sizeChanged, handle(), &Surface::sizeChanged);
            }
            const QSize newSize = source->buffer->size();
            sizeChanged = newSize.isValid() && newSize != oldSize;
        }

        if (!target->buffer && !source->buffer && emitChanged) {
            // Null buffer set on a not mapped surface, don't emit unmapped.
            bufferChanged = false;
        }

        buffer = source->buffer;
    }

    // Copy values.
    if (bufferChanged) {
        target->buffer = buffer;
        target->offset = source->offset;
        target->damage = source->damage;
        target->bufferDamage = source->bufferDamage;
        target->bufferIsSet = source->bufferIsSet;
    }
    if (childrenChanged) {
        target->childrenChanged = source->childrenChanged;
        target->children = source->children;
    }
    target->callbacks.append(source->callbacks);

    if (shadowChanged) {
        target->shadow = source->shadow;
        target->shadowIsSet = true;
    }
    if (blurChanged) {
        target->blur = source->blur;
        target->blurIsSet = true;
    }
    if (contrastChanged) {
        target->contrast = source->contrast;
        target->contrastIsSet = true;
    }
    if (slideChanged) {
        target->slide = source->slide;
        target->slideIsSet = true;
    }
    if (inputRegionChanged) {
        target->input = source->input;
        target->inputIsInfinite = source->inputIsInfinite;
        target->inputIsSet = true;
    }
    if (opaqueRegionChanged) {
        target->opaque = source->opaque;
        target->opaqueIsSet = true;
    }
    if (scaleFactorChanged) {
        target->scale = source->scale;
        target->scaleIsSet = true;
    }
    if (transformChanged) {
        target->transform = source->transform;
        target->transformIsSet = true;
    }

    if (destinationSizeChanged) {
        target->destinationSize = source->destinationSize;
        target->destinationSizeIsSet = true;
        sizeChanged |= static_cast<bool>(buffer);
    }
    if (sourceRectangleChanged) {
        if (buffer && !target->destinationSize.isValid() && source->sourceRectangle.isValid()) {
            // TODO: We should make this dependent on the previous size being different.
            //       But looking at above sizeChanged calculation when setting the buffer
            //       we need to do fix this there as well (does not look at buffer transform
            //       and destination size).
            sizeChanged = true;
        }
        target->sourceRectangle = source->sourceRectangle;
        target->sourceRectangleIsSet = true;
    }

    // Now check that source rectangle is (still) well defined.
    soureRectangleIntegerCheck(target->destinationSize, target->sourceRectangle);
    soureRectangleContainCheck(buffer, target->transform, target->scale, target->sourceRectangle);

    if (!lockedPointer.isNull()) {
        lockedPointer->d_ptr->commit();
    }
    if (!confinedPointer.isNull()) {
        confinedPointer->d_ptr->commit();
    }

    *source = State{};
    source->children = target->children;

    if (opaqueRegionChanged) {
        Q_EMIT handle()->opaqueChanged(target->opaque);
    }
    if (inputRegionChanged) {
        Q_EMIT handle()->inputChanged(target->input);
    }

    if (scaleFactorChanged) {
        Q_EMIT handle()->scaleChanged(target->scale);
        if (buffer && !sizeChanged) {
            Q_EMIT handle()->sizeChanged();
        }
    }
    if (transformChanged) {
        Q_EMIT handle()->transformChanged(target->transform);
    }

    if (bufferChanged && emitChanged) {
        if (target->buffer && (!target->damage.isEmpty() || !target->bufferDamage.isEmpty())) {
            const QRegion windowRegion
                = QRegion(0, 0, handle()->size().width(), handle()->size().height());
            if (!windowRegion.isEmpty()) {
                QRegion bufferDamage;
                if (!target->bufferDamage.isEmpty()) {
                    typedef Output::Transform Tr;
                    const Tr tr = target->transform;
                    const qint32 sc = target->scale;
                    if (tr == Tr::Rotated90 || tr == Tr::Rotated270 || tr == Tr::Flipped90
                        || tr == Tr::Flipped270) {
                        // calculate transformed + scaled buffer damage
                        for (const auto& rect : target->bufferDamage) {
                            const auto add = QRegion(rect.x() / sc,
                                                     rect.y() / sc,
                                                     rect.height() / sc,
                                                     rect.width() / sc);
                            bufferDamage = bufferDamage.united(add);
                        }
                    } else if (sc != 1) {
                        // calculate scaled buffer damage
                        for (const auto& rect : target->bufferDamage) {
                            const auto add = QRegion(rect.x() / sc,
                                                     rect.y() / sc,
                                                     rect.width() / sc,
                                                     rect.height() / sc);
                            bufferDamage = bufferDamage.united(add);
                        }
                    } else {
                        bufferDamage = target->bufferDamage;
                    }
                }
                target->damage = windowRegion.intersected(target->damage.united(bufferDamage));
                if (emitChanged) {
                    subsurfaceIsMapped = true;
                    trackedDamage = trackedDamage.united(target->damage);
                    Q_EMIT handle()->damaged(target->damage);
                    // workaround for https://bugreports.qt.io/browse/QTBUG-52092
                    // if the surface is a sub-surface, but the main surface is not yet mapped, fake
                    // frame rendered
                    if (subsurface) {
                        const auto mainSurface = subsurface->mainSurface();
                        if (!mainSurface || !mainSurface->buffer()) {
                            handle()->frameRendered(0);
                        }
                    }
                }
            }
        } else if (!target->buffer && emitChanged) {
            subsurfaceIsMapped = false;
            Q_EMIT handle()->unmapped();
        }
    }
    if (!emitChanged) {
        return;
    }
    if (sizeChanged) {
        Q_EMIT handle()->sizeChanged();
    }
    if (shadowChanged) {
        Q_EMIT handle()->shadowChanged();
    }
    if (blurChanged) {
        Q_EMIT handle()->blurChanged();
    }
    if (contrastChanged) {
        Q_EMIT handle()->contrastChanged();
    }
    if (slideChanged) {
        Q_EMIT handle()->slideOnShowHideChanged();
    }
    if (sourceRectangleChanged) {
        Q_EMIT handle()->sourceRectangleChanged();
    }
    if (childrenChanged) {
        Q_EMIT handle()->subsurfaceTreeChanged();
    }
}

void Surface::Private::commit()
{
    if (subsurface && subsurface->isSynchronized()) {
        swapStates(&pending, &subsurfacePending, false);
    } else {
        swapStates(&pending, &current, true);
        if (subsurface) {
            subsurface->d_ptr->commit();
        }

        // Commit all subsurfaces to apply position changes.
        // "The cached state is applied to the sub-surface immediately after the parent surface's
        // state is applied"

        for (auto it = current.children.cbegin(); it != current.children.cend(); ++it) {
            const auto& subsurface = *it;
            if (!subsurface) {
                continue;
            }
            subsurface->d_ptr->commit();
        }
    }

    if (shellSurface) {
        shellSurface->commit();
    }

    Q_EMIT handle()->committed();
}

void Surface::Private::commitSubsurface()
{
    if (!subsurface || !subsurface->isSynchronized()) {
        return;
    }
    swapStates(&subsurfacePending, &current, true);

    // "The cached state is applied to the sub-surface immediately after the parent surface's state
    // is applied"
    for (auto it = current.children.cbegin(); it != current.children.cend(); ++it) {
        const auto& subsurface = *it;
        if (!subsurface || !subsurface->isSynchronized()) {
            continue;
        }
        subsurface->d_ptr->commit();
    }
}

void Surface::Private::damage(const QRect& rect)
{
    pending.damage = pending.damage.united(rect);
}

void Surface::Private::damageBuffer(const QRect& rect)
{
    if (!pending.bufferIsSet || (pending.bufferIsSet && !pending.buffer)) {
        // TODO: should we send an error?
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
    // TODO: put the frame callback in a separate class inheriting Resource.
    wl_resource* frameCallback = client()->createResource(&wl_callback_interface, 1, callback);
    if (!frameCallback) {
        wl_resource_post_no_memory(resource());
        return;
    }
    wl_resource_set_implementation(frameCallback, nullptr, this, destroyFrameCallback);
    pending.callbacks << frameCallback;
}

void Surface::Private::attachBuffer(wl_resource* buffer, const QPoint& offset)
{
    pending.bufferIsSet = true;
    pending.offset = offset;

    if (pending.buffer) {
        delete pending.buffer;
    }

    if (!buffer) {
        // Got a null buffer, deletes content in next frame.
        pending.buffer = nullptr;
        pending.damage = QRegion();
        pending.bufferDamage = QRegion();
        return;
    }

    pending.buffer = new Buffer(buffer, q_ptr);

    QObject::connect(
        pending.buffer, &Buffer::resourceDestroyed, handle(), [this, buffer = pending.buffer]() {
            if (pending.buffer == buffer) {
                pending.buffer = nullptr;
            }
            if (subsurfacePending.buffer == buffer) {
                subsurfacePending.buffer = nullptr;
            }
            if (current.buffer == buffer) {
                current.buffer->unref();
                current.buffer = nullptr;
            }
        });
}

void Surface::Private::destroyFrameCallback(wl_resource* wlResource)
{
    auto priv = static_cast<Private*>(wl_resource_get_user_data(wlResource));

    priv->current.callbacks.removeAll(wlResource);
    priv->pending.callbacks.removeAll(wlResource);
    priv->subsurfacePending.callbacks.removeAll(wlResource);
}

void Surface::Private::attachCallback([[maybe_unused]] wl_client* wlClient,
                                      wl_resource* wlResource,
                                      wl_resource* buffer,
                                      int32_t sx,
                                      int32_t sy)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->attachBuffer(buffer, QPoint(sx, sy));
}

void Surface::Private::damageCallback([[maybe_unused]] wl_client* wlClient,
                                      wl_resource* wlResource,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->damage(QRect(x, y, width, height));
}

void Surface::Private::damageBufferCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->damageBuffer(QRect(x, y, width, height));
}

void Surface::Private::frameCallback([[maybe_unused]] wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t callback)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->addFrameCallback(callback);
}

void Surface::Private::opaqueRegionCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            wl_resource* wlRegion)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    auto region = wlRegion ? Wayland::Resource<Region>::fromResource(wlRegion)->handle() : nullptr;
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
    auto priv = static_cast<Private*>(fromResource(wlResource));
    auto region = wlRegion ? Wayland::Resource<Region>::fromResource(wlRegion)->handle() : nullptr;
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
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->commit();
}

void Surface::Private::bufferTransformCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               int32_t transform)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    priv->setTransform(Output::Transform(transform));
}

void Surface::Private::bufferScaleCallback([[maybe_unused]] wl_client* wlClient,
                                           wl_resource* wlResource,
                                           int32_t scale)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
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

Buffer* Surface::buffer()
{

    return d_ptr->current.buffer;
}

Buffer* Surface::buffer() const
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

// Surface *Surface::get(wl_resource *native)
//{
//    return Private::get<Surface>(native);
//}

// Surface *Surface::get(quint32 id, const Client* client)
//{
//    return Private::get<Surface>(id, client);
//}

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
    // TODO: Apply transform to the buffer size.
    return d_ptr->current.buffer->size() / scale();
}

QPointer<Shadow> Surface::shadow() const
{

    return d_ptr->current.shadow;
}

QPointer<BlurInterface> Surface::blur() const
{

    return d_ptr->current.blur;
}

QPointer<Contrast> Surface::contrast() const
{

    return d_ptr->current.contrast;
}

QPointer<SlideInterface> Surface::slideOnShowHide() const
{

    return d_ptr->current.slide;
}

bool Surface::isMapped() const
{

    if (d_ptr->subsurface) {
        // From the spec: "A sub-surface becomes mapped, when a non-NULL wl_buffer is applied and
        // the parent surface is mapped."
        return d_ptr->subsurfaceIsMapped && d_ptr->subsurface->parentSurface()
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

std::vector<Output*> Surface::outputs() const
{

    return d_ptr->outputs;
}

void Surface::setOutputs(std::vector<Output*> outputs)
{
    std::vector<Output*> removedOutputs = d_ptr->outputs;

    for (auto it = outputs.cbegin(), end = outputs.cend(); it != end; ++it) {
        auto stays = *it;
        removedOutputs.erase(std::remove(removedOutputs.begin(), removedOutputs.end(), stays),
                             removedOutputs.end());
    }

    for (auto it = removedOutputs.cbegin(), end = removedOutputs.cend(); it != end; ++it) {
        auto const binds = (*it)->d_ptr->getBinds(d_ptr->client()->handle());
        for (auto bind : binds) {
            d_ptr->send<wl_surface_send_leave>(bind->resource());
        }
        disconnect(d_ptr->outputDestroyedConnections.take(*it));
    }

    std::vector<Output*> addedOutputs = outputs;
    for (auto it = d_ptr->outputs.cbegin(), end = d_ptr->outputs.cend(); it != end; ++it) {
        auto const keeping = *it;
        addedOutputs.erase(std::remove(addedOutputs.begin(), addedOutputs.end(), keeping),
                           addedOutputs.end());
    }

    for (auto it = addedOutputs.cbegin(), end = addedOutputs.cend(); it != end; ++it) {
        auto const add = *it;

        auto const binds = add->d_ptr->getBinds(d_ptr->client()->handle());
        for (auto bind : binds) {
            d_ptr->send<wl_surface_send_enter>(bind->resource());
        }

        d_ptr->outputDestroyedConnections[add] = connect(add, &Output::removed, this, [this, add] {
            auto outputs = d_ptr->outputs;
            bool removed = false;
            outputs.erase(std::remove_if(outputs.begin(),
                                         outputs.end(),
                                         [&removed, add](Output* out) {
                                             if (add == out) {
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
    // TODO: send enter when the client binds the Output another time

    d_ptr->outputs = outputs;
}

Surface* Surface::surfaceAt(const QPointF& position)
{
    if (!isMapped()) {
        return nullptr;
    }

    // Go from top to bottom. Top most child is last in the vector.
    auto it = d_ptr->current.children.end();
    while (it != d_ptr->current.children.begin()) {
        auto const& current = *(--it);
        auto surface = current->surface();
        if (!surface) {
            continue;
        }
        if (auto s = surface->surfaceAt(position - current->position())) {
            return s;
        }
    }
    // check whether the geometry contains the pos
    if (!size().isEmpty() && QRectF(QPoint(0, 0), size()).contains(position)) {
        return this;
    }
    return nullptr;
}

Surface* Surface::inputSurfaceAt(const QPointF& position)
{
    // TODO: Most of this is very similar to Surface::surfaceAt
    //       Is there a way to reduce the code duplication?
    if (!isMapped()) {
        return nullptr;
    }

    // Go from top to bottom. Top most child is last in list.
    auto it = d_ptr->current.children.end();
    while (it != d_ptr->current.children.begin()) {
        auto const& current = *(--it);
        auto surface = current->surface();
        if (!surface) {
            continue;
        }
        if (auto s = surface->inputSurfaceAt(position - current->position())) {
            return s;
        }
    }
    // Check whether the geometry and input region contain the pos.
    if (!size().isEmpty() && QRectF(QPoint(0, 0), size()).contains(position)
        && (inputIsInfinite() || input().contains(position.toPoint()))) {
        return this;
    }
    return nullptr;
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

}
}
