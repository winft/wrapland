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
#include "input_method_v2.h"
#include "primary_selection.h"
#include "surface.h"
#include "text_input_v2_p.h"
#include "text_input_v3_p.h"
#include "utils.h"

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

std::vector<DataDevice*> Seat::Private::dataDevicesForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, dataDevices);
}

TextInputV2* Seat::Private::textInputV2ForSurface(Surface* surface) const
{
    return interfaceForSurface(surface, textInputs);
}

text_input_v3* Seat::Private::textInputV3ForSurface(Surface* surface) const
{
    return interfaceForSurface(surface, textInputsV3);
}

void Seat::Private::cleanupDataDevice(DataDevice* dataDevice)
{
    remove_one(dataDevices, dataDevice);
    remove_one(keyboards.focus.selections, dataDevice);
    if (currentSelection == dataDevice) {
        // current selection is cleared
        currentSelection = nullptr;
        Q_EMIT q_ptr->selectionChanged(nullptr);
        for (auto selection : keyboards.focus.selections) {
            selection->sendClearSelection();
        }
    }
}

template<>
void Seat::Private::register_device(DataDevice* device)
{
    registerDataDevice(device);
}

void Seat::Private::registerDataDevice(DataDevice* dataDevice)
{
    dataDevices.push_back(dataDevice);

    QObject::connect(dataDevice, &DataDevice::resourceDestroyed, q_ptr, [this, dataDevice] {
        cleanupDataDevice(dataDevice);
    });

    QObject::connect(dataDevice, &DataDevice::selectionChanged, q_ptr, [this, dataDevice] {
        updateSelection(dataDevice, true);
    });
    QObject::connect(dataDevice, &DataDevice::selectionCleared, q_ptr, [this, dataDevice] {
        updateSelection(dataDevice, false);
    });

    QObject::connect(dataDevice, &DataDevice::dragStarted, q_ptr, [this, dataDevice] {
        drags.perform_drag(dataDevice);
    });

    // Is the new DataDevice for the current keyoard focus?
    if (keyboards.focus.surface) {
        // Same client?
        if (keyboards.focus.surface->client() == dataDevice->client()) {
            keyboards.focus.selections.push_back(dataDevice);
            if (currentSelection && currentSelection->selection()) {
                dataDevice->sendSelection(currentSelection);
            }
        }
    }
}

void Seat::Private::cleanupPrimarySelectionDevice(PrimarySelectionDevice* primarySelectionDevice)
{
    remove_one(primarySelectionDevices, primarySelectionDevice);
    remove_one(keyboards.focus.primarySelections, primarySelectionDevice);
    if (currentPrimarySelectionDevice == primarySelectionDevice) {
        // current selection is cleared
        currentPrimarySelectionDevice = nullptr;
        Q_EMIT q_ptr->primarySelectionChanged(nullptr);
        for (auto selection : keyboards.focus.primarySelections) {
            selection->sendClearSelection();
        }
    }
}

template<>
void Seat::Private::register_device(PrimarySelectionDevice* device)
{
    registerPrimarySelectionDevice(device);
}

void Seat::Private::registerPrimarySelectionDevice(PrimarySelectionDevice* primarySelectionDevice)
{
    primarySelectionDevices.push_back(primarySelectionDevice);

    QObject::connect(
        primarySelectionDevice,
        &PrimarySelectionDevice::resourceDestroyed,
        q_ptr,
        [this, primarySelectionDevice] { cleanupPrimarySelectionDevice(primarySelectionDevice); });
    QObject::connect(
        primarySelectionDevice,
        &PrimarySelectionDevice::selectionChanged,
        q_ptr,
        [this, primarySelectionDevice] { updateSelection(primarySelectionDevice, true); });
    QObject::connect(
        primarySelectionDevice,
        &PrimarySelectionDevice::selectionCleared,
        q_ptr,
        [this, primarySelectionDevice] { updateSelection(primarySelectionDevice, false); });

    // Is the new PrimarySelectionDevice for the current keyoard on focus?
    if (keyboards.focus.surface) {
        // Same client?
        if (keyboards.focus.surface->client() == primarySelectionDevice->client()) {
            keyboards.focus.primarySelections.push_back(primarySelectionDevice);
            if (currentPrimarySelectionDevice && currentPrimarySelectionDevice->selection()) {
                primarySelectionDevice->sendSelection(currentPrimarySelectionDevice);
            }
        }
    }
}

void Seat::Private::cancelPreviousSelection(PrimarySelectionDevice* dataDevice) const
{
    if (!currentPrimarySelectionDevice) {
        return;
    }
    if (auto s = currentPrimarySelectionDevice->selection()) {
        if (currentPrimarySelectionDevice != dataDevice) {
            s->cancel();
        }
    }
}

void Seat::Private::updateSelection(PrimarySelectionDevice* dataDevice, bool set)
{
    bool selChanged = currentPrimarySelectionDevice != dataDevice;
    if (keyboards.focus.surface && (keyboards.focus.surface->client() == dataDevice->client())) {
        // cancel the previous selection
        cancelPreviousSelection(dataDevice);
        // new selection on a data device belonging to current keyboard focus
        currentPrimarySelectionDevice = dataDevice;
    }
    if (dataDevice == currentPrimarySelectionDevice) {
        // need to send out the selection
        for (auto focussedDevice : keyboards.focus.primarySelections) {
            if (set) {
                focussedDevice->sendSelection(dataDevice);
            } else {
                focussedDevice->sendClearSelection();
                currentPrimarySelectionDevice = nullptr;
                selChanged = true;
            }
        }
    }
    if (selChanged) {
        Q_EMIT q_ptr->primarySelectionChanged(currentPrimarySelectionDevice);
    }
}

void Seat::Private::registerInputMethod(input_method_v2* im)
{
    assert(!input_method);
    input_method = im;

    QObject::connect(im, &input_method_v2::resourceDestroyed, q_ptr, [this] {
        input_method = nullptr;
        Q_EMIT q_ptr->input_method_v2_changed();
    });
    Q_EMIT q_ptr->input_method_v2_changed();
}

void Seat::Private::registerTextInput(TextInputV2* ti)
{
    // Text input version 0 might call this multiple times.
    if (std::find(textInputs.begin(), textInputs.end(), ti) != textInputs.end()) {
        return;
    }
    textInputs.push_back(ti);
    if (global_text_input.focus.surface
        && global_text_input.focus.surface->client() == ti->d_ptr->client()->handle()) {
        // This is a text input for the currently focused text input surface.
        if (!global_text_input.v2.text_input) {
            global_text_input.v2.text_input = ti;
            ti->d_ptr->sendEnter(global_text_input.focus.surface, global_text_input.v2.serial);
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &TextInputV2::resourceDestroyed, q_ptr, [this, ti] {
        remove_one(textInputs, ti);
        if (global_text_input.v2.text_input == ti) {
            global_text_input.v2.text_input = nullptr;
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    });
}

void Seat::Private::registerTextInput(text_input_v3* ti)
{
    // Text input version 0 might call this multiple times.
    if (find(textInputsV3.begin(), textInputsV3.end(), ti) != textInputsV3.end()) {
        return;
    }
    textInputsV3.push_back(ti);
    if (global_text_input.focus.surface
        && global_text_input.focus.surface->client() == ti->d_ptr->client()->handle()) {
        // This is a text input for the currently focused text input surface.
        if (!global_text_input.v3.text_input) {
            global_text_input.v3.text_input = ti;
            ti->d_ptr->send_enter(global_text_input.focus.surface);
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &text_input_v3::resourceDestroyed, q_ptr, [this, ti] {
        remove_one(textInputsV3, ti);
        if (global_text_input.v3.text_input == ti) {
            global_text_input.v3.text_input = nullptr;
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    });
}

void Seat::Private::cancelPreviousSelection(DataDevice* dataDevice) const
{
    if (!currentSelection) {
        return;
    }
    if (auto s = currentSelection->selection()) {
        if (currentSelection != dataDevice) {
            // only if current selection is not on the same device
            // that would cancel the newly set source
            s->cancel();
        }
    }
}

void Seat::Private::updateSelection(DataDevice* dataDevice, bool set)
{
    bool selChanged = currentSelection != dataDevice;
    if (keyboards.focus.surface && (keyboards.focus.surface->client() == dataDevice->client())) {
        // cancel the previous selection
        cancelPreviousSelection(dataDevice);
        // new selection on a data device belonging to current keyboard focus
        currentSelection = dataDevice;
    }
    if (dataDevice == currentSelection) {
        // need to send out the selection
        for (auto focussedDevice : keyboards.focus.selections) {
            if (set) {
                focussedDevice->sendSelection(dataDevice);
            } else {
                focussedDevice->sendClearSelection();
                currentSelection = nullptr;
                selChanged = true;
            }
        }
    }
    if (selChanged) {
        Q_EMIT q_ptr->selectionChanged(currentSelection);
    }
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

    if (d_ptr->keyboards.focus.surface) {
        auto dataDevices = d_ptr->dataDevicesForSurface(surface);
        d_ptr->keyboards.focus.selections = dataDevices;
        for (auto dataDevice : dataDevices) {
            if (d_ptr->currentSelection) {
                dataDevice->sendSelection(d_ptr->currentSelection);
            } else {
                dataDevice->sendClearSelection();
            }
        }
        auto const primarySelectionDevices
            = interfacesForSurface(surface, d_ptr->primarySelectionDevices);
        d_ptr->keyboards.focus.primarySelections = primarySelectionDevices;
        for (auto dataDevice : primarySelectionDevices) {
            if (d_ptr->currentPrimarySelectionDevice) {
                dataDevice->sendSelection(d_ptr->currentPrimarySelectionDevice);
            } else {
                dataDevice->sendClearSelection();
            }
        }
    }

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
    auto const serial = d_ptr->display()->handle()->nextSerial();
    auto const old_ti = d_ptr->global_text_input.v2.text_input;

    if (old_ti) {
        // TODO(unknown author): setFocusedSurface like in other interfaces
        old_ti->d_ptr->sendLeave(serial, d_ptr->global_text_input.focus.surface);
    }

    auto ti = d_ptr->textInputV2ForSurface(surface);

    if (ti && !ti->d_ptr->resource()) {
        // TODO(romangg): can this check be removed?
        ti = nullptr;
    }

    d_ptr->global_text_input.v2.text_input = ti;

    if (surface) {
        d_ptr->global_text_input.v2.serial = serial;
    }

    if (ti) {
        // TODO(unknown author): setFocusedSurface like in other interfaces
        ti->d_ptr->sendEnter(surface, serial);
    }

    return old_ti != ti;
}

bool Seat::setFocusedTextInputV3Surface(Surface* surface)
{
    auto const old_ti = d_ptr->global_text_input.v3.text_input;

    if (old_ti) {
        old_ti->d_ptr->send_leave(d_ptr->global_text_input.focus.surface);
    }

    auto ti = d_ptr->textInputV3ForSurface(surface);

    if (ti && !ti->d_ptr->resource()) {
        // TODO(romangg): can this check be removed?
        ti = nullptr;
    }

    d_ptr->global_text_input.v3.text_input = ti;

    if (ti) {
        ti->d_ptr->send_enter(surface);
    }

    return old_ti != ti;
}

void Seat::setFocusedTextInputSurface(Surface* surface)
{
    if (d_ptr->global_text_input.focus.surface) {
        disconnect(d_ptr->global_text_input.focus.destroy_connection);
    }

    auto changed = setFocusedTextInputV2Surface(surface) || setFocusedTextInputV3Surface(surface);
    d_ptr->global_text_input.focus = {};

    if (surface) {
        d_ptr->global_text_input.focus.surface = surface;
        d_ptr->global_text_input.focus.destroy_connection
            = connect(surface, &Surface::resourceDestroyed, this, [this] {
                  setFocusedTextInputSurface(nullptr);
              });
    }

    if (changed) {
        Q_EMIT focusedTextInputChanged();
    }
}

Surface* Seat::focusedTextInputSurface() const
{
    return d_ptr->global_text_input.focus.surface;
}

TextInputV2* Seat::focusedTextInputV2() const
{
    return d_ptr->global_text_input.v2.text_input;
}

text_input_v3* Seat::focusedTextInputV3() const
{
    return d_ptr->global_text_input.v3.text_input;
}

DataDevice* Seat::selection() const
{
    return d_ptr->currentSelection;
}

void Seat::setSelection(DataDevice* dataDevice)
{
    if (d_ptr->currentSelection == dataDevice) {
        return;
    }
    // cancel the previous selection
    d_ptr->cancelPreviousSelection(dataDevice);
    d_ptr->currentSelection = dataDevice;

    for (auto focusedDevice : d_ptr->keyboards.focus.selections) {
        if (dataDevice && dataDevice->selection()) {
            focusedDevice->sendSelection(dataDevice);
        } else {
            focusedDevice->sendClearSelection();
        }
    }
    Q_EMIT selectionChanged(dataDevice);
}

PrimarySelectionDevice* Seat::primarySelection() const
{
    return d_ptr->currentPrimarySelectionDevice;
}

void Seat::setPrimarySelection(PrimarySelectionDevice* dataDevice)
{
    if (d_ptr->currentPrimarySelectionDevice == dataDevice) {
        return;
    }
    // cancel the previous selection
    d_ptr->cancelPreviousSelection(dataDevice);
    d_ptr->currentPrimarySelectionDevice = dataDevice;

    for (auto focusedDevice : d_ptr->keyboards.focus.primarySelections) {
        if (dataDevice && dataDevice->selection()) {
            focusedDevice->sendSelection(dataDevice);
        } else {
            focusedDevice->sendClearSelection();
        }
    }
    Q_EMIT primarySelectionChanged(dataDevice);
}

}
