/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "touch_pool.h"
#include "data_device.h"
#include "display.h"
#include "pointer.h"
#include "pointer_p.h"
#include "seat.h"
#include "seat_p.h"
#include "touch.h"
#include "utils.h"

#include <config-wrapland.h>

#if HAVE_LINUX_INPUT_H
#include <linux/input-event-codes.h>
#endif

namespace Wrapland::Server
{

touch_pool::touch_pool(Seat* seat)
    : seat{seat}
{
}

touch_pool::~touch_pool()
{
    QObject::disconnect(focus.surface_lost_notifier);
    for (auto dev : devices) {
        QObject::disconnect(dev, nullptr, seat, nullptr);
    }
}

touch_focus const& touch_pool::get_focus() const
{
    return focus;
}

std::vector<Touch*> const& touch_pool::get_devices() const
{
    return devices;
}

void touch_pool::create_device(Client* client, uint32_t version, uint32_t id)
{
    auto touch = new Touch(client, version, id, seat);
    devices.push_back(touch);

    if (focus.surface && focus.surface->client() == client) {
        // this is a touch for the currently focused touch surface
        focus.devices.push_back(touch);
        if (!ids.empty()) {
            // TODO(unknown author): send out all the points
        }
    }
    QObject::connect(touch, &Touch::resourceDestroyed, seat, [touch, this] {
        remove_one(devices, touch);
        remove_one(focus.devices, touch);

        assert(!contains(devices, touch));
        assert(!contains(focus.devices, touch));
    });
    Q_EMIT seat->touchCreated(touch);
}

void touch_pool::set_focused_surface(Surface* surface, QPointF const& surfacePosition)
{
    if (is_in_progress()) {
        // changing surface not allowed during a touch sequence
        return;
    }
    Q_ASSERT(!seat->drags().is_touch_drag());

    if (focus.surface) {
        QObject::disconnect(focus.surface_lost_notifier);
    }
    focus = touch_focus();
    focus.surface = surface;
    focus.offset = surfacePosition;
    focus.devices = interfacesForSurface(surface, devices);
    if (focus.surface) {
        focus.surface_lost_notifier
            = QObject::connect(surface, &Surface::resourceDestroyed, seat, [this] {
                  if (is_in_progress()) {
                      // Surface destroyed during touch sequence - send a cancel
                      for (auto touch : focus.devices) {
                          touch->cancel();
                      }
                  }
                  focus = touch_focus();
              });
    }
}

void touch_pool::set_focused_surface_position(QPointF const& surfacePosition)
{
    focus.offset = surfacePosition;
}

int32_t touch_pool::touch_down(QPointF const& globalPosition)
{
    const int32_t id = ids.empty() ? 0 : ids.crbegin()->first + 1;
    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    auto const pos = globalPosition - focus.offset;
    for (auto touch : focus.devices) {
        touch->down(id, serial, pos);
    }

    if (id == 0) {
        focus.first_touch_position = globalPosition;
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && focus.devices.empty() && seat->hasPointer()) {
        // If the client did not bind the touch interface fall back
        // to at least emulating touch through pointer events.
        forEachInterface(
            focus.surface, seat->pointers().get_devices(), [this, pos, serial](auto pointer) {
                pointer->d_ptr->sendEnter(serial, focus.surface, pos);
                pointer->d_ptr->sendMotion(pos);
                pointer->buttonPressed(serial, BTN_LEFT);
                pointer->d_ptr->sendFrame();
            });
    }
#endif

    ids[id] = serial;
    return id;
}

void touch_pool::touch_up(int32_t id)
{
    Q_ASSERT(ids.count(id));
    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    if (seat->drags().is_touch_drag() && seat->drags().get_source().serial == ids[id]) {
        // the implicitly grabbing touch point has been upped
        seat->drags().drop();
    }
    for (auto touch : focus.devices) {
        touch->up(id, serial);
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && focus.devices.empty() && seat->hasPointer()) {
        // Client did not bind touch, fall back to emulating with pointer events.
        const uint32_t serial = seat->d_ptr->display()->handle->nextSerial();
        forEachInterface(focus.surface, seat->pointers().get_devices(), [serial](auto pointer) {
            pointer->buttonReleased(serial, BTN_LEFT);
        });
    }
#endif

    ids.erase(id);
}

void touch_pool::touch_move(int32_t id, QPointF const& globalPosition)
{
    Q_ASSERT(ids.count(id));
    auto const pos = globalPosition - focus.offset;
    for (auto touch : focus.devices) {
        touch->move(id, pos);
    }

    if (id == 0) {
        focus.first_touch_position = globalPosition;
    }

    if (id == 0 && focus.devices.empty() && seat->hasPointer()) {
        // Client did not bind touch, fall back to emulating with pointer events.
        forEachInterface(focus.surface, seat->pointers().get_devices(), [pos](auto pointer) {
            pointer->d_ptr->sendMotion(pos);
        });
    }
    Q_EMIT seat->touchMoved(id, ids[id], globalPosition);
}

void touch_pool::touch_move_any(QPointF const& pos)
{
    assert(!ids.empty());
    touch_move(ids.cbegin()->first, pos);
}

void touch_pool::touch_frame() const
{
    for (auto touch : focus.devices) {
        touch->frame();
    }
}

void touch_pool::cancel_sequence()
{
    for (auto touch : focus.devices) {
        touch->cancel();
    }
    if (seat->drags().is_touch_drag()) {
        // cancel the drag, don't drop.
        seat->drags().cancel();
    }
    ids.clear();
}

bool touch_pool::is_in_progress() const
{
    return !ids.empty();
}

bool touch_pool::has_implicit_grab(uint32_t serial) const
{
    if (!focus.surface) {
        // origin surface has been destroyed
        return false;
    }
    auto check = [serial](auto const& pair) { return pair.second == serial; };
    return std::find_if(ids.begin(), ids.end(), check) != ids.end();
}

}
