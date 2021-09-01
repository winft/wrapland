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

drag_source const& drag_pool::get_source() const
{
    return source;
}

void drag_pool::end(uint32_t serial)
{
    auto trgt = target;
    QObject::disconnect(source.device_destroy_notifier);
    QObject::disconnect(source.destroy_notifier);
    if (source.dev && source.dev->dragSource()) {
        source.dev->dragSource()->dropPerformed();
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
    if (source.mode == drag_mode::pointer) {
        set_target(new_surface, seat->pointers().get_position(), inputTransformation);
    } else {
        assert(source.mode == drag_mode::touch);
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

    if (source.mode == drag_mode::pointer) {
        seat->pointers().set_position(globalPosition);
    } else if (source.mode == drag_mode::touch
               && seat->d_ptr->touches.value().get_focus().firstTouchPos != globalPosition) {
        // TODO(romangg): instead of moving any touch point could we move with id 0? Probably yes
        //                if we always end a drag once the id 0 touch point has been lifted.
        seat->touches().touch_move_any(globalPosition);
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
    return source.mode != drag_mode::none;
}

bool drag_pool::is_pointer_drag() const
{
    return source.mode == drag_mode::pointer;
}

bool drag_pool::is_touch_drag() const
{
    return source.mode == drag_mode::touch;
}

void drag_pool::perform_drag(DataDevice* dataDevice)
{
    const auto dragSerial = dataDevice->dragImplicitGrabSerial();
    auto* dragSurface = dataDevice->origin();
    auto& pointers = seat->pointers();

    if (pointers.has_implicit_grab(dragSerial)) {
        source.mode = drag_mode::pointer;
        source.pointer
            = interfaceForSurface(dragSurface, seat->d_ptr->pointers.value().get_devices());
        transformation = pointers.get_focus().transformation;
    } else if (seat->touches().has_implicit_grab(dragSerial)) {
        source.mode = drag_mode::touch;
        source.touch = interfaceForSurface(dragSurface, seat->d_ptr->touches.value().get_devices());
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

    source.dev = dataDevice;
    source.device_destroy_notifier
        = QObject::connect(dataDevice, &DataDevice::resourceDestroyed, seat, [this] {
              end(seat->d_ptr->display()->handle()->nextSerial());
          });

    if (dataDevice->dragSource()) {
        source.destroy_notifier = QObject::connect(
            dataDevice->dragSource(), &DataSource::resourceDestroyed, seat, [this] {
                const auto serial = seat->d_ptr->display()->handle()->nextSerial();
                if (target) {
                    target->updateDragTarget(nullptr, serial);
                    target = nullptr;
                }
                end(serial);
            });
    } else {
        source.destroy_notifier = QMetaObject::Connection();
    }
    dataDevice->updateDragTarget(proxied ? nullptr : originSurface,
                                 dataDevice->dragImplicitGrabSerial());
    Q_EMIT seat->dragStarted();
    Q_EMIT seat->dragSurfaceChanged();
}

}
