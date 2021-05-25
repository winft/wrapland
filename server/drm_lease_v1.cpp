/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "drm_lease_v1_p.h"

#include "client.h"
#include "display.h"
#include "output.h"
#include "utils.h"

#include <unistd.h>

namespace Wrapland::Server
{

const struct wp_drm_lease_device_v1_interface drm_lease_device_v1::Private::s_interface = {
    cb<create_lease_request_callback>,
    cb<release_callback>,
};

drm_lease_device_v1::Private::Private(Display* display, drm_lease_device_v1* q_ptr)
    : drm_lease_device_v1_global(q_ptr, display, &wp_drm_lease_device_v1_interface, &s_interface)
    , q_ptr{q_ptr}
{
    create();
}

drm_lease_device_v1::Private::~Private()
{
    for (auto& con : connectors) {
        con->d_ptr->device = nullptr;
    }
}

void drm_lease_device_v1::Private::bindInit(drm_lease_device_v1_bind* bind)
{
    waiting_binds.push_back(bind);
    Q_EMIT handle()->needs_new_client_fd();
}

void drm_lease_device_v1::Private::create_lease_request_callback(drm_lease_device_v1_bind* bind,
                                                                 uint32_t id)
{
    auto request = new drm_lease_request_v1(
        bind->client()->handle(), bind->version(), id, bind->global()->handle());
    if (!request->d_ptr->resource()) {
        bind->post_no_memory();
        delete request;
        return;
    }
}

void drm_lease_device_v1::Private::release_callback([[maybe_unused]] drm_lease_device_v1_bind* bind)
{
    auto priv = bind->global()->handle()->d_ptr.get();

    remove_one(priv->active_binds, bind);
    remove_one(priv->waiting_binds, bind);

    priv->send<wp_drm_lease_device_v1_send_released>(bind);
    delete bind;
}

void drm_lease_device_v1::Private::send_connector(drm_lease_device_v1_bind* bind,
                                                  drm_lease_connector_v1* connector)
{
    auto con_res = new drm_lease_connector_v1_res(bind->client(), bind->version(), 0, connector);
    send<wp_drm_lease_device_v1_send_connector>(bind, con_res->d_ptr->resource());
    connector->d_ptr->add_resource(con_res);
}

drm_lease_device_v1::drm_lease_device_v1(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

drm_lease_device_v1::~drm_lease_device_v1() = default;

drm_lease_connector_v1* drm_lease_device_v1::create_connector(Output* output)
{
    auto connector = new drm_lease_connector_v1(output, this);
    d_ptr->connectors.push_back(connector);
    for (auto bind : d_ptr->active_binds) {
        d_ptr->send_connector(bind, connector);
        d_ptr->send<wp_drm_lease_device_v1_send_done>(bind);
    }
    return connector;
}

void drm_lease_device_v1::update_fd(int fd)
{
    if (d_ptr->waiting_binds.empty()) {
        if (fd > 0) {
            close(fd);
        }
        return;
    }

    auto bind = d_ptr->waiting_binds.front();

    if (fd > 0) {
        d_ptr->send<wp_drm_lease_device_v1_send_drm_fd>(bind, fd);
    }

    for (auto& connector : d_ptr->connectors) {
        d_ptr->send_connector(bind, connector);
    }
    d_ptr->send<wp_drm_lease_device_v1_send_done>(bind);

    d_ptr->waiting_binds.pop_front();
    d_ptr->active_binds.push_back(bind);
}

drm_lease_connector_v1::Private::Private(Output* output,
                                         drm_lease_device_v1* device,
                                         drm_lease_connector_v1* q)
    : output{output}
    , device{device}
    , q_ptr{q}
{
}

void drm_lease_connector_v1::Private::add_resource(drm_lease_connector_v1_res* res)
{
    resources.push_back(res);
    res->d_ptr->send<wp_drm_lease_connector_v1_send_name>(output->name().c_str());
    res->d_ptr->send<wp_drm_lease_connector_v1_send_description>(output->description().c_str());
    res->d_ptr->send<wp_drm_lease_connector_v1_send_connector_id>(output->connector_id());
    res->d_ptr->send<wp_drm_lease_connector_v1_send_done>();
}

drm_lease_connector_v1::drm_lease_connector_v1(Output* output, drm_lease_device_v1* device)
    : QObject(nullptr)
    , d_ptr{new Private(output, device, this)}
{
}

drm_lease_connector_v1::~drm_lease_connector_v1()
{
    for (auto& res : d_ptr->resources) {
        res->d_ptr->connector = nullptr;
        res->d_ptr->send<wp_drm_lease_connector_v1_send_withdrawn>();
    }
    if (d_ptr->device) {
        remove_one(d_ptr->device->d_ptr->connectors, this);
    }
}

Output* drm_lease_connector_v1::output() const
{
    return d_ptr->output;
}

struct wp_drm_lease_connector_v1_interface const drm_lease_connector_v1_res::Private::s_interface
    = {
        destroyCallback,
};

drm_lease_connector_v1_res::Private::Private(Wayland::Client* client,
                                             uint32_t version,
                                             uint32_t id,
                                             drm_lease_connector_v1* connector,
                                             drm_lease_connector_v1_res* q_ptr)
    : Wayland::Resource<drm_lease_connector_v1_res>(client,
                                                    version,
                                                    id,
                                                    &wp_drm_lease_connector_v1_interface,
                                                    &s_interface,
                                                    q_ptr)
    , connector{connector}
    , q_ptr{q_ptr}
{
}

drm_lease_connector_v1_res::Private::~Private()
{
    if (connector) {
        remove_one(connector->d_ptr->resources, q_ptr);
    }
}

drm_lease_connector_v1_res::drm_lease_connector_v1_res(Wayland::Client* client,
                                                       uint32_t version,
                                                       uint32_t id,
                                                       drm_lease_connector_v1* connector)
    : d_ptr(new Private(client, version, id, connector, this))
{
}

const struct wp_drm_lease_request_v1_interface drm_lease_request_v1::Private::s_interface = {
    request_connector_callback,
    submit_callback,
};

drm_lease_request_v1::Private::Private(Client* client,
                                       uint32_t version,
                                       uint32_t id,
                                       drm_lease_device_v1* device,
                                       drm_lease_request_v1* qptr)
    : Wayland::Resource<drm_lease_request_v1>(client,
                                              version,
                                              id,
                                              &wp_drm_lease_request_v1_interface,
                                              &s_interface,
                                              qptr)
    , device{device}
{
}

void drm_lease_request_v1::Private::request_connector_callback(wl_client* /*wlClient*/,
                                                               wl_resource* wlResource,
                                                               wl_resource* wlConnector)
{
    auto priv = handle(wlResource)->d_ptr;
    auto connector
        = Wayland::Resource<drm_lease_connector_v1_res>::handle(wlConnector)->d_ptr->connector;

    if (!priv->device || !connector->d_ptr->device) {
        // Global has been deleted server-side, fail silently.
        return;
    }

    if (connector->d_ptr->device != priv->device) {
        priv->postError(WP_DRM_LEASE_REQUEST_V1_ERROR_WRONG_DEVICE,
                        "requested a connector from a different lease device");
        return;
    }
    if (std::find(priv->connectors.cbegin(), priv->connectors.cend(), connector)
        != priv->connectors.end()) {
        priv->postError(WP_DRM_LEASE_REQUEST_V1_ERROR_DUPLICATE_CONNECTOR,
                        "requested a connector twice");
        return;
    }
    priv->connectors.push_back(connector);
}

void drm_lease_request_v1::Private::submit_callback(wl_client* /*wlClient*/,
                                                    wl_resource* wlResource,
                                                    uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr;

    if (priv->connectors.empty()) {
        priv->postError(WP_DRM_LEASE_REQUEST_V1_ERROR_EMPTY_LEASE,
                        "requested a lease without requesting a connector");
        return;
    }

    // Create lease object.
    auto lease = new drm_lease_v1(
        priv->client()->handle(), priv->version(), id, std::move(priv->connectors), priv->device);

    // Delete the request.
    auto device = priv->device;
    priv->serverSideDestroy();
    delete priv->handle();
    delete priv;

    if (device) {
        Q_EMIT device->leased(lease);
    } else {
        lease->finish();
    }
}

drm_lease_request_v1::drm_lease_request_v1(Client* client,
                                           uint32_t version,
                                           uint32_t id,
                                           drm_lease_device_v1* device)
    : d_ptr(new Private(client, version, id, device, this))
{
    d_ptr->destroyConnection = connect(
        device, &drm_lease_device_v1::destroyed, this, [this] { d_ptr->device = nullptr; });
    connect(client, &Client::disconnected, this, [this] { disconnect(d_ptr->destroyConnection); });
}

std::vector<drm_lease_connector_v1*> drm_lease_request_v1::connectors() const
{
    return d_ptr->connectors;
}

struct wp_drm_lease_v1_interface const drm_lease_v1::Private::s_interface = {
    destroyCallback,
};

drm_lease_v1::Private::Private(Client* client,
                               uint32_t version,
                               uint32_t id,
                               std::vector<drm_lease_connector_v1*> connectors,
                               drm_lease_device_v1* device,
                               drm_lease_v1* q)
    : Wayland::Resource<drm_lease_v1>(client,
                                      version,
                                      id,
                                      &wp_drm_lease_v1_interface,
                                      &s_interface,
                                      q)
    , connectors{std::move(connectors)}
    , device{device}
{
}

drm_lease_v1::drm_lease_v1(Client* client,
                           uint32_t version,
                           uint32_t id,
                           std::vector<Wrapland::Server::drm_lease_connector_v1*>&& connectors,
                           drm_lease_device_v1* device)
    : d_ptr(new Private(client, version, id, std::move(connectors), device, this))
{
}

Client* drm_lease_v1::lessee() const
{
    return d_ptr->client()->handle();
}

std::vector<drm_lease_connector_v1*> drm_lease_v1::connectors() const
{
    return d_ptr->connectors;
}

int drm_lease_v1::lessee_id() const
{
    return d_ptr->lessee_id;
}

void drm_lease_v1::grant(int fd)
{
    d_ptr->send<wp_drm_lease_v1_send_lease_fd>(fd);
}

void drm_lease_v1::finish()
{
    d_ptr->send<wp_drm_lease_v1_send_finished>();
}
}
