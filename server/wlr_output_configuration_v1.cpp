/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "wlr_output_configuration_v1_p.h"

#include "wlr_output_configuration_head_v1_p.h"
#include "wlr_output_head_v1_p.h"

#include "display.h"
#include "utils.h"

namespace Wrapland::Server
{

wlr_output_configuration_v1::Private::Private(Client* client,
                                              uint32_t version,
                                              uint32_t id,
                                              wlr_output_manager_v1& manager,
                                              wlr_output_configuration_v1& q_ptr)
    : manager{&manager}
    , res{new wlr_output_configuration_v1_res(client, version, id, q_ptr)}
{
}

wlr_output_configuration_v1::wlr_output_configuration_v1(Client* client,
                                                         uint32_t version,
                                                         uint32_t id,
                                                         wlr_output_manager_v1& manager)
    : d_ptr{std::make_unique<Private>(client, version, id, manager, *this)}
{
}

wlr_output_configuration_v1::~wlr_output_configuration_v1()
{
    if (d_ptr->manager) {
        remove_all(d_ptr->manager->d_ptr->configurations, this);
    }
    if (d_ptr->res) {
        d_ptr->res->d_ptr->front = nullptr;
    }
}

std::vector<wlr_output_configuration_head_v1*> wlr_output_configuration_v1::enabled_heads() const
{
    assert(d_ptr->res);
    return d_ptr->res->enabled_heads();
}

void wlr_output_configuration_v1::send_succeeded()
{
    if (d_ptr->res) {
        d_ptr->res->send_succeeded();
    }
    delete this;
}

void wlr_output_configuration_v1::send_failed()
{
    if (d_ptr->res) {
        d_ptr->res->send_failed();
    }
    delete this;
}

void wlr_output_configuration_v1::send_cancelled()
{
    assert(d_ptr->res);
    remove_all(d_ptr->manager->d_ptr->configurations, this);
    d_ptr->res->send_cancelled();
}

wlr_output_configuration_v1_res::Private::Private(Client* client,
                                                  uint32_t version,
                                                  uint32_t id,
                                                  wlr_output_configuration_v1& front,
                                                  wlr_output_configuration_v1_res& q_ptr)
    : Wayland::Resource<wlr_output_configuration_v1_res>(client,
                                                         version,
                                                         id,
                                                         &zwlr_output_configuration_v1_interface,
                                                         &s_interface,
                                                         &q_ptr)
    , front{&front}
{
}

wlr_output_configuration_v1_res::Private::~Private()
{
    if (front) {
        if (is_used) {
            front->d_ptr->res = nullptr;
        } else {
            delete front;
        }
    }
}

bool wlr_output_configuration_v1_res::Private::check_head_enablement(wlr_output_head_v1_res* head)
{
    if (contains(disabled_heads, head)) {
        postError(ZWLR_OUTPUT_CONFIGURATION_V1_ERROR_ALREADY_CONFIGURED_HEAD,
                  "head disabled before enabling");
        return false;
    }

    if (std::find_if(enabled_heads.begin(),
                     enabled_heads.end(),
                     [&](auto enabled) { return enabled->d_ptr->head == head; })
        != enabled_heads.end()) {
        postError(ZWLR_OUTPUT_CONFIGURATION_V1_ERROR_ALREADY_CONFIGURED_HEAD, "head enabled twice");
        return false;
    }

    return true;
}

bool wlr_output_configuration_v1_res::Private::check_all_heads_configured()
{
    for (auto head : front->d_ptr->manager->d_ptr->heads) {
        if (std::find_if(enabled_heads.begin(),
                         enabled_heads.end(),
                         [head](auto enabled) { return enabled->d_ptr->head->d_ptr->head == head; })
            != enabled_heads.end()) {
            continue;
        }
        if (std::find_if(disabled_heads.begin(),
                         disabled_heads.end(),
                         [head](auto disabled) { return disabled->d_ptr->head == head; })
            != disabled_heads.end()) {
            continue;
        }

        postError(ZWLR_OUTPUT_CONFIGURATION_V1_ERROR_UNCONFIGURED_HEAD, "head is unconfigured");
        return false;
    }

    return true;
}

bool wlr_output_configuration_v1_res::Private::check_already_used()
{
    if (is_used) {
        postError(ZWLR_OUTPUT_CONFIGURATION_V1_ERROR_ALREADY_USED, "config already used");
        return true;
    }

    return false;
}

void wlr_output_configuration_v1_res::Private::enable_head_callback(wl_client* /*wlClient*/,
                                                                    wl_resource* wlResource,
                                                                    uint32_t id,
                                                                    wl_resource* wlrHead)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto head = Wayland::Resource<wlr_output_head_v1_res>::get_handle(wlrHead);

    if (!priv->check_head_enablement(head) || priv->check_already_used()) {
        return;
    }

    priv->enabled_heads.push_back(
        new wlr_output_configuration_head_v1(priv->client->handle, priv->version, id, *head));
}

void wlr_output_configuration_v1_res::Private::disable_head_callback(wl_client* /*wlClient*/,
                                                                     wl_resource* wlResource,
                                                                     wl_resource* wlrHead)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto head = Wayland::Resource<wlr_output_head_v1_res>::get_handle(wlrHead);

    if (!priv->check_head_enablement(head) || priv->check_already_used()) {
        return;
    }

    priv->disabled_heads.push_back(head);
}

void wlr_output_configuration_v1_res::Private::apply_callback(wl_client* /*wlClient*/,
                                                              wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    if (priv->is_cancelled) {
        return;
    }
    if (!priv->check_all_heads_configured() || priv->check_already_used()) {
        return;
    }

    priv->is_used = true;

    assert(priv->front);
    assert(priv->front->d_ptr->manager);
    remove_all(priv->front->d_ptr->manager->d_ptr->configurations, priv->front);

    Q_EMIT priv->front->d_ptr->manager->apply_config(priv->front);
}

void wlr_output_configuration_v1_res::Private::test_callback(wl_client* /*wlClient*/,
                                                             wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    if (priv->is_cancelled) {
        return;
    }
    if (!priv->check_all_heads_configured() || priv->check_already_used()) {
        return;
    }

    priv->is_used = true;

    assert(priv->front);
    assert(priv->front->d_ptr->manager);
    remove_all(priv->front->d_ptr->manager->d_ptr->configurations, priv->front);

    Q_EMIT priv->front->d_ptr->manager->test_config(priv->front);
}

struct zwlr_output_configuration_v1_interface const
    wlr_output_configuration_v1_res::Private::s_interface
    = {
        enable_head_callback,
        disable_head_callback,
        apply_callback,
        test_callback,
        destroyCallback,
};

wlr_output_configuration_v1_res::wlr_output_configuration_v1_res(Client* client,
                                                                 uint32_t version,
                                                                 uint32_t id,
                                                                 wlr_output_configuration_v1& front)
    : d_ptr{new Private(client, version, id, front, *this)}
{
}

wlr_output_configuration_v1_res::~wlr_output_configuration_v1_res() = default;

std::vector<wlr_output_configuration_head_v1*>
wlr_output_configuration_v1_res::enabled_heads() const
{
    return d_ptr->enabled_heads;
}

void wlr_output_configuration_v1_res::send_succeeded() const
{
    assert(!d_ptr->is_cancelled);
    assert(d_ptr->is_used);
    d_ptr->send<zwlr_output_configuration_v1_send_succeeded>();
}

void wlr_output_configuration_v1_res::send_failed() const
{
    assert(!d_ptr->is_cancelled);
    assert(d_ptr->is_used);
    d_ptr->send<zwlr_output_configuration_v1_send_failed>();
}

void wlr_output_configuration_v1_res::send_cancelled() const
{
    assert(!d_ptr->is_cancelled);
    d_ptr->is_cancelled = true;
    d_ptr->send<zwlr_output_configuration_v1_send_cancelled>();
}

}
