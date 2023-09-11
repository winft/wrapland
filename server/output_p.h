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
#pragma once

#include "output.h"

#include <gsl/pointers>
#include <wayland-server.h>

namespace Wrapland::Server
{
class Client;
class WlOutput;
class wlr_output_head_v1;
class XdgOutput;

class Display;

wl_output_transform output_to_transform(output_transform transform);
output_transform transform_to_output(wl_output_transform transform);

struct output_data {
    output_metadata meta;
    output_state state;
};

class output::Private
{
public:
    Private(output_metadata metadata, output_manager& manager, output* q_ptr);
    ~Private();

    void update_client_scale();
    void done();

    /**
     * Called internally when for updates of objects synced with wl_output.
     */
    void done_wl(Client* client) const;

    static int32_t get_mode_flags(output_mode const& mode, output_state const& state);

    gsl::not_null<output_manager*> manager;

    int connector_id{0};
    std::vector<output_mode> modes;

    struct {
        bool supported = false;
        output_dpms_mode mode{output_dpms_mode::off};
    } dpms;

    output_data pending;
    output_data published;

    std::unique_ptr<WlOutput> wayland_output;
    std::unique_ptr<XdgOutput> xdg_output;
    std::unique_ptr<wlr_output_head_v1> wlr_head_v1;

    output* q_ptr;

private:
    int32_t toTransform() const;
    int32_t toSubpixel() const;
};

}
