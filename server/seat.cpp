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
    if (pointers) {
        capabilities |= WL_SEAT_CAPABILITY_POINTER;
    }
    if (keyboards) {
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (touches) {
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
    d_ptr->set_capability(d_ptr->keyboards, has);
}

void Seat::setHasPointer(bool has)
{
    d_ptr->set_capability(d_ptr->pointers, has);
}

void Seat::setHasTouch(bool has)
{
    d_ptr->set_capability(d_ptr->touches, has);
}

pointer_pool& Seat::pointers() const
{
    return *d_ptr->pointers;
}

keyboard_pool& Seat::keyboards() const
{
    return *d_ptr->keyboards;
}

void Seat::setName(const std::string& name)
{
    if (d_ptr->name == name) {
        return;
    }
    d_ptr->name = name;
    d_ptr->sendName();
}

void Seat::Private::getPointerCallback(SeatBind* bind, uint32_t id)
{
    auto& manager = bind->global()->handle()->d_ptr->pointers;
    if (!manager) {
        // If we have no pointer capability we ignore the created resource.
        return;
    }
    manager.value().create_device(bind->client()->handle(), bind->version(), id);
}

void Seat::Private::getKeyboardCallback(SeatBind* bind, uint32_t id)
{
    auto& manager = bind->global()->handle()->d_ptr->keyboards;
    if (!manager) {
        // If we have no keyboard capability we ignore the created resource.
        return;
    }
    manager.value().create_device(bind->client()->handle(), bind->version(), id);
}

void Seat::Private::getTouchCallback(SeatBind* bind, uint32_t id)
{
    auto& manager = bind->global()->handle()->d_ptr->touches;
    if (!manager) {
        // If we have no touch capability we ignore the created resource.
        return;
    }
    manager.value().create_device(bind->client()->handle(), bind->version(), id);
}

std::string Seat::name() const
{
    return d_ptr->name;
}

bool Seat::hasPointer() const
{
    return d_ptr->pointers.has_value();
}

bool Seat::hasKeyboard() const
{
    return d_ptr->keyboards.has_value();
}

bool Seat::hasTouch() const
{
    return d_ptr->touches.has_value();
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

void Seat::setFocusedKeyboardSurface(Surface* surface)
{
    assert(hasKeyboard());

    d_ptr->keyboards.value().set_focused_surface(surface);
    d_ptr->data_devices.set_focused_surface(surface);
    d_ptr->primary_selection_devices.set_focused_surface(surface);

    // Focused text input surface follows keyboard.
    setFocusedTextInputSurface(surface);
}

void Seat::cancelTouchSequence()
{
    d_ptr->touches.value().cancel_sequence();
}

Touch* Seat::focusedTouch() const
{
    if (d_ptr->touches.value().focus.devices.empty()) {
        return nullptr;
    }
    return d_ptr->touches.value().focus.devices.front();
}

Surface* Seat::focusedTouchSurface() const
{
    if (!d_ptr->touches) {
        return nullptr;
    }
    return d_ptr->touches.value().focus.surface;
}

QPointF Seat::focusedTouchSurfacePosition() const
{
    return d_ptr->touches.value().focus.offset;
}

bool Seat::isTouchSequence() const
{
    return d_ptr->touches.value().is_in_progress();
}

void Seat::setFocusedTouchSurface(Surface* surface, const QPointF& surfacePosition)
{
    d_ptr->touches.value().set_focused_surface(surface, surfacePosition);
}

void Seat::setFocusedTouchSurfacePosition(const QPointF& surfacePosition)
{
    d_ptr->touches.value().set_focused_surface_position(surfacePosition);
}

int32_t Seat::touchDown(const QPointF& globalPosition)
{
    return d_ptr->touches.value().touch_down(globalPosition);
}

void Seat::touchMove(int32_t id, const QPointF& globalPosition)
{
    d_ptr->touches.value().touch_move(id, globalPosition);
}

void Seat::touchUp(int32_t id)
{
    d_ptr->touches.value().touch_up(id);
}

void Seat::touchFrame()
{
    d_ptr->touches.value().touch_frame();
}

bool Seat::hasImplicitTouchGrab(uint32_t serial) const
{
    if (!d_ptr->touches) {
        return false;
    }
    return d_ptr->touches.value().has_implicit_grab(serial);
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
