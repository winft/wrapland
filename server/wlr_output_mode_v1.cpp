/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "wlr_output_mode_v1_p.h"

#include "display.h"
#include "utils.h"
#include "wayland/global.h"

namespace Wrapland::Server
{

wlr_output_mode_v1::Private::Private(Client* client,
                                     uint32_t version,
                                     output_mode const& mode,
                                     wlr_output_mode_v1& q_ptr)
    : Wayland::Resource<wlr_output_mode_v1>(client,
                                            version,
                                            0,
                                            &zwlr_output_mode_v1_interface,
                                            &s_interface,
                                            &q_ptr)
    , mode{mode}
{
}

void wlr_output_mode_v1::Private::send_data()
{
    send<zwlr_output_mode_v1_send_size>(mode.size.width(), mode.size.height());
    send<zwlr_output_mode_v1_send_refresh>(mode.refresh_rate);
    if (mode.preferred) {
        send<zwlr_output_mode_v1_send_preferred>();
    }
}

struct zwlr_output_mode_v1_interface const wlr_output_mode_v1::Private::s_interface = {
    destroyCallback,
};

wlr_output_mode_v1::wlr_output_mode_v1(Client* client, uint32_t version, output_mode const& mode)
    : d_ptr{new Private(client, version, mode, *this)}
{
}

}
