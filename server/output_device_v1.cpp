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
#include "output_device_v1_p.h"

#include "output_p.h"

#include "display.h"

#include "wayland-output-device-v1-server-protocol.h"

namespace Wrapland::Server
{

OutputDeviceV1::Private::Private(Output* output, Display* display, OutputDeviceV1* q_ptr)
    : OutputDeviceV1Global(q_ptr, display, &zkwinft_output_device_v1_interface, nullptr)
    , displayHandle{display}
    , output(output)
{
    create();
    displayHandle->add_output_device_v1(q_ptr);
}

std::tuple<char const*, char const*, char const*, char const*, char const*, int32_t, int32_t>
info_args(OutputState const& state)
{
    return std::make_tuple(state.info.name.c_str(),
                           state.info.description.c_str(),
                           state.info.make.c_str(),
                           state.info.model.c_str(),
                           state.info.serial_number.c_str(),
                           state.info.physical_size.width(),
                           state.info.physical_size.height());
}

std::tuple<wl_fixed_t, wl_fixed_t, wl_fixed_t, wl_fixed_t> geometry_args(OutputState const& state)
{
    auto const geo = state.geometry;

    return std::make_tuple(wl_fixed_from_double(geo.x()),
                           wl_fixed_from_double(geo.y()),
                           wl_fixed_from_double(geo.width()),
                           wl_fixed_from_double(geo.height()));
}

void OutputDeviceV1::Private::bindInit(OutputDeviceV1Bind* bind)
{
    auto const state = output->d_ptr->published;

    send<zkwinft_output_device_v1_send_info>(bind, info_args(state));
    send<zkwinft_output_device_v1_send_enabled>(bind, state.enabled);

    for (auto const& mode : output->d_ptr->modes) {
        if (mode != output->d_ptr->published.mode) {
            sendMode(bind, mode);
        }
    }
    sendMode(bind, output->d_ptr->published.mode);

    send<zkwinft_output_device_v1_send_transform>(bind,
                                                  Output::Private::to_transform(state.transform));
    send<zkwinft_output_device_v1_send_geometry>(bind, geometry_args(state));

    send<zkwinft_output_device_v1_send_done>(bind);
    bind->client->flush();
}

void OutputDeviceV1::Private::sendMode(OutputDeviceV1Bind* bind, Output::Mode const& mode)
{
    auto flags = Output::Private::get_mode_flags(mode, output->d_ptr->published);

    send<zkwinft_output_device_v1_send_mode>(
        bind, flags, mode.size.width(), mode.size.height(), mode.refresh_rate, mode.id);
}

void OutputDeviceV1::Private::sendMode(Output::Mode const& mode)
{
    // Only called on update. In this case we want to send the pending mode.
    auto flags = Output::Private::get_mode_flags(mode, output->d_ptr->pending);

    send<zkwinft_output_device_v1_send_mode>(
        flags, mode.size.width(), mode.size.height(), mode.refresh_rate, mode.id);
}

bool OutputDeviceV1::Private::broadcast()
{
    auto const published = output->d_ptr->published;
    auto const pending = output->d_ptr->pending;

    bool changed = false;

    if (published.info.name != pending.info.name
        || published.info.description != pending.info.description
        || published.info.make != pending.info.make || published.info.model != pending.info.model
        || published.info.serial_number != pending.info.serial_number
        || published.info.physical_size != pending.info.physical_size) {
        send<zkwinft_output_device_v1_send_info>(info_args(pending));
        changed = true;
    }

    if (published.enabled != pending.enabled) {
        send<zkwinft_output_device_v1_send_enabled>(pending.enabled);
        changed = true;
    }

    if (published.mode != pending.mode) {
        sendMode(pending.mode);
        changed = true;
    }

    if (published.transform != pending.transform) {
        send<zkwinft_output_device_v1_send_transform>(
            Output::Private::to_transform(pending.transform));
        changed = true;
    }

    if (published.geometry != pending.geometry) {
        send<zkwinft_output_device_v1_send_geometry>(geometry_args(pending));
        changed = true;
    }

    return changed;
}

void OutputDeviceV1::Private::done()
{
    send<zkwinft_output_device_v1_send_done>();
}

OutputDeviceV1::OutputDeviceV1(Output* output, Display* display)
    : d_ptr(new Private(output, display, this))
{
}

OutputDeviceV1::~OutputDeviceV1()
{
    if (d_ptr->displayHandle) {
        d_ptr->displayHandle->removeOutputDevice(this);
    }
}

Output* OutputDeviceV1::output() const
{
    return d_ptr->output;
}

}
