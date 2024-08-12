/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "plasma_activation_feedback_p.h"

#include "client.h"
#include "display.h"
#include "seat_p.h"

namespace Wrapland::Server
{

struct org_kde_plasma_activation_feedback_interface const
    plasma_activation_feedback::Private::s_interface
    = {
        resourceDestroyCallback,
};

struct org_kde_plasma_activation_interface const plasma_activation::Private::s_interface
    = {destroyCallback};

plasma_activation_feedback::Private::Private(Display* display, plasma_activation_feedback* q_ptr)
    : plasma_activation_feedback_global(q_ptr,
                                        display,
                                        &org_kde_plasma_activation_feedback_interface,
                                        &s_interface)
    , q_ptr{q_ptr}
{
    create();
}

plasma_activation_feedback::plasma_activation_feedback(Display* display)
    : d_ptr(new Private(display, this))
{
}

plasma_activation_feedback::~plasma_activation_feedback()
{
    for (auto& activation : d_ptr->activations) {
        for (auto per_bind_activation : activation.second) {
            per_bind_activation->d_ptr->manager = nullptr;
        }
    }
}

void plasma_activation_feedback::Private::bindInit(plasma_activation_feedback_global::bind_t* bind)
{
    for (auto& activation : activations) {
        activation.second.push_back(create_activation(bind, activation.first));
    }
}

plasma_activation* plasma_activation_feedback::Private::create_activation(
    plasma_activation_feedback_global::bind_t* bind,
    std::string const& id)
{
    auto activation = new plasma_activation(bind->client->handle, bind->version, 0, id, q_ptr);
    bind->send<org_kde_plasma_activation_feedback_send_activation>(activation->d_ptr->resource);
    activation->d_ptr->send<org_kde_plasma_activation_send_app_id>(id.c_str());
    return activation;
}

void plasma_activation_feedback::app_id(std::string const& id)
{
    // TODO(romangg): Should we instead add one more for each call?
    if (d_ptr->activations.find(id) != d_ptr->activations.end()) {
        return;
    }

    std::vector<plasma_activation*> activations;

    for (auto bind : d_ptr->getBinds()) {
        activations.push_back(d_ptr->create_activation(bind, id));
    }

    d_ptr->activations.insert({id, activations});
}

void plasma_activation_feedback::finished(std::string const& id) const
{
    auto& activation = d_ptr->activations.at(id);
    for (auto per_bind_activation : activation) {
        per_bind_activation->finished();
    }
    d_ptr->activations.erase(id);
}

plasma_activation::Private::Private(Client* client,
                                    uint32_t version,
                                    uint32_t id,
                                    std::string app_id,
                                    plasma_activation_feedback* manager,
                                    plasma_activation* q_ptr)
    : Wayland::Resource<plasma_activation>(client,
                                           version,
                                           id,
                                           &org_kde_plasma_activation_interface,
                                           &s_interface,
                                           q_ptr)
    , app_id{std::move(app_id)}
    , manager{manager}
{
}

plasma_activation::~plasma_activation()
{
    if (d_ptr->manager && !d_ptr->app_id.empty()) {
        remove_one(d_ptr->manager->d_ptr->activations.at(d_ptr->app_id), this);
    }
}

plasma_activation::plasma_activation(Client* client,
                                     uint32_t version,
                                     uint32_t id,
                                     std::string const& app_id,
                                     plasma_activation_feedback* manager)
    : d_ptr(new Private(client, version, id, app_id, manager, this))
{
}

void plasma_activation::finished() const
{
    d_ptr->app_id = {};
    d_ptr->send<org_kde_plasma_activation_send_finished>();
}

}
