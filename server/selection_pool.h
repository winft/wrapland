/*
    SPDX-FileCopyrightText: 2020, 2021 Roman Gilg <subdiff@gmail.com>
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

    struct {
        std::vector<Device*> devices;
        Source* source{nullptr};
        QMetaObject::Connection source_destroy_notifier;
    } focus;

    std::vector<Device*> devices;

private:
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

    QObject::connect(device, &Device::resourceDestroyed, seat, [this, device] {
        remove_one(devices, device);
        remove_one(focus.devices, device);
    });

    QObject::connect(device, &Device::selection_changed, seat, [this, device] {
        if (has_keyboard_focus(device, seat)) {
            set_selection(device->selection());
        }
    });

    if (has_keyboard_focus(device, seat)) {
        focus.devices.push_back(device);
        if (focus.source) {
            device->send_selection(focus.source);
        }
    }
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::set_focused_surface(Surface* surface)
{
    if (!surface) {
        // No surface set. Per protocol we just won't send future selection events to the client.
        focus.devices = {};
        return;
    }

    if (focus.devices.size() && focus.devices.front()->client() == surface->client()) {
        // No client change. Selection must not be resent.
        return;
    }

    focus.devices = interfacesForSurface(surface, devices);
    if (focus.source) {
        transmit(focus.source);
    }
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::set_selection(Source* source)
{
    if (focus.source == source) {
        return;
    }

    auto old_source = focus.source;
    focus.source = source;

    QObject::disconnect(focus.source_destroy_notifier);
    focus.source_destroy_notifier = QMetaObject::Connection();

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

    if (old_source) {
        old_source->cancel();
    }
}

template<typename Device, typename Source, void (Seat::*signal)(Source*)>
void selection_pool<Device, Source, signal>::transmit(Source* source)
{
    std::for_each(focus.devices.begin(), focus.devices.end(), [source](Device* dev) {
        dev->send_selection(source);
    });
}

}
