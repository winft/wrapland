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
#include "output_p.h"

#include "output_device_v1_p.h"
#include "wl_output_p.h"
#include "xdg_output_p.h"

#include "display.h"

#include "wayland/client.h"
#include "wayland/display.h"

#include <cassert>
#include <functional>
#include <wayland-server.h>

namespace Wrapland::Server
{

output::Private::Private(Display* display, output* q_ptr)
    : display_handle(display)
    , device{new OutputDeviceV1(q_ptr, display)}
    , q_ptr{q_ptr}
{
}

void output::Private::done()
{
    if (published.enabled != pending.enabled) {
        if (pending.enabled) {
            wayland_output.reset(new WlOutput(q_ptr, display_handle));
            xdg_output.reset(new XdgOutput(q_ptr, display_handle));
        } else {
            wayland_output.reset();
            xdg_output.reset();
        }
    }
    if (pending.enabled) {
        auto wayland_change = wayland_output->d_ptr->broadcast();
        auto xdg_change = xdg_output->d_ptr->broadcast();
        if (wayland_change || xdg_change) {
            wayland_output->d_ptr->done();
            xdg_output->d_ptr->done();
        }
    }
    if (device->d_ptr->broadcast()) {
        device->d_ptr->done();
    }
    published = pending;
}

void output::Private::done_wl(Client* client) const
{
    if (!wayland_output) {
        return;
    }

    auto binds = wayland_output->d_ptr->getBinds(client);
    for (auto bind : binds) {
        wayland_output->d_ptr->done(bind);
    }
}

int32_t output::Private::get_mode_flags(output_mode const& mode, output_state const& state)
{
    int32_t flags = 0;

    if (state.mode == mode) {
        flags |= WL_OUTPUT_MODE_CURRENT;
    }
    if (mode.preferred) {
        flags |= WL_OUTPUT_MODE_PREFERRED;
    }
    return flags;
}

int32_t output::Private::to_transform(output_transform transform)
{
    switch (transform) {
    case output_transform::normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case output_transform::rotated_90:
        return WL_OUTPUT_TRANSFORM_90;
    case output_transform::rotated_180:
        return WL_OUTPUT_TRANSFORM_180;
    case output_transform::rotated_270:
        return WL_OUTPUT_TRANSFORM_270;
    case output_transform::flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case output_transform::flipped_90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case output_transform::flipped_180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case output_transform::flipped_270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    }
    abort();
}

void output::Private::update_client_scale()
{
    auto logical_size = pending.geometry.size();
    auto mode_size = pending.mode.size;

    if (logical_size.width() <= 0 || logical_size.height() <= 0 || mode_size.width() <= 0
        || mode_size.height() <= 0) {
        pending.client_scale = 1;
        return;
    }

    auto width_ratio = mode_size.width() / logical_size.width();
    auto height_ratio = mode_size.height() / logical_size.height();

    pending.client_scale = std::ceil(std::max(width_ratio, height_ratio));
}

bool output_mode::operator==(output_mode const& mode) const
{
    return size == mode.size && refresh_rate == mode.refresh_rate && id == mode.id;
}

bool output_mode::operator!=(output_mode const& mode) const
{
    return !(*this == mode);
}

output::output(Display* display)
    : d_ptr(new Private(display, this))
{
}

output::~output() = default;

void output::done()
{
    d_ptr->done();
}

output_subpixel output::subpixel() const
{
    return d_ptr->pending.subpixel;
}

void output::set_subpixel(output_subpixel subpixel)
{
    d_ptr->pending.subpixel = subpixel;
}

output_transform output::transform() const
{
    return d_ptr->pending.transform;
}

output_metadata const& output::get_metadata() const
{
    return d_ptr->pending.meta;
}

void output::set_metadata(output_metadata const& data)
{
    d_ptr->pending.meta = data;
}

void output::set_connector_id(int id)
{
    d_ptr->connector_id = id;
}

void output::generate_description()
{
    auto& meta = d_ptr->pending.meta;
    std::string descr;
    if (!meta.make.empty()) {
        descr = meta.make;
    }
    if (!meta.model.empty()) {
        descr = (descr.empty() ? "" : descr + " ") + meta.model;
    }
    if (!meta.name.empty()) {
        if (descr.empty()) {
            descr = meta.name;
        } else {
            descr += " (" + meta.name + ")";
        }
    }
    meta.description = descr;
}

bool output::enabled() const
{
    return d_ptr->pending.enabled;
}

void output::set_enabled(bool enabled)
{
    d_ptr->pending.enabled = enabled;
}

int output::connector_id() const
{
    return d_ptr->connector_id;
}

std::vector<output_mode> output::modes() const
{
    return d_ptr->modes;
}

int output::mode_id() const
{
    return d_ptr->pending.mode.id;
}

QSize output::mode_size() const
{
    return d_ptr->pending.mode.size;
}

int output::refresh_rate() const
{
    return d_ptr->pending.mode.refresh_rate;
}

void output::add_mode(output_mode const& mode)
{
    d_ptr->pending.mode = mode;

    auto it = std::find(d_ptr->modes.begin(), d_ptr->modes.end(), mode);

    if (it != d_ptr->modes.end()) {
        d_ptr->modes.at(it - d_ptr->modes.begin()) = mode;
    } else {
        d_ptr->modes.push_back(mode);
    }
}

bool output::set_mode(int id)
{
    for (auto const& mode : d_ptr->modes) {
        if (mode.id == id) {
            d_ptr->pending.mode = mode;
            d_ptr->update_client_scale();
            return true;
        }
    }
    return false;
}

bool output::set_mode(output_mode const& mode)
{
    for (auto const& cmp : d_ptr->modes) {
        if (cmp == mode) {
            d_ptr->pending.mode = cmp;
            d_ptr->update_client_scale();
            return true;
        }
    }
    return false;
}

bool output::set_mode(QSize const& size, int refresh_rate)
{
    for (auto const& mode : d_ptr->modes) {
        if (mode.size == size && mode.refresh_rate == refresh_rate) {
            d_ptr->pending.mode = mode;
            d_ptr->update_client_scale();
            return true;
        }
    }
    return false;
}

QRectF output::geometry() const
{
    return d_ptr->pending.geometry;
}

void output::set_transform(output_transform transform)
{
    d_ptr->pending.transform = transform;
}

void output::set_geometry(QRectF const& geometry)
{
    d_ptr->pending.geometry = geometry;
    d_ptr->update_client_scale();
}

int output::client_scale() const
{
    return d_ptr->pending.client_scale;
}

void output::set_dpms_supported(bool supported)
{
    if (d_ptr->dpms.supported == supported) {
        return;
    }
    d_ptr->dpms.supported = supported;
    Q_EMIT dpms_supported_changed();
}

bool output::dpms_supported() const
{
    return d_ptr->dpms.supported;
}

void output::set_dpms_mode(output_dpms_mode mode)
{
    if (d_ptr->dpms.mode == mode) {
        return;
    }
    d_ptr->dpms.mode = mode;
    Q_EMIT dpms_mode_changed();
}

output_dpms_mode output::dpms_mode() const
{
    return d_ptr->dpms.mode;
}

OutputDeviceV1* output::output_device_v1() const
{
    return d_ptr->device.get();
}

WlOutput* output::wayland_output() const
{
    return d_ptr->wayland_output.get();
}

XdgOutput* output::xdg_output() const
{
    return d_ptr->xdg_output.get();
}

}
