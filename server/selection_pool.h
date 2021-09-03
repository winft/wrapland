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
    void set_focused_surface(Surface* surface);

    void set_selection(Source* source);
    void clear_selection(Device* device);

    struct {
        std::vector<Device*> devices;
        Source* source{nullptr};
        QMetaObject::Connection source_destroy_notifier;
    } focus;

    std::vector<Device*> devices;

private:
    void cleanup_device(Device* device);

    void change_selection(Device* device, Source* source);
    void update_selection(Device* device, Source* source);

    void transmit(Source* source);

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
        assert(source);
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
void selection_pool<Device, Source, signal>::set_focused_surface(Surface* surface)
{
    focus.devices = interfacesForSurface(surface, devices);
    transmit(focus.source);
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::transmit(Source* source)
{
    if (source) {
        std::for_each(focus.devices.begin(), focus.devices.end(), [source](Device* dev) {
            dev->sendSelection(source);
        });
    } else {
        std::for_each(focus.devices.begin(), focus.devices.end(), [](Device* dev) {
            dev->sendClearSelection();
        });
    }
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::set_selection(Source* source)
{
    if (focus.source == source) {
        return;
    }

    QObject::disconnect(focus.source_destroy_notifier);
    focus.source_destroy_notifier = QMetaObject::Connection();

    if (focus.source) {
        focus.source->cancel();
    }

    focus.source = source;

    if (source) {
        focus.source_destroy_notifier
            = QObject::connect(source, &Source::resourceDestroyed, seat, [this] {
                  focus.source = nullptr;
                  transmit(nullptr);
                  Q_EMIT(seat->*signal)(nullptr);
              });
    }

    transmit(source);
    Q_EMIT(seat->*signal)(source);
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::update_selection(Device* device, Source* source)
{
    if (has_keyboard_focus(device, seat)) {
        set_selection(source);
    }
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::change_selection(Device* device, Source* source)
{
    update_selection(device, source);
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::clear_selection(Device* device)
{
    update_selection(device, nullptr);
}
}
