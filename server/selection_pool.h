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

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
struct selection_pool {
    explicit selection_pool(Seat* seat);

    void register_device(Device* device);
    void cleanup_device(Device* device);

    void set_selection(Source* source);

    void cancel_previous_selection(Source* new_source);
    void set_focused_surface(Surface* surface);
    void advertise();

    bool update_selection(Device* device, Source* source);
    void change_selection(Device* device, Source* source);
    void clear_selection(Device* device);
    void do_set_source(Source* source);

    struct {
        std::vector<Device*> devices;
        Source* source{nullptr};
        QMetaObject::Connection source_destroy_notifier;
    } focus;

    std::vector<Device*> devices;
    Seat* seat;
};

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
selection_pool<Device, Source, signal>::selection_pool(Seat* seat)
    : seat{seat}
{
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::register_device(Device* device)
{
    devices.push_back(device);

    QObject::connect(
        device, &Device::resourceDestroyed, seat, [this, device] { cleanup_device(device); });
    QObject::connect(device, &Device::selectionChanged, seat, [this, device](Source* source) {
        change_selection(device, source);
    });
    QObject::connect(
        device, &Device::selectionCleared, seat, [this, device] { clear_selection(device); });

    if (has_keyboard_focus(device, seat)) {
        focus.devices.push_back(device);
        if (focus.source) {
            device->sendSelection(focus.source);
        }
    }
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::cleanup_device(Device* device)
{
    remove_one(devices, device);
    remove_one(focus.devices, device);
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::cancel_previous_selection(Source* new_source)
{
    if (!focus.source || (focus.source == new_source)) {
        return;
    }
    QObject::disconnect(focus.source_destroy_notifier);
    focus.source_destroy_notifier = QMetaObject::Connection();
    focus.source->cancel();
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::set_focused_surface(Surface* surface)
{
    focus.devices = interfacesForSurface(surface, devices);
    advertise();
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::advertise()
{
    for (auto device : focus.devices) {
        if (focus.source) {
            device->sendSelection(focus.source);
        } else {
            device->sendClearSelection();
        }
    }
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::do_set_source(Source* source)
{
    cancel_previous_selection(source);

    if (source && focus.source != source) {
        focus.source_destroy_notifier
            = QObject::connect(source, &Source::resourceDestroyed, seat, [this] {
                  focus.source = nullptr;
                  advertise();
                  Q_EMIT(seat->*signal)(nullptr);
              });
    }
    focus.source = source;
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::set_selection(Source* source)
{
    if (focus.source == source) {
        return;
    }

    do_set_source(source);

    for (auto focusedDevice : focus.devices) {
        if (source) {
            focusedDevice->sendSelection(source);
        } else {
            focusedDevice->sendClearSelection();
        }
    }

    Q_EMIT(seat->*signal)(source);
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
bool selection_pool<Device, Source, signal>::update_selection(Device* device, Source* source)
{
    if (!has_keyboard_focus(device, seat) || focus.source == source) {
        return false;
    }

    do_set_source(source);
    return true;
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::change_selection(Device* device, Source* source)
{
    if (!update_selection(device, source)) {
        return;
    }

    for (auto dev : focus.devices) {
        dev->sendSelection(source);
    }

    Q_EMIT(seat->*signal)(focus.source);
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::clear_selection(Device* device)
{
    if (!update_selection(device, nullptr)) {
        return;
    }

    for (auto focusedDevice : focus.devices) {
        focusedDevice->sendClearSelection();
    }

    Q_EMIT(seat->*signal)(focus.source);
}
}
