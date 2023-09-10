/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "wlr_output_configuration_head_v1_p.h"

#include "display.h"
#include "utils.h"
#include "wayland/global.h"
#include "wayland/resource.h"
#include "wlr_output_configuration_v1.h"
#include "wlr_output_head_v1_p.h"
#include "wlr_output_mode_v1_p.h"

namespace Wrapland::Server
{

struct zwlr_output_configuration_head_v1_interface const
    wlr_output_configuration_head_v1::Private::s_interface
    = {
        set_mode_callback,
        set_custom_mode_callback,
        set_position_callback,
        set_transform_callback,
        set_scale_callback,
        set_adaptive_sync_callback,
};

bool is_portrait_transform(output_transform tr)
{
    using ot = output_transform;
    return tr == ot::rotated_90 || tr == ot::rotated_270 || tr == ot::flipped_90
        || tr == ot::flipped_270;
}

QSize estimate_logical_size(output_state const& state, double scale)
{
    assert(scale > 0);
    auto size = state.mode.size / scale;
    if (is_portrait_transform(state.transform)) {
        size.transpose();
    }
    return size;
}

wlr_output_configuration_head_v1::Private::Private(Client* client,
                                                   uint32_t version,
                                                   uint32_t id,
                                                   wlr_output_head_v1_res& head,
                                                   wlr_output_configuration_head_v1& q_ptr)
    : Wayland::Resource<wlr_output_configuration_head_v1>(
        client,
        version,
        id,
        &zwlr_output_configuration_head_v1_interface,
        &s_interface,
        &q_ptr)
    , state{head.d_ptr->head->output.d_ptr->published.state}
    , scale{estimate_scale(state)}
    , head{&head}
{
}

void wlr_output_configuration_head_v1::Private::set_mode_callback(wl_client* /*wlClient*/,
                                                                  wl_resource* wlResource,
                                                                  wl_resource* wlrMode)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto mode = Wayland::Resource<wlr_output_mode_v1>::get_handle(wlrMode);

    auto const& modes = priv->head->d_ptr->modes;
    auto mode_it = std::find(modes.cbegin(), modes.cend(), mode);
    if (mode_it == modes.cend()) {
        priv->postError(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_MODE,
                        "mode not found in head");
        return;
    }

    priv->state.mode = (*mode_it)->d_ptr->mode;
    priv->state.geometry.setSize(estimate_logical_size(priv->state, priv->scale));
}

void wlr_output_configuration_head_v1::Private::set_custom_mode_callback(wl_client* /*wlClient*/,
                                                                         wl_resource* wlResource,
                                                                         int32_t wlWidth,
                                                                         int32_t wlHeight,
                                                                         int32_t wlRefresh)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->state.mode = {.size = {wlWidth, wlHeight}, .refresh_rate = wlRefresh};
}

void wlr_output_configuration_head_v1::Private::set_position_callback(wl_client* /*wlClient*/,
                                                                      wl_resource* wlResource,
                                                                      int32_t pos_x,
                                                                      int32_t pos_y)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->state.geometry.moveTopLeft(QPoint{pos_x, pos_y});
}

void wlr_output_configuration_head_v1::Private::set_transform_callback(wl_client* /*wlClient*/,
                                                                       wl_resource* wlResource,
                                                                       int32_t wlTransform)
{
    auto priv = get_handle(wlResource)->d_ptr;
    if (wlTransform < WL_OUTPUT_TRANSFORM_NORMAL || wlTransform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
        priv->postError(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_TRANSFORM,
                        "transform enum out of range");
        return;
    }
    priv->state.transform = transform_to_output(static_cast<wl_output_transform>(wlTransform));
    priv->state.geometry.setSize(estimate_logical_size(priv->state, priv->scale));
}

void wlr_output_configuration_head_v1::Private::set_scale_callback(wl_client* /*wlClient*/,
                                                                   wl_resource* wlResource,
                                                                   wl_fixed_t wlScale)
{
    auto priv = get_handle(wlResource)->d_ptr;
    if (wlScale <= 0) {
        priv->postError(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_SCALE,
                        "scale out of range");
        return;
    }

    priv->scale = wl_fixed_to_double(wlScale);
    priv->state.geometry.setSize(estimate_logical_size(priv->state, priv->scale));
}

void wlr_output_configuration_head_v1::Private::set_adaptive_sync_callback(wl_client* /*wlClient*/,
                                                                           wl_resource* wlResource,
                                                                           uint32_t wlState)
{
    auto priv = get_handle(wlResource)->d_ptr;
    if (wlState != ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED
        && wlState != ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_ENABLED) {
        priv->postError(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_ADAPTIVE_SYNC_STATE,
                        "adaptive sync state out of range");
        return;
    }

    priv->state.adaptive_sync = wlState == ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_ENABLED;
}

wlr_output_configuration_head_v1::wlr_output_configuration_head_v1(Client* client,
                                                                   uint32_t version,
                                                                   uint32_t id,
                                                                   wlr_output_head_v1_res& head)
    : d_ptr{new Private(client, version, id, head, *this)}
{
}

wlr_output_configuration_head_v1::~wlr_output_configuration_head_v1() = default;

output& wlr_output_configuration_head_v1::get_output() const
{
    return d_ptr->head->d_ptr->head->output;
}

output_state const& wlr_output_configuration_head_v1::get_state() const
{
    return d_ptr->state;
}

}
