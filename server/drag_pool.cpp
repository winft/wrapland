/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "drag_pool.h"
#include "data_device.h"
#include "data_source.h"
#include "display.h"
#include "seat.h"
#include "seat_p.h"
#include "touch.h"
#include "utils.h"

namespace Wrapland::Server
{

drag_pool::drag_pool(Seat* seat)
    : seat{seat}
{
}

void drag_pool::end(uint32_t serial)
{
    auto trgt = target;
    QObject::disconnect(destroyConnection);
    QObject::disconnect(dragSourceDestroyConnection);
    if (source && source->dragSource()) {
        source->dragSource()->dropPerformed();
    }
    if (trgt) {
        trgt->drop();
        trgt->updateDragTarget(nullptr, serial);
    }
    *this = drag_pool(seat);
    Q_EMIT seat->dragSurfaceChanged();
    Q_EMIT seat->dragEnded();
}

void drag_pool::set_target(Surface* new_surface, const QMatrix4x4& inputTransformation)
{
    if (mode == Mode::Pointer) {
        set_target(new_surface, seat->pointers().get_position(), inputTransformation);
    } else {
        Q_ASSERT(mode == Mode::Touch);
        set_target(new_surface,
                   seat->d_ptr->touches.value().get_focus().firstTouchPos,
                   inputTransformation);
    }
}

void drag_pool::set_target(Surface* new_surface,
                           const QPointF& globalPosition,
                           const QMatrix4x4& inputTransformation)
{
    if (new_surface == surface) {
        // no change
        return;
    }
    auto const serial = seat->d_ptr->display()->handle()->nextSerial();
    if (target) {
        target->updateDragTarget(nullptr, serial);
        QObject::disconnect(target_destroy_connection);
        target_destroy_connection = QMetaObject::Connection();
    }

    // In theory we can have multiple data devices and we should send the drag to all of them, but
    // that seems overly complicated. In practice so far the only case for multiple data devices is
    // for clipboard overriding.
    target = interfaceForSurface(new_surface, seat->d_ptr->data_devices.devices);

    if (mode == Mode::Pointer) {
        seat->pointers().set_position(globalPosition);
    } else if (mode == Mode::Touch
               && seat->d_ptr->touches.value().get_focus().firstTouchPos != globalPosition) {
        seat->touches().touch_move(seat->d_ptr->touches.value().ids.cbegin()->first,
                                   globalPosition);
    }
    if (target) {
        surface = new_surface;
        transformation = inputTransformation;
        target->updateDragTarget(surface, serial);
        target_destroy_connection
            = QObject::connect(target, &DataDevice::resourceDestroyed, seat, [this] {
                  QObject::disconnect(target_destroy_connection);
                  target_destroy_connection = QMetaObject::Connection();
                  target = nullptr;
              });

    } else {
        surface = nullptr;
    }
    Q_EMIT seat->dragSurfaceChanged();
}

bool drag_pool::is_in_progress() const
{
    return mode != Mode::None;
}

bool drag_pool::is_pointer_drag() const
{
    return mode == Mode::Pointer;
}

bool drag_pool::is_touch_drag() const
{
    return mode == Mode::Touch;
}

void drag_pool::perform_drag(DataDevice* dataDevice)
{
    const auto dragSerial = dataDevice->dragImplicitGrabSerial();
    auto* dragSurface = dataDevice->origin();
    auto& pointers = seat->pointers();

    if (pointers.has_implicit_grab(dragSerial)) {
        mode = Mode::Pointer;
        sourcePointer
            = interfaceForSurface(dragSurface, seat->d_ptr->pointers.value().get_devices());
        transformation = pointers.get_focus().transformation;
    } else if (seat->touches().has_implicit_grab(dragSerial)) {
        mode = Mode::Touch;
        sourceTouch = interfaceForSurface(dragSurface, seat->d_ptr->touches.value().devices);
        // TODO(unknown author): touch transformation
    } else {
        // no implicit grab, abort drag
        return;
    }
    auto* originSurface = dataDevice->origin();
    const bool proxied = originSurface->dataProxy();
    if (!proxied) {
        // origin surface
        target = dataDevice;
        surface = originSurface;
        // TODO(unknown author): transformation needs to be either pointer or touch
        transformation = pointers.get_focus().transformation;
    }

    source = dataDevice;
    destroyConnection = QObject::connect(dataDevice, &DataDevice::resourceDestroyed, seat, [this] {
        end(seat->d_ptr->display()->handle()->nextSerial());
    });

    if (dataDevice->dragSource()) {
        dragSourceDestroyConnection = QObject::connect(
            dataDevice->dragSource(), &DataSource::resourceDestroyed, seat, [this] {
                const auto serial = seat->d_ptr->display()->handle()->nextSerial();
                if (target) {
                    target->updateDragTarget(nullptr, serial);
                    target = nullptr;
                }
                end(serial);
            });
    } else {
        dragSourceDestroyConnection = QMetaObject::Connection();
    }
    dataDevice->updateDragTarget(proxied ? nullptr : originSurface,
                                 dataDevice->dragImplicitGrabSerial());
    Q_EMIT seat->dragStarted();
    Q_EMIT seat->dragSurfaceChanged();
}

}
