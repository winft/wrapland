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

drag_target const& drag_pool::get_target() const
{
    return target;
}

void drag_pool::cancel()
{
    if (target.dev) {
        target.dev->update_drag_target(nullptr, 0);
        target.dev = nullptr;
    }
    end(0);
}

void drag_pool::end(uint32_t serial)
{
    auto trgt = target.dev;

    QObject::disconnect(source.device_destroy_notifier);
    QObject::disconnect(source.destroy_notifier);

    if (source.dev && source.dev->drag_source()) {
        source.dev->drag_source()->send_dnd_drop_performed();
    }

    if (trgt) {
        trgt->drop();
        trgt->update_drag_target(nullptr, serial);
    }

    source = {};
    target = {};

    Q_EMIT seat->dragSurfaceChanged();
    Q_EMIT seat->dragEnded();
}

void drag_pool::set_target(Surface* new_surface, const QMatrix4x4& inputTransformation)
{
    if (source.mode == drag_mode::pointer) {
        set_target(new_surface, seat->pointers().get_position(), inputTransformation);
    } else {
        assert(source.mode == drag_mode::touch);
        set_target(
            new_surface, seat->touches().get_focus().first_touch_position, inputTransformation);
    }
}

void drag_pool::set_target(Surface* new_surface,
                           const QPointF& globalPosition,
                           const QMatrix4x4& inputTransformation)
{
    if (new_surface == target.surface) {
        // no change
        return;
    }
    auto const serial = seat->d_ptr->display()->handle()->nextSerial();
    if (target.dev) {
        target.dev->update_drag_target(nullptr, serial);
        QObject::disconnect(target.destroy_notifier);
        target.destroy_notifier = QMetaObject::Connection();
    }

    // In theory we can have multiple data devices and we should send the drag to all of them, but
    // that seems overly complicated. In practice so far the only case for multiple data devices is
    // for clipboard overriding.
    target.dev = interfaceForSurface(new_surface, seat->d_ptr->data_devices.devices);

    if (source.mode == drag_mode::pointer) {
        seat->pointers().set_position(globalPosition);
    } else if (source.mode == drag_mode::touch
               && seat->touches().get_focus().first_touch_position != globalPosition) {
        // TODO(romangg): instead of moving any touch point could we move with id 0? Probably yes
        //                if we always end a drag once the id 0 touch point has been lifted.
        seat->touches().touch_move_any(globalPosition);
    }
    if (target.dev) {
        target.surface = new_surface;
        target.transformation = inputTransformation;
        target.dev->update_drag_target(target.surface, serial);
        target.destroy_notifier
            = QObject::connect(target.dev, &DataDevice::resourceDestroyed, seat, [this] {
                  QObject::disconnect(target.destroy_notifier);
                  target.destroy_notifier = QMetaObject::Connection();
                  target.dev = nullptr;
              });

    } else {
        target.surface = nullptr;
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
    const auto dragSerial = dataDevice->drag_implicit_grab_serial();
    auto* dragSurface = dataDevice->origin();
    auto& pointers = seat->pointers();

    if (pointers.has_implicit_grab(dragSerial)) {
        source.mode = drag_mode::pointer;
        source.pointer = interfaceForSurface(dragSurface, seat->pointers().get_devices());
        target.transformation = pointers.get_focus().transformation;
    } else if (seat->touches().has_implicit_grab(dragSerial)) {
        source.mode = drag_mode::touch;
        source.touch = interfaceForSurface(dragSurface, seat->touches().get_devices());
        // TODO(unknown author): touch transformation
    } else {
        // no implicit grab, abort drag
        return;
    }
    auto* originSurface = dataDevice->origin();
    const bool proxied = originSurface->dataProxy();
    if (!proxied) {
        // origin surface
        target.dev = dataDevice;
        target.surface = originSurface;
        // TODO(unknown author): transformation needs to be either pointer or touch
        target.transformation = pointers.get_focus().transformation;
    }

    source.dev = dataDevice;
    source.device_destroy_notifier
        = QObject::connect(dataDevice, &DataDevice::resourceDestroyed, seat, [this] {
              end(seat->d_ptr->display()->handle()->nextSerial());
          });

    if (dataDevice->drag_source()) {
        source.destroy_notifier = QObject::connect(
            dataDevice->drag_source(), &DataSource::resourceDestroyed, seat, [this] {
                const auto serial = seat->d_ptr->display()->handle()->nextSerial();
                if (target.dev) {
                    target.dev->update_drag_target(nullptr, serial);
                    target.dev = nullptr;
                }
                end(serial);
            });
    } else {
        source.destroy_notifier = QMetaObject::Connection();
    }
    dataDevice->update_drag_target(proxied ? nullptr : originSurface,
                                   dataDevice->drag_implicit_grab_serial());
    Q_EMIT seat->dragStarted();
    Q_EMIT seat->dragSurfaceChanged();
}

}
