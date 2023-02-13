/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "pointer_pool.h"
#include "data_device.h"
#include "display.h"
#include "pointer_p.h"
#include "seat.h"
#include "seat_p.h"
#include "utils.h"

#include <QHash>
#include <unordered_set>

#include <config-wrapland.h>
#include <linux/input-event-codes.h>

#include <cstdint>

namespace Wrapland::Server
{

enum class button_state {
    released,
    pressed,
};

uint32_t qtToWaylandButton(Qt::MouseButton button)
{
#if HAVE_LINUX_INPUT_H
    static QHash<Qt::MouseButton, uint32_t> const s_buttons({
        {Qt::LeftButton, BTN_LEFT},
        {Qt::RightButton, BTN_RIGHT},
        {Qt::MiddleButton, BTN_MIDDLE},
        {Qt::ExtraButton1, BTN_BACK},    // note: QtWayland maps BTN_SIDE
        {Qt::ExtraButton2, BTN_FORWARD}, // note: QtWayland maps BTN_EXTRA
        {Qt::ExtraButton3, BTN_TASK},    // note: QtWayland maps BTN_FORWARD
        {Qt::ExtraButton4, BTN_EXTRA},   // note: QtWayland maps BTN_BACK
        {Qt::ExtraButton5, BTN_SIDE},    // note: QtWayland maps BTN_TASK
        {Qt::ExtraButton6, BTN_TASK + 1},
        {Qt::ExtraButton7, BTN_TASK + 2},
        {Qt::ExtraButton8, BTN_TASK + 3},
        {Qt::ExtraButton9, BTN_TASK + 4},
        {Qt::ExtraButton10, BTN_TASK + 5},
        {Qt::ExtraButton11, BTN_TASK + 6},
        {Qt::ExtraButton12, BTN_TASK + 7},
        {Qt::ExtraButton13, BTN_TASK + 8}
        // further mapping not possible, 0x120 is BTN_JOYSTICK
    });
    return s_buttons.value(button, 0);
#else
    return 0;
#endif
}

pointer_pool::pointer_pool(Seat* seat)
    : seat{seat}
{
}

pointer_pool::~pointer_pool()
{
    QObject::disconnect(focus.surface_lost_notifier);
    for (auto dev : devices) {
        QObject::disconnect(dev, nullptr, seat, nullptr);
    }
}

pointer_focus const& pointer_pool::get_focus() const
{
    return focus;
}

std::vector<Pointer*> const& pointer_pool::get_devices() const
{
    return devices;
}

void pointer_pool::create_device(Client* client, uint32_t version, uint32_t id)
{
    auto pointer = new Pointer(client, version, id, seat);
    devices.push_back(pointer);

    if (focus.surface && focus.surface->client() == pointer->client()) {
        // this is a pointer for the currently focused pointer surface
        focus.devices.push_back(pointer);
        pointer->setFocusedSurface(focus.serial, focus.surface);
        pointer->frame();
        if (focus.devices.size() == 1) {
            // got a new pointer
            Q_EMIT seat->focusedPointerChanged(pointer);
        }
    }

    QObject::connect(pointer, &Pointer::resourceDestroyed, seat, [pointer, this] {
        remove_one(devices, pointer);
        if (remove_one(focus.devices, pointer)) {
            if (focus.devices.empty()) {
                Q_EMIT seat->focusedPointerChanged(nullptr);
            }
        }

        assert(!contains(devices, pointer));
        assert(!contains(focus.devices, pointer));
    });

    Q_EMIT seat->pointerCreated(pointer);
}

void pointer_pool::update_button_serial(uint32_t button, uint32_t serial)
{
    buttonSerials[button] = serial;
}

void pointer_pool::update_button_state(uint32_t button, button_state state)
{
    buttonStates[button] = state;
}

QPointF pointer_pool::get_position() const
{
    return pos;
}

void pointer_pool::set_position(QPointF const& position)
{
    if (pos != position) {
        pos = position;
        for (auto pointer : focus.devices) {
            pointer->motion(focus.transformation.map(position));
        }
        // TODO(romangg): should we provide the transformed position here?
        Q_EMIT seat->pointerPosChanged(position);
    }
}

void pointer_pool::set_focused_surface(Surface* surface, QPointF const& surfacePosition)
{
    QMatrix4x4 transformation;
    transformation.translate(-static_cast<float>(surfacePosition.x()),
                             -static_cast<float>(surfacePosition.y()));
    set_focused_surface(surface, transformation);

    if (focus.surface) {
        focus.offset = surfacePosition;
    }
}

void pointer_pool::set_focused_surface(Surface* surface, QMatrix4x4 const& transformation)
{
    if (seat->drags().is_pointer_drag()) {
        // ignore
        return;
    }

    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    std::unordered_set<Pointer*> framePointers;

    for (auto pointer : focus.devices) {
        pointer->setFocusedSurface(serial, nullptr);
        framePointers.insert(pointer);
    }

    if (focus.surface) {
        QObject::disconnect(focus.surface_lost_notifier);
    }

    focus = pointer_focus();
    focus.surface = surface;

    focus.devices.clear();

    if (surface) {
        focus.devices = interfacesForSurface(surface, devices);

        focus.surface_lost_notifier
            = QObject::connect(surface, &Surface::resourceDestroyed, seat, [this] {
                  focus = pointer_focus();
                  Q_EMIT seat->focusedPointerChanged(nullptr);
              });
        focus.offset = QPointF();
        focus.transformation = transformation;
        focus.serial = serial;
    }

    if (focus.devices.empty()) {
        Q_EMIT seat->focusedPointerChanged(nullptr);
        for (auto pointer : framePointers) {
            pointer->frame();
        }
        return;
    }

    // TODO(unknown author): signal with all pointers
    Q_EMIT seat->focusedPointerChanged(focus.devices.front());

    for (auto pointer : focus.devices) {
        pointer->setFocusedSurface(serial, surface);
        framePointers.insert(pointer);
    }

    for (auto pointer : framePointers) {
        pointer->frame();
    }
}

void pointer_pool::set_focused_surface_position(QPointF const& surfacePosition)
{
    if (focus.surface) {
        focus.offset = surfacePosition;
        focus.transformation = QMatrix4x4();
        focus.transformation.translate(-static_cast<float>(surfacePosition.x()),
                                       -static_cast<float>(surfacePosition.y()));
    }
}

void pointer_pool::set_focused_surface_transformation(QMatrix4x4 const& transformation)
{

    if (focus.surface) {
        focus.transformation = transformation;
    }
}

void pointer_pool::button_pressed(Qt::MouseButton button)
{
    const uint32_t nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    button_pressed(nativeButton);
}

void pointer_pool::button_pressed(uint32_t button)
{
    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    update_button_serial(button, serial);
    update_button_state(button, button_state::pressed);
    if (seat->drags().is_pointer_drag()) {
        // ignore
        return;
    }
    if (focus.surface) {
        for (auto pointer : focus.devices) {
            pointer->buttonPressed(serial, button);
        }
    }
}

void pointer_pool::button_released(Qt::MouseButton button)
{
    const uint32_t nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    button_released(nativeButton);
}

void pointer_pool::button_released(uint32_t button)
{
    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    const uint32_t currentButtonSerial = button_serial(button);
    update_button_serial(button, serial);
    update_button_state(button, button_state::released);
    if (seat->drags().is_pointer_drag()) {
        if (seat->drags().get_source().serial != currentButtonSerial) {
            // not our drag button - ignore
            return;
        }
        seat->drags().drop();
        return;
    }
    if (focus.surface) {
        for (auto pointer : focus.devices) {
            pointer->buttonReleased(serial, button);
        }
    }
}

bool pointer_pool::is_button_pressed(Qt::MouseButton button) const
{
    return is_button_pressed(qtToWaylandButton(button));
}

bool pointer_pool::is_button_pressed(uint32_t button) const
{
    auto it = buttonStates.find(button);
    if (it == buttonStates.end()) {
        return false;
    }
    return it->second == button_state::pressed;
}

uint32_t pointer_pool::button_serial(Qt::MouseButton button) const
{
    return button_serial(qtToWaylandButton(button));
}

uint32_t pointer_pool::button_serial(uint32_t button) const
{
    auto it = buttonSerials.find(button);
    if (it == buttonSerials.end()) {
        return 0;
    }
    return it->second;
}

void pointer_pool::send_axis(Qt::Orientation orientation,
                             qreal delta,
                             int32_t discreteDelta,
                             PointerAxisSource source) const
{
    if (seat->drags().is_pointer_drag()) {
        // ignore
        return;
    }
    if (focus.surface) {
        for (auto pointer : focus.devices) {
            pointer->axis(orientation, delta, discreteDelta, source);
        }
    }
}

void pointer_pool::send_axis(Qt::Orientation orientation, uint32_t delta) const
{
    if (seat->drags().is_pointer_drag()) {
        // ignore
        return;
    }
    if (focus.surface) {
        for (auto pointer : focus.devices) {
            pointer->axis(orientation, delta);
        }
    }
}

bool pointer_pool::has_implicit_grab(uint32_t serial) const
{
    for (auto it : buttonSerials) {
        if (it.second == serial) {
            return is_button_pressed(it.first);
        }
    }
    return false;
}

void pointer_pool::relative_motion(QSizeF const& delta,
                                   QSizeF const& deltaNonAccelerated,
                                   uint64_t microseconds) const
{
    if (focus.surface) {
        for (auto pointer : focus.devices) {
            pointer->relativeMotion(delta, deltaNonAccelerated, microseconds);
        }
    }
}

void pointer_pool::start_swipe_gesture(uint32_t fingerCount)
{
    if (gesture.surface) {
        return;
    }

    gesture.surface = focus.surface;
    if (!gesture.surface) {
        return;
    }

    gesture.surface_destroy_notifier = QObject::connect(
        gesture.surface, &Surface::resourceDestroyed, seat, [this] { cleanup_gesture(); });

    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    forEachInterface(gesture.surface, devices, [serial, fingerCount](auto pointer) {
        pointer->d_ptr->startSwipeGesture(serial, fingerCount);
    });
}

void pointer_pool::update_swipe_gesture(QSizeF const& delta) const
{
    if (!gesture.surface) {
        return;
    }

    forEachInterface(gesture.surface, devices, [delta](auto pointer) {
        pointer->d_ptr->updateSwipeGesture(delta);
    });
}

void pointer_pool::end_swipe_gesture()
{
    if (!gesture.surface) {
        return;
    }

    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    forEachInterface(gesture.surface, devices, [serial](auto pointer) {
        pointer->d_ptr->endSwipeGesture(serial);
    });

    cleanup_gesture();
}

void pointer_pool::cancel_swipe_gesture()
{
    if (!gesture.surface) {
        return;
    }

    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    forEachInterface(gesture.surface, devices, [serial](auto pointer) {
        pointer->d_ptr->cancelSwipeGesture(serial);
    });

    cleanup_gesture();
}

void pointer_pool::start_pinch_gesture(uint32_t fingerCount)
{
    if (gesture.surface) {
        return;
    }

    gesture.surface = focus.surface;
    if (!gesture.surface) {
        return;
    }

    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    forEachInterface(gesture.surface, devices, [serial, fingerCount](auto pointer) {
        pointer->d_ptr->startPinchGesture(serial, fingerCount);
    });
}

void pointer_pool::update_pinch_gesture(QSizeF const& delta, qreal scale, qreal rotation) const
{
    if (!gesture.surface) {
        return;
    }

    forEachInterface(gesture.surface, devices, [delta, scale, rotation](auto pointer) {
        pointer->d_ptr->updatePinchGesture(delta, scale, rotation);
    });
}

void pointer_pool::end_pinch_gesture()
{
    if (!gesture.surface) {
        return;
    }

    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    forEachInterface(gesture.surface, devices, [serial](auto pointer) {
        pointer->d_ptr->endPinchGesture(serial);
    });

    cleanup_gesture();
}

void pointer_pool::cancel_pinch_gesture()
{
    if (!gesture.surface) {
        return;
    }

    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    forEachInterface(gesture.surface, devices, [serial](auto pointer) {
        pointer->d_ptr->cancelPinchGesture(serial);
    });

    cleanup_gesture();
}

void pointer_pool::cleanup_gesture()
{
    QObject::disconnect(gesture.surface_destroy_notifier);
    gesture.surface = nullptr;
}

void pointer_pool::frame() const
{
    for (auto pointer : focus.devices) {
        pointer->frame();
    }
}

}
