/*
    SPDX-FileCopyrightText: 2020, 2021 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "drag_pool.h"

#include "data_device.h"
#include "data_offer.h"
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
    cancel_target();
    end();
    Q_EMIT seat->dragEnded(false);
}

void drag_pool::drop()
{
    for_each_target_device([](auto dev) { dev->drop(); });

    for (auto& device : target.devices) {
        QObject::disconnect(device.destroy_notifier);
    }
    target.devices.clear();

    cancel_target();
    end();
    Q_EMIT seat->dragEnded(true);
}

void drag_pool::end()
{
    QObject::disconnect(source.destroy_notifier);
    QObject::disconnect(source.action_notifier);

    if (source.src) {
        source.src->send_dnd_drop_performed();
    }

    source = {};
    target = {};
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

    cancel_target();

    auto const serial = seat->d_ptr->display()->handle->nextSerial();

    if (source.mode == drag_mode::pointer) {
        seat->pointers().set_position(globalPosition);
    } else if (source.mode == drag_mode::touch
               && seat->touches().get_focus().first_touch_position != globalPosition) {
        // TODO(romangg): instead of moving any touch point could we move with id 0? Probably yes
        //                if we always end a drag once the id 0 touch point has been lifted.
        seat->touches().touch_move_any(globalPosition);
    }

    update_target(new_surface, serial, inputTransformation);
}

void drag_pool::set_source_client_movement_blocked(bool block)
{
    source.movement_blocked = block;
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

void drag_pool::start(data_source* src, Surface* origin, Surface* icon, uint32_t serial)
{
    auto& pointers = seat->pointers();

    if (pointers.has_implicit_grab(serial)) {
        source.mode = drag_mode::pointer;
        source.pointer = interfaceForSurface(origin, seat->pointers().get_devices());
        target.transformation = pointers.get_focus().transformation;
    } else if (seat->touches().has_implicit_grab(serial)) {
        source.mode = drag_mode::touch;
        source.touch = interfaceForSurface(origin, seat->touches().get_devices());
        // TODO(unknown author): touch transformation
    } else {
        // We fallback to a pointer drag.
        source.mode = drag_mode::pointer;
        // TODO(romangg): Should we set other properties here too?
    }

    // Source is allowed to be null, handled by client internally.
    source.src = src;
    source.surfaces.origin = origin;
    source.surfaces.icon = icon;
    source.serial = serial;

    if (src) {
        QObject::connect(src, &data_source::resourceDestroyed, seat, [this] { source = {}; });

        source.action_notifier
            = QObject::connect(src, &data_source::supported_dnd_actions_changed, seat, [this] {
                  for (auto& device : target.devices) {
                      if (device.offer) {
                          match_actions(device.offer);
                      }
                  }
              });

        source.destroy_notifier
            = QObject::connect(src, &data_source::resourceDestroyed, seat, [this] { cancel(); });
    }

    target.surface = source.surfaces.origin;

    // TODO(unknown author): transformation needs to be either pointer or touch
    target.transformation = pointers.get_focus().transformation;

    update_target(source.surfaces.origin, serial, target.transformation);

    Q_EMIT seat->dragStarted();
}

void drag_pool::update_target(Surface* surface,
                              uint32_t serial,
                              QMatrix4x4 const& inputTransformation)
{
    cancel_target();

    auto devices = interfacesForSurface(surface, seat->d_ptr->data_devices.devices);

    if (!surface || devices.empty()) {
        if (source.src) {
            source.src->send_action(dnd_action::none);
        }
        if (!surface) {
            return;
        }
    }

    for (auto&& dev : devices) {
        auto destroy_notifier
            = QObject::connect(dev, &data_device::resourceDestroyed, seat, [this, dev] {
                  auto it = std::find_if(target.devices.begin(),
                                         target.devices.end(),
                                         [&dev](auto& target) { return target.dev == dev; });
                  if (it != target.devices.end()) {
                      QObject::disconnect(it->destroy_notifier);
                      target.devices.erase(it);
                  }
                  if (target.devices.empty()) {
                      // TODO(romangg): Inform source?
                  }
              });
        target.devices.push_back(
            {dev, nullptr, QMetaObject::Connection(), std::move(destroy_notifier)});
    }

    assert(surface);
    target.surface = surface;
    target.transformation = inputTransformation;

    target.surface_destroy_notifier
        = QObject::connect(surface, &Surface::resourceDestroyed, seat, [this] { cancel_target(); });

    setup_motion();
    update_offer(serial);
}

void drag_pool::update_offer(uint32_t serial)
{
    // TODO(unknown author): handle touch position
    auto const pos = target.transformation.map(seat->pointers().get_position());

    for (auto& device : target.devices) {
        device.offer = device.dev->create_offer(source.src);
        device.dev->enter(serial, target.surface, pos, device.offer);

        if (device.offer) {
            device.offer->send_source_actions();

            device.offer_action_notifier = QObject::connect(
                device.offer, &data_offer::dnd_actions_changed, seat, [this, offer = device.offer] {
                    match_actions(offer);
                });
        }
    }
}

dnd_action drag_pool::target_actions_update(dnd_actions receiver_actions,
                                            dnd_action preffered_action)
{
    auto select = [this](auto action) {
        source.src->send_action(action);
        return action;
    };

    if (source.src->supported_dnd_actions().testFlag(preffered_action)) {
        return select(preffered_action);
    }

    auto const src_actions = source.src->supported_dnd_actions();

    if (src_actions.testFlag(dnd_action::copy) && receiver_actions.testFlag(dnd_action::copy)) {
        return select(dnd_action::copy);
    }
    if (src_actions.testFlag(dnd_action::move) && receiver_actions.testFlag(dnd_action::move)) {
        return select(dnd_action::move);
    }
    if (src_actions.testFlag(dnd_action::ask) && receiver_actions.testFlag(dnd_action::ask)) {
        return select(dnd_action::ask);
    }

    return select(dnd_action::none);
}

void drag_pool::match_actions(data_offer* offer)
{
    assert(offer);

    auto action
        = target_actions_update(offer->supported_dnd_actions(), offer->preferred_dnd_action());
    offer->send_action(action);
}

void drag_pool::for_each_target_device(std::function<void(data_device*)> apply) const
{
    std::for_each(
        target.devices.cbegin(), target.devices.cend(), [&](auto& dev) { apply(dev.dev); });
}

void drag_pool::cancel_target()
{
    if (!target.surface) {
        return;
    }

    for (auto& device : target.devices) {
        device.dev->leave();
        QObject::disconnect(device.destroy_notifier);
        QObject::disconnect(device.offer_action_notifier);
    }
    target.devices.clear();

    QObject::disconnect(target.motion_notifier);
    target.motion_notifier = QMetaObject::Connection();

    QObject::disconnect(target.surface_destroy_notifier);
    target.surface_destroy_notifier = QMetaObject::Connection();
    target.surface = nullptr;
}

void drag_pool::setup_motion()
{
    if (is_pointer_drag()) {
        setup_pointer_motion();
    } else if (is_touch_drag()) {
        setup_touch_motion();
    }
}

void drag_pool::setup_pointer_motion()
{
    assert(is_pointer_drag());

    target.motion_notifier = QObject::connect(seat, &Seat::pointerPosChanged, seat, [this] {
        auto pos = target.transformation.map(seat->pointers().get_position());
        auto ts = seat->timestamp();
        for_each_target_device([&](auto dev) { dev->motion(ts, pos); });
    });
}

void drag_pool::setup_touch_motion()
{
    assert(is_touch_drag());

    target.motion_notifier = QObject::connect(
        seat, &Seat::touchMoved, seat, [this](auto /*id*/, auto serial, auto global_pos) {
            if (serial != source.serial) {
                // different touch down has been moved
                return;
            }
            auto pos = seat->drags().get_target().transformation.map(global_pos);
            auto ts = seat->timestamp();
            for_each_target_device([&](auto dev) { dev->motion(ts, pos); });
        });
}

}
