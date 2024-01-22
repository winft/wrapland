/*
    SPDX-FileCopyrightText: 2024 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "security_context_v1_p.h"

#include "display.h"

namespace Wrapland::Server
{

struct wp_security_context_manager_v1_interface const
    security_context_manager_v1::Private::s_interface
    = {
        resourceDestroyCallback,
        cb<create_listener_callback>,
};

security_context_manager_v1::Private::Private(Display* display, security_context_manager_v1* q_ptr)
    : security_context_manager_v1_global(q_ptr,
                                         display,
                                         &wp_security_context_manager_v1_interface,
                                         &s_interface)
{
    create();
}

security_context_manager_v1::Private::~Private()
{
    for (auto& [key, inviter] : inviters) {
        inviter->finish = {};
    }
}

void security_context_manager_v1::Private::add_inviter(int listen_fd,
                                                       int close_fd,
                                                       std::string const& app_id)
{
    auto index = ++inviter_index;
    auto inviter = std::make_unique<security_context_inviter>(
        listen_fd, close_fd, app_id, *handle->d_ptr->display()->handle, [this, index] {
            inviters.erase(index);
        });
    if (inviter->is_finished) {
        return;
    }

    handle->d_ptr->inviters.insert({index, std::move(inviter)});
}

void security_context_manager_v1::Private::create_listener_callback(
    security_context_manager_v1_bind* bind,
    uint32_t id,
    int listen_fd,
    int close_fd)
{
    if (!bind->client->security_context_app_id().empty()) {
        bind->post_error(WP_SECURITY_CONTEXT_MANAGER_V1_ERROR_NESTED,
                         "Client already with security context");
        return;
    }

    auto handle = bind->global()->handle;
    auto ctx = new security_context_v1(bind->client->handle, bind->version, id);
    QObject::connect(ctx,
                     &security_context_v1::committed,
                     handle,
                     [handle, listen_fd, close_fd](auto const& app_id) {
                         handle->d_ptr->add_inviter(listen_fd, close_fd, app_id);
                     });
}

security_context_manager_v1::security_context_manager_v1(Display* display)
    : d_ptr(new Private(display, this))
{
}

security_context_manager_v1::~security_context_manager_v1() = default;

const struct wp_security_context_v1_interface security_context_v1::Private::s_interface = {
    destroyCallback,
    set_sandbox_engine_callback,
    set_app_id_callback,
    set_instance_id_callback,
    commit_callback,
};

security_context_v1::Private::Private(Client* client,
                                      uint32_t version,
                                      uint32_t id,
                                      security_context_v1* q_ptr)
    : Wayland::Resource<security_context_v1>(client,
                                             version,
                                             id,
                                             &wp_security_context_v1_interface,
                                             &s_interface,
                                             q_ptr)
    , q_ptr{q_ptr}
{
}

namespace
{

bool check_committed_error(security_context_v1::Private& priv)
{
    if (priv.received.commit) {
        priv.postError(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Already committed");
        return false;
    }
    return true;
}

}

void security_context_v1::Private::set_sandbox_engine_callback(wl_client* /*wlClient*/,
                                                               wl_resource* wlResource,
                                                               char const* engine)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!check_committed_error(*priv)) {
        return;
    }
    if (priv->received.sandbox_engine) {
        priv->postError(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Already set sandbox engine");
        return;
    }

    priv->sandbox_engine = engine;
    priv->received.sandbox_engine = true;
}

void security_context_v1::Private::set_app_id_callback(wl_client* /*wlClient*/,
                                                       wl_resource* wlResource,
                                                       char const* id)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!check_committed_error(*priv)) {
        return;
    }
    if (priv->received.app_id) {
        priv->postError(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Already set app id");
        return;
    }

    priv->app_id = id;
    priv->received.app_id = true;
}

void security_context_v1::Private::set_instance_id_callback(wl_client* /*wlClient*/,
                                                            wl_resource* wlResource,
                                                            char const* id)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!check_committed_error(*priv)) {
        return;
    }
    if (priv->received.instance_id) {
        priv->postError(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Already set instance id");
        return;
    }

    priv->instance_id = id;
    priv->received.instance_id = true;
}

void security_context_v1::Private::commit_callback(wl_client* /*wlClient*/, wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (!check_committed_error(*priv)) {
        return;
    }

    priv->received.commit = true;

    if (priv->sandbox_engine.empty()) {
        priv->postError(WP_SECURITY_CONTEXT_V1_ERROR_INVALID_METADATA,
                        "Sandbox engine name cannot be empty");
        return;
    }

    Q_EMIT priv->q_ptr->committed(priv->app_id);
}

security_context_v1::security_context_v1(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}

}
