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
#include "seat.h"
#include "seat_p.h"

#include "client.h"
#include "data_device.h"
#include "data_source.h"
#include "display.h"
#include "primary_selection.h"
#include "surface.h"

#include <config-wrapland.h>
#include <cstdint>

#ifndef WL_SEAT_NAME_SINCE_VERSION
#define WL_SEAT_NAME_SINCE_VERSION 2
#endif

namespace Wrapland::Server
{

Seat::Private::Private(Seat* q, Display* display)
    : SeatGlobal(q, display, &wl_seat_interface, &s_interface)
    , pointers(q)
    , keyboards(q)
    , touches(q)
    , drags(q)
    , data_devices(q)
    , primary_selection_devices(q)
    , text_inputs(q)
    , q_ptr(q)
{
}

const struct wl_seat_interface Seat::Private::s_interface = {
    cb<getPointerCallback>,
    cb<getKeyboardCallback>,
    cb<getTouchCallback>,
    resourceDestroyCallback,
};

Seat::Seat(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    connect(this, &Seat::nameChanged, this, [this] { d_ptr->sendName(); });

    auto sendCapabilities = [this] { d_ptr->sendCapabilities(); };
    connect(this, &Seat::hasPointerChanged, this, sendCapabilities);
    connect(this, &Seat::hasKeyboardChanged, this, sendCapabilities);
    connect(this, &Seat::hasTouchChanged, this, sendCapabilities);

    d_ptr->create();
}

Seat::~Seat() = default;

void Seat::Private::bindInit(SeatBind* bind)
{
    send<wl_seat_send_capabilities>(bind, getCapabilities());
    send<wl_seat_send_name, WL_SEAT_NAME_SINCE_VERSION>(bind, name.c_str());
}

void Seat::Private::sendName()
{
    send<wl_seat_send_name, WL_SEAT_NAME_SINCE_VERSION>(name.c_str());
}

uint32_t Seat::Private::getCapabilities() const
{
    uint32_t capabilities = 0;
    if (pointer) {
        capabilities |= WL_SEAT_CAPABILITY_POINTER;
    }
    if (keyboard) {
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (touch) {
        capabilities |= WL_SEAT_CAPABILITY_TOUCH;
    }
    return capabilities;
}

void Seat::Private::sendCapabilities()
{
    send<wl_seat_send_capabilities>(getCapabilities());
}

void Seat::setHasKeyboard(bool has)
{
    if (d_ptr->keyboard == has) {
        return;
    }
    d_ptr->keyboard = has;
    Q_EMIT hasKeyboardChanged(d_ptr->keyboard);
}

void Seat::setHasPointer(bool has)
{
    if (d_ptr->pointer == has) {
        return;
    }
    d_ptr->pointer = has;
    Q_EMIT hasPointerChanged(d_ptr->pointer);
}

void Seat::setHasTouch(bool has)
{
    if (d_ptr->touch == has) {
        return;
    }
    d_ptr->touch = has;
    Q_EMIT hasTouchChanged(d_ptr->touch);
}

void Seat::setName(const std::string& name)
{
    if (d_ptr->name == name) {
        return;
    }
    d_ptr->name = name;
    Q_EMIT nameChanged(d_ptr->name);
}

void Seat::Private::getPointerCallback(SeatBind* bind, uint32_t id)
{
    auto& manager = bind->global()->handle()->d_ptr->pointers;
    manager.create_device(bind->client()->handle(), bind->version(), id);
}

void Seat::Private::getKeyboardCallback(SeatBind* bind, uint32_t id)
{
    auto& manager = bind->global()->handle()->d_ptr->keyboards;
    manager.create_device(bind->client()->handle(), bind->version(), id);
}

void Seat::Private::getTouchCallback(SeatBind* bind, uint32_t id)
{
    auto& manager = bind->global()->handle()->d_ptr->touches;
    manager.create_device(bind->client()->handle(), bind->version(), id);
}

std::string Seat::name() const
{
    return d_ptr->name;
}

bool Seat::hasPointer() const
{
    return d_ptr->pointer;
}

bool Seat::hasKeyboard() const
{
    return d_ptr->keyboard;
}

bool Seat::hasTouch() const
{
    return d_ptr->touch;
}

QPointF Seat::pointerPos() const
{
    return d_ptr->pointers.pos;
}

void Seat::setPointerPos(const QPointF& pos)
{
    d_ptr->pointers.set_position(pos);
}

uint32_t Seat::timestamp() const
{
    return d_ptr->timestamp;
}

void Seat::setTimestamp(uint32_t time)
{
    if (d_ptr->timestamp == time) {
        return;
    }
    d_ptr->timestamp = time;
    Q_EMIT timestampChanged(time);
}

void Seat::setDragTarget(Surface* surface,
                         const QPointF& globalPosition,
                         const QMatrix4x4& inputTransformation)
{
    d_ptr->drags.set_target(surface, globalPosition, inputTransformation);
}

void Seat::setDragTarget(Surface* surface, const QMatrix4x4& inputTransformation)
{
    d_ptr->drags.set_target(surface, inputTransformation);
}

Surface* Seat::focusedPointerSurface() const
{
    return d_ptr->pointers.focus.surface;
}

void Seat::setFocusedPointerSurface(Surface* surface, const QPointF& surfacePosition)
{
    d_ptr->pointers.set_focused_surface(surface, surfacePosition);
}

void Seat::setFocusedPointerSurface(Surface* surface, const QMatrix4x4& transformation)
{
    d_ptr->pointers.set_focused_surface(surface, transformation);
}

Pointer* Seat::focusedPointer() const
{
    if (d_ptr->pointers.focus.devices.empty()) {
        return nullptr;
    }
    return d_ptr->pointers.focus.devices.front();
}

void Seat::setFocusedPointerSurfacePosition(const QPointF& surfacePosition)
{
    d_ptr->pointers.set_focused_surface_position(surfacePosition);
}

QPointF Seat::focusedPointerSurfacePosition() const
{
    return d_ptr->pointers.focus.offset;
}

void Seat::setFocusedPointerSurfaceTransformation(const QMatrix4x4& transformation)
{
    d_ptr->pointers.set_focused_surface_transformation(transformation);
}

QMatrix4x4 Seat::focusedPointerSurfaceTransformation() const
{
    return d_ptr->pointers.focus.transformation;
}

bool Seat::isPointerButtonPressed(Qt::MouseButton button) const
{
    return d_ptr->pointers.is_button_pressed(button);
}

bool Seat::isPointerButtonPressed(uint32_t button) const
{
    return d_ptr->pointers.is_button_pressed(button);
}

void Seat::pointerAxisV5(Qt::Orientation orientation,
                         qreal delta,
                         int32_t discreteDelta,
                         PointerAxisSource source)
{
    d_ptr->pointers.send_axis(orientation, delta, discreteDelta, source);
}

void Seat::pointerAxis(Qt::Orientation orientation, uint32_t delta)
{
    d_ptr->pointers.send_axis(orientation, delta);
}

void Seat::pointerButtonPressed(Qt::MouseButton button)
{
    d_ptr->pointers.button_pressed(button);
}

void Seat::pointerButtonPressed(uint32_t button)
{
    d_ptr->pointers.button_pressed(button);
}

void Seat::pointerButtonReleased(Qt::MouseButton button)
{
    d_ptr->pointers.button_released(button);
}

void Seat::pointerButtonReleased(uint32_t button)
{
    d_ptr->pointers.button_released(button);
}

uint32_t Seat::pointerButtonSerial(Qt::MouseButton button) const
{
    return d_ptr->pointers.button_serial(button);
}

uint32_t Seat::pointerButtonSerial(uint32_t button) const
{
    return d_ptr->pointers.button_serial(button);
}

void Seat::relativePointerMotion(const QSizeF& delta,
                                 const QSizeF& deltaNonAccelerated,
                                 uint64_t microseconds)
{
    d_ptr->pointers.relative_motion(delta, deltaNonAccelerated, microseconds);
}

void Seat::startPointerSwipeGesture(uint32_t fingerCount)
{
    d_ptr->pointers.start_swipe_gesture(fingerCount);
}

void Seat::updatePointerSwipeGesture(const QSizeF& delta)
{
    d_ptr->pointers.update_swipe_gesture(delta);
}

void Seat::endPointerSwipeGesture()
{
    d_ptr->pointers.end_swipe_gesture();
}

void Seat::cancelPointerSwipeGesture()
{
    d_ptr->pointers.cancel_swipe_gesture();
}

void Seat::startPointerPinchGesture(uint32_t fingerCount)
{
    d_ptr->pointers.start_pinch_gesture(fingerCount);
}

void Seat::updatePointerPinchGesture(const QSizeF& delta, qreal scale, qreal rotation)
{
    d_ptr->pointers.update_pinch_gesture(delta, scale, rotation);
}

void Seat::endPointerPinchGesture()
{
    d_ptr->pointers.end_pinch_gesture();
}

void Seat::cancelPointerPinchGesture()
{
    d_ptr->pointers.cancel_pinch_gesture();
}

void Seat::keyPressed(uint32_t key)
{
    d_ptr->keyboards.key_pressed(key);
}

void Seat::keyReleased(uint32_t key)
{
    d_ptr->keyboards.key_released(key);
}

Surface* Seat::focusedKeyboardSurface() const
{
    return d_ptr->keyboards.focus.surface;
}

void Seat::setFocusedKeyboardSurface(Surface* surface)
{
    d_ptr->keyboards.set_focused_surface(surface);
    d_ptr->data_devices.set_focused_surface(surface);
    d_ptr->primary_selection_devices.set_focused_surface(surface);

    // Focused text input surface follows keyboard.
    if (hasKeyboard()) {
        setFocusedTextInputSurface(surface);
    }
}

void Seat::setKeymap(std::string const& content)
{
    d_ptr->keyboards.set_keymap(content);
}

void Seat::updateKeyboardModifiers(uint32_t depressed,
                                   uint32_t latched,
                                   uint32_t locked,
                                   uint32_t group)
{
    d_ptr->keyboards.update_modifiers(depressed, latched, locked, group);
}

void Seat::setKeyRepeatInfo(int32_t charactersPerSecond, int32_t delay)
{
    d_ptr->keyboards.set_repeat_info(charactersPerSecond, delay);
}

int32_t Seat::keyRepeatDelay() const
{
    return d_ptr->keyboards.keyRepeat.delay;
}

int32_t Seat::keyRepeatRate() const
{
    return d_ptr->keyboards.keyRepeat.charactersPerSecond;
}

bool Seat::isKeymapXkbCompatible() const
{
    return d_ptr->keyboards.keymap.xkbcommonCompatible;
}

int Seat::keymapFileDescriptor() const
{
    return d_ptr->keyboards.keymap.fd;
}

uint32_t Seat::keymapSize() const
{
    return d_ptr->keyboards.keymap.content.size();
}

uint32_t Seat::depressedModifiers() const
{
    return d_ptr->keyboards.modifiers.depressed;
}

uint32_t Seat::groupModifiers() const
{
    return d_ptr->keyboards.modifiers.group;
}

uint32_t Seat::latchedModifiers() const
{
    return d_ptr->keyboards.modifiers.latched;
}

uint32_t Seat::lockedModifiers() const
{
    return d_ptr->keyboards.modifiers.locked;
}

uint32_t Seat::lastModifiersSerial() const
{
    return d_ptr->keyboards.modifiers.serial;
}

std::vector<uint32_t> Seat::pressedKeys() const
{
    return d_ptr->keyboards.pressed_keys();
}

Keyboard* Seat::focusedKeyboard() const
{
    if (d_ptr->keyboards.focus.devices.empty()) {
        return nullptr;
    }
    return d_ptr->keyboards.focus.devices.front();
}

void Seat::cancelTouchSequence()
{
    d_ptr->touches.cancel_sequence();
}

Touch* Seat::focusedTouch() const
{
    if (d_ptr->touches.focus.devices.empty()) {
        return nullptr;
    }
    return d_ptr->touches.focus.devices.front();
}

Surface* Seat::focusedTouchSurface() const
{
    return d_ptr->touches.focus.surface;
}

QPointF Seat::focusedTouchSurfacePosition() const
{
    return d_ptr->touches.focus.offset;
}

bool Seat::isTouchSequence() const
{
    return d_ptr->touches.is_in_progress();
}

void Seat::setFocusedTouchSurface(Surface* surface, const QPointF& surfacePosition)
{
    d_ptr->touches.set_focused_surface(surface, surfacePosition);
}

void Seat::setFocusedTouchSurfacePosition(const QPointF& surfacePosition)
{
    d_ptr->touches.set_focused_surface_position(surfacePosition);
}

int32_t Seat::touchDown(const QPointF& globalPosition)
{
    return d_ptr->touches.touch_down(globalPosition);
}

void Seat::touchMove(int32_t id, const QPointF& globalPosition)
{
    d_ptr->touches.touch_move(id, globalPosition);
}

void Seat::touchUp(int32_t id)
{
    d_ptr->touches.touch_up(id);
}

void Seat::touchFrame()
{
    d_ptr->touches.touch_frame();
}

bool Seat::hasImplicitTouchGrab(uint32_t serial) const
{
    return d_ptr->touches.has_implicit_grab(serial);
}

input_method_v2* Seat::get_input_method_v2() const
{
    return d_ptr->input_method;
}

bool Seat::isDrag() const
{
    return d_ptr->drags.is_in_progress();
}

bool Seat::isDragPointer() const
{
    return d_ptr->drags.is_pointer_drag();
}

bool Seat::isDragTouch() const
{
    return d_ptr->drags.is_touch_drag();
}

bool Seat::hasImplicitPointerGrab(uint32_t serial) const
{
    return d_ptr->pointers.has_implicit_grab(serial);
}

QMatrix4x4 Seat::dragSurfaceTransformation() const
{
    return d_ptr->drags.transformation;
}

Surface* Seat::dragSurface() const
{
    return d_ptr->drags.surface;
}

Pointer* Seat::dragPointer() const
{
    if (d_ptr->drags.mode != drag_pool::Mode::Pointer) {
        return nullptr;
    }

    return d_ptr->drags.sourcePointer;
}

DataDevice* Seat::dragSource() const
{
    return d_ptr->drags.source;
}

bool Seat::setFocusedTextInputV2Surface(Surface* surface)
{
    return d_ptr->text_inputs.set_v2_focused_surface(surface);
}

bool Seat::setFocusedTextInputV3Surface(Surface* surface)
{
    return d_ptr->text_inputs.set_v3_focused_surface(surface);
}

void Seat::setFocusedTextInputSurface(Surface* surface)
{
    d_ptr->text_inputs.set_focused_surface(surface);
}

Surface* Seat::focusedTextInputSurface() const
{
    return d_ptr->text_inputs.focus.surface;
}

TextInputV2* Seat::focusedTextInputV2() const
{
    return d_ptr->text_inputs.v2.text_input;
}

text_input_v3* Seat::focusedTextInputV3() const
{
    return d_ptr->text_inputs.v3.text_input;
}

DataDevice* Seat::selection() const
{
    return d_ptr->data_devices.current_selection;
}

void Seat::setSelection(DataDevice* dataDevice)
{
    d_ptr->data_devices.set_selection(dataDevice);
}

PrimarySelectionDevice* Seat::primarySelection() const
{
    return d_ptr->primary_selection_devices.current_selection;
}

void Seat::setPrimarySelection(PrimarySelectionDevice* dataDevice)
{
    d_ptr->primary_selection_devices.set_selection(dataDevice);
}

}
