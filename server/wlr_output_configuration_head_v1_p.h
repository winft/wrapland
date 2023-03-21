/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "wlr_output_configuration_head_v1.h"

#include "wayland/resource.h"

#include "wayland-wlr-output-management-v1-server-protocol.h"

namespace Wrapland::Server
{

class wlr_output_configuration_v1;

class wlr_output_configuration_head_v1::Private
    : public Wayland::Resource<wlr_output_configuration_head_v1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            wlr_output_configuration_v1& configuration,
            wlr_output_head_v1_res& head,
            wlr_output_configuration_head_v1& q_ptr);

    output_state state;
    double scale;

    wlr_output_configuration_v1& configuration;
    wlr_output_head_v1_res* head;

private:
    static void
    set_mode_callback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlrMode);
    static void set_custom_mode_callback(wl_client* wlClient,
                                         wl_resource* wlResource,
                                         int32_t wlWidth,
                                         int32_t wlHeight,
                                         int32_t wlRefresh);
    static void set_position_callback(wl_client* wlClient,
                                      wl_resource* wlResource,
                                      int32_t pos_x,
                                      int32_t pos_y);
    static void
    set_transform_callback(wl_client* wlClient, wl_resource* wlResource, int32_t wlTransform);
    static void
    set_scale_callback(wl_client* wlClient, wl_resource* wlResource, wl_fixed_t wlScale);

    static const struct zwlr_output_configuration_head_v1_interface s_interface;
};

}
