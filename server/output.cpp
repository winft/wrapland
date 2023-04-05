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

#include "display.h"
#include "output_manager.h"
#include "utils.h"
#include "wl_output_p.h"
#include "wlr_output_head_v1_p.h"
#include "xdg_output_p.h"

#include "wayland/client.h"
#include "wayland/display.h"

#include <QRectF>
#include <cassert>
#include <functional>

namespace Wrapland::Server
{

wl_output_transform output_to_transform(output_transform transform)
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

output_transform transform_to_output(wl_output_transform transform)
{
    switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
        return output_transform::normal;
    case WL_OUTPUT_TRANSFORM_90:
        return output_transform::rotated_90;
    case WL_OUTPUT_TRANSFORM_180:
        return output_transform::rotated_180;
    case WL_OUTPUT_TRANSFORM_270:
        return output_transform::rotated_270;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
        return output_transform::flipped;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        return output_transform::flipped_90;
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        return output_transform::flipped_180;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        return output_transform::flipped_270;
    }
    abort();
}

output::Private::Private(output_metadata metadata, output_manager& manager, output* q_ptr)
    : manager{manager}
    , q_ptr{q_ptr}
{
    if (metadata.description.empty()) {
        metadata.description = output_generate_description(metadata);
    }
    pending.meta = std::move(metadata);
    published.meta = pending.meta;

    manager.outputs.push_back(q_ptr);
    QObject::connect(&manager.display, &Display::destroyed, q_ptr, [this] {
        xdg_output.reset();
        wayland_output.reset();
    });
}

output::Private::~Private()
{
    remove_all(manager.outputs, q_ptr);
}

void output::Private::done()
{
    if (published.state.enabled != pending.state.enabled) {
        if (pending.state.enabled) {
            wayland_output.reset(new WlOutput(q_ptr, &manager.display));
            if (manager.xdg_manager) {
                xdg_output.reset(new XdgOutput(q_ptr, &manager.display));
            }
        } else {
            wayland_output.reset();
            xdg_output.reset();
        }
    }
    if (pending.state.enabled) {
        auto wayland_change = wayland_output->d_ptr->broadcast();
        auto xdg_change = xdg_output ? xdg_output->d_ptr->broadcast() : false;
        if (wayland_change || xdg_change) {
            wayland_output->d_ptr->done();
            if (xdg_output) {
                xdg_output->d_ptr->done();
            }
        }
    }
    if (auto& wlr_man = manager.wlr_manager_v1) {
        if (wlr_head_v1) {
            wlr_head_v1->broadcast();
        } else {
            wlr_head_v1 = std::make_unique<wlr_output_head_v1>(*q_ptr, *wlr_man);
        }
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

void output::Private::update_client_scale()
{
    auto logical_size = pending.state.geometry.size();
    auto mode_size = pending.state.mode.size;

    if (logical_size.width() <= 0 || logical_size.height() <= 0 || mode_size.width() <= 0
        || mode_size.height() <= 0) {
        pending.state.client_scale = 1;
        return;
    }

    auto width_ratio = mode_size.width() / logical_size.width();
    auto height_ratio = mode_size.height() / logical_size.height();

    pending.state.client_scale = std::ceil(std::max(width_ratio, height_ratio));
}

bool output_mode::operator==(output_mode const& mode) const
{
    return size == mode.size && refresh_rate == mode.refresh_rate && id == mode.id;
}

bool output_mode::operator!=(output_mode const& mode) const
{
    return !(*this == mode);
}

output::output(output_manager& manager)
    : output(output_metadata(), manager)
{
}

output::output(output_metadata metadata, output_manager& manager)
    : d_ptr(new Private(std::move(metadata), manager, this))
{
}

output::~output() = default;

void output::done()
{
    d_ptr->done();
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

std::string output_generate_description(output_metadata const& data)
{
    std::string descr;

    if (!data.make.empty()) {
        descr = data.make;
    }
    if (!data.model.empty()) {
        descr = (descr.empty() ? "" : descr + " ") + data.model;
    }
    if (!data.name.empty()) {
        if (descr.empty()) {
            descr = data.name;
        } else {
            descr += " (" + data.name + ")";
        }
    }

    return descr;
}

output_state const& output::get_state() const
{
    return d_ptr->pending.state;
}

void output::set_state(output_state const& data)
{
    if (!contains(d_ptr->modes, data.mode)) {
        // TODO(romangg): Allow custom modes?
        return;
    }
    d_ptr->pending.state = data;
    d_ptr->update_client_scale();
}

int output::connector_id() const
{
    return d_ptr->connector_id;
}

std::vector<output_mode> output::modes() const
{
    return d_ptr->modes;
}

void output::add_mode(output_mode const& mode)
{
    d_ptr->pending.state.mode = mode;

    auto it = std::find(d_ptr->modes.begin(), d_ptr->modes.end(), mode);

    if (it != d_ptr->modes.end()) {
        d_ptr->modes.at(it - d_ptr->modes.begin()) = mode;
    } else {
        d_ptr->modes.push_back(mode);
    }
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

WlOutput* output::wayland_output() const
{
    return d_ptr->wayland_output.get();
}

XdgOutput* output::xdg_output() const
{
    return d_ptr->xdg_output.get();
}

}
