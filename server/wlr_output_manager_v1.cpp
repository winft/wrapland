/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "wlr_output_manager_v1_p.h"

#include "display.h"
#include "utils.h"
#include "wayland/global.h"
#include "wlr_output_configuration_v1_p.h"
#include "wlr_output_head_v1_p.h"

namespace Wrapland::Server
{

wlr_output_manager_v1::Private::Private(wlr_output_manager_v1* q_ptr, Display* display)
    : wlr_output_manager_v1_global(q_ptr, display, &zwlr_output_manager_v1_interface, &s_interface)
{
    create();
}

wlr_output_manager_v1::Private::~Private()
{
    std::for_each(configurations.cbegin(), configurations.cend(), [](auto config) {
        config->d_ptr->manager = nullptr;
    });

    for (auto bind : getBinds()) {
        if (!is_finished(bind)) {
            bind->send<zwlr_output_manager_v1_send_finished>();
        }
    }
}

void wlr_output_manager_v1::Private::bindInit(wlr_output_manager_v1_global::bind_t* bind)
{
    for (auto&& head : heads) {
        head->add_bind(*bind);
    }
    bind->send<zwlr_output_manager_v1_send_done>(serial);
}

void wlr_output_manager_v1::Private::prepareUnbind(wlr_output_manager_v1_global::bind_t* bind)
{
    remove_all(finished_binds, bind);
}

void wlr_output_manager_v1::Private::add_head(wlr_output_head_v1& head)
{
    changed = true;
    heads.push_back(&head);

    for (auto bind : getBinds()) {
        head.add_bind(*bind);
    }
}

bool wlr_output_manager_v1::Private::is_finished(wlr_output_manager_v1_global::bind_t* bind) const
{
    return contains(finished_binds, bind);
}

void wlr_output_manager_v1::Private::create_configuration_callback(
    wlr_output_manager_v1_global::bind_t* bind,
    uint32_t id,
    uint32_t serial)
{
    auto priv = bind->global()->handle->d_ptr.get();
    if (priv->is_finished(bind)) {
        return;
    }

    auto config
        = new wlr_output_configuration_v1(bind->client->handle, bind->version, id, *priv->handle);

    if (priv->serial != serial) {
        config->send_cancelled();
        return;
    }

    priv->configurations.push_back(config);
}

void wlr_output_manager_v1::Private::stop_callback(wlr_output_manager_v1_global::bind_t* bind)
{
    auto priv = bind->global()->handle->d_ptr.get();
    if (priv->is_finished(bind)) {
        return;
    }

    priv->finished_binds.push_back(bind);
    bind->send<zwlr_output_manager_v1_send_finished>();
}

struct zwlr_output_manager_v1_interface const wlr_output_manager_v1::Private::s_interface = {
    cb<create_configuration_callback>,
    cb<stop_callback>,
};

wlr_output_manager_v1::wlr_output_manager_v1(Display* display)
    : d_ptr{std::make_unique<Private>(this, display)}
{
}

wlr_output_manager_v1::~wlr_output_manager_v1() = default;

void wlr_output_manager_v1::done()
{
    if (!d_ptr->changed) {
        return;
    }

    d_ptr->changed = false;
    ++d_ptr->serial;

    for (auto&& config : d_ptr->configurations) {
        config->send_cancelled();
    }

    for (auto&& bind : d_ptr->getBinds()) {
        if (!d_ptr->is_finished(bind)) {
            bind->send<zwlr_output_manager_v1_send_done>(d_ptr->serial);
        }
    }
}

}
