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
    d_ptr->set_capability(WL_SEAT_CAPABILITY_KEYBOARD, d_ptr->keyboards, has);
}

void Seat::setHasPointer(bool has)
{
    d_ptr->set_capability(WL_SEAT_CAPABILITY_POINTER, d_ptr->pointers, has);
}

void Seat::setHasTouch(bool has)
{
    d_ptr->set_capability(WL_SEAT_CAPABILITY_TOUCH, d_ptr->touches, has);
}

pointer_pool& Seat::pointers() const
{
    return *d_ptr->pointers;
}

keyboard_pool& Seat::keyboards() const
{
    return *d_ptr->keyboards;
}

touch_pool& Seat::touches() const
{
    return *d_ptr->touches;
}

text_input_pool& Seat::text_inputs() const
{
    return d_ptr->text_inputs;
}

drag_pool& Seat::drags() const
{
    return d_ptr->drags;
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
    auto priv = bind->global()->handle()->d_ptr.get();
    auto& manager = priv->pointers;
    if (!manager) {
        // If we have no pointer capability we ignore the created resource.
        if (!(priv->prior_caps & WL_SEAT_CAPABILITY_POINTER)) {
            bind->post_error(WL_SEAT_ERROR_MISSING_CAPABILITY,
                             "Seat never had the pointer capability");
        }
        return;
    }
    manager.value().create_device(bind->client()->handle(), bind->version(), id);
}

void Seat::Private::getKeyboardCallback(SeatBind* bind, uint32_t id)
{
    auto priv = bind->global()->handle()->d_ptr.get();
    auto& manager = priv->keyboards;
    if (!manager) {
        // If we have no keyboard capability we ignore the created resource.
        if (!(priv->prior_caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
            bind->post_error(WL_SEAT_ERROR_MISSING_CAPABILITY,
                             "Seat never had the keyboard capability");
        }
        return;
    }
    manager.value().create_device(bind->client()->handle(), bind->version(), id);
}

void Seat::Private::getTouchCallback(SeatBind* bind, uint32_t id)
{
    auto priv = bind->global()->handle()->d_ptr.get();
    auto& manager = priv->touches;
    if (!manager) {
        // If we have no touch capability we ignore the created resource.
        if (!(priv->prior_caps & WL_SEAT_CAPABILITY_TOUCH)) {
            bind->post_error(WL_SEAT_ERROR_MISSING_CAPABILITY,
                             "Seat never had the touch capability");
        }
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

void Seat::setFocusedKeyboardSurface(Surface* surface)
{
    assert(hasKeyboard());

    // Data sharing devices receive a potential selection directly before keyboard enter.
    d_ptr->data_devices.set_focused_surface(surface);
    d_ptr->primary_selection_devices.set_focused_surface(surface);

    d_ptr->keyboards.value().set_focused_surface(surface);

    // Focused text input surface follows keyboard.
    d_ptr->text_inputs.set_focused_surface(surface);
}

input_method_v2* Seat::get_input_method_v2() const
{
    return d_ptr->input_method;
}

data_source* Seat::selection() const
{
    return d_ptr->data_devices.focus.source;
}

void Seat::setSelection(data_source* source)
{
    d_ptr->data_devices.set_selection(source);
}

primary_selection_source* Seat::primarySelection() const
{
    return d_ptr->primary_selection_devices.focus.source;
}

void Seat::setPrimarySelection(primary_selection_source* source)
{
    d_ptr->primary_selection_devices.set_selection(source);
}

}
