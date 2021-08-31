/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "seat.h"
#include "surface.h"
#include "utils.h"

#include <vector>

namespace Wrapland::Server
{

template<typename Device, void (Seat::*signal)(Device*)>
struct selection_pool {
    explicit selection_pool(Seat* seat);

    void register_device(Device* device);
    void cleanup_device(Device* device);

    void set_selection(Device* device);

    void cancel_previous_selection(Device* new_device) const;
    void set_focused_surface(Surface* surface);
    void advertise();

    bool update_selection(Device* new_device);
    void change_selection(Device* device);
    void clear_selection(Device* device);

    std::vector<Device*> devices;

    std::vector<Device*> selections;
    Device* current_selection = nullptr;

    Seat* seat;
};

template<typename Device, void (Seat::*signal)(Device*)>
selection_pool<Device, signal>::selection_pool(Seat* seat)
    : seat{seat}
{
}

template<typename Device, void (Seat::*signal)(Device*)>
void selection_pool<Device, signal>::register_device(Device* device)
{
    devices.push_back(device);

    QObject::connect(
        device, &Device::resourceDestroyed, seat, [this, device] { cleanup_device(device); });
    QObject::connect(
        device, &Device::selectionChanged, seat, [this, device] { change_selection(device); });
    QObject::connect(
        device, &Device::selectionCleared, seat, [this, device] { clear_selection(device); });

    if (has_keyboard_focus(device, seat)) {
        selections.push_back(device);
        if (current_selection && current_selection->selection()) {
            device->sendSelection(current_selection);
        }
    }
}

template<typename Device, void (Seat::*signal)(Device*)>
void selection_pool<Device, signal>::cleanup_device(Device* device)
{
    remove_one(devices, device);
    remove_one(selections, device);
    if (current_selection == device) {
        current_selection = nullptr;
        Q_EMIT(seat->*signal)(nullptr);
        advertise();
    }
}

template<typename Device, void (Seat::*signal)(Device*)>
void selection_pool<Device, signal>::cancel_previous_selection(Device* new_device) const
{
    if (!current_selection || (current_selection == new_device)) {
        return;
    }
    if (auto s = current_selection->selection()) {
        s->cancel();
    }
}

template<typename Device, void (Seat::*signal)(Device*)>
void selection_pool<Device, signal>::set_focused_surface(Surface* surface)
{
    selections = interfacesForSurface(surface, devices);
    advertise();
}

template<typename Device, void (Seat::*signal)(Device*)>
void selection_pool<Device, signal>::advertise()
{
    for (auto device : selections) {
        if (current_selection) {
            device->sendSelection(current_selection);
        } else {
            device->sendClearSelection();
        }
    }
}

template<typename Device, void (Seat::*signal)(Device*)>
void selection_pool<Device, signal>::set_selection(Device* device)
{
    if (current_selection == device) {
        return;
    }
    cancel_previous_selection(device);
    current_selection = device;

    for (auto focusedDevice : selections) {
        if (device && device->selection()) {
            focusedDevice->sendSelection(device);
        } else {
            focusedDevice->sendClearSelection();
        }
    }
    Q_EMIT(seat->*signal)(device);
}

template<typename Device, void (Seat::*signal)(Device*)>
bool selection_pool<Device, signal>::update_selection(Device* device)
{
    bool selChanged = current_selection != device;
    if (has_keyboard_focus(device, seat)) {
        cancel_previous_selection(device);
        current_selection = device;
    }
    return selChanged;
}

template<typename Device, void (Seat::*signal)(Device*)>
void selection_pool<Device, signal>::change_selection(Device* device)
{
    bool selChanged = update_selection(device);
    if (device == current_selection) {
        for (auto focusedDevice : selections) {
            focusedDevice->sendSelection(device);
        }
    }
    if (selChanged) {
        Q_EMIT(seat->*signal)(current_selection);
    }
}

template<typename Device, void (Seat::*signal)(Device*)>
void selection_pool<Device, signal>::clear_selection(Device* device)
{
    bool selChanged = update_selection(device);
    if (device == current_selection) {
        for (auto focusedDevice : selections) {
            focusedDevice->sendClearSelection();
            current_selection = nullptr;
            selChanged = true;
        }
    }
    if (selChanged) {
        Q_EMIT(seat->*signal)(current_selection);
    }
}

}
