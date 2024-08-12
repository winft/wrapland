/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "drm_lease_v1.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QObject>
#include <deque>

#include <wayland-drm-lease-v1-server-protocol.h>

namespace Wrapland::Server
{

class Display;

constexpr uint32_t drm_lease_device_v1Version = 1;
using drm_lease_device_v1_global = Wayland::Global<drm_lease_device_v1, drm_lease_device_v1Version>;

class drm_lease_device_v1::Private : public drm_lease_device_v1_global
{
public:
    Private(Display* display, drm_lease_device_v1* q_ptr);
    ~Private() override;

    void bindInit(drm_lease_device_v1_global::bind_t* bind) override;
    void prepareUnbind(drm_lease_device_v1_global::bind_t* bind) override;

    void add_connector(drm_lease_connector_v1* connector);
    void send_connector(drm_lease_device_v1_global::bind_t* bind,
                        drm_lease_connector_v1* connector);

    void add_lease(drm_lease_v1* lease, int leese_id);
    void remove_lease(int leese_id);

    std::deque<drm_lease_device_v1_global::bind_t*> waiting_binds;
    std::vector<drm_lease_device_v1_global::bind_t*> active_binds;
    std::vector<drm_lease_connector_v1*> connectors;

private:
    static void create_lease_request_callback(drm_lease_device_v1_global::bind_t* bind,
                                              uint32_t id);
    static void release_callback(drm_lease_device_v1_global::bind_t* bind);

    static const struct wp_drm_lease_device_v1_interface s_interface;

    drm_lease_device_v1* q_ptr;
};

class drm_lease_request_v1 : public QObject
{
    Q_OBJECT
public:
    drm_lease_request_v1(Client* client,
                         uint32_t version,
                         uint32_t id,
                         drm_lease_device_v1* device);

    std::vector<drm_lease_connector_v1*> connectors() const;
    Client* applicant() const;

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

class drm_lease_request_v1::Private : public Wayland::Resource<drm_lease_request_v1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            drm_lease_device_v1* device,
            drm_lease_request_v1* q_ptr);
    ~Private() override = default;

    drm_lease_device_v1* device;
    std::vector<drm_lease_connector_v1*> connectors;
    QMetaObject::Connection destroyConnection;

private:
    static void request_connector_callback(wl_client* wlClient,
                                           wl_resource* wlResource,
                                           wl_resource* wlConnector);
    static void submit_callback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);

    static const struct wp_drm_lease_request_v1_interface s_interface;
};

class drm_lease_connector_v1_res : public QObject
{
    Q_OBJECT
public:
    drm_lease_connector_v1_res(Wayland::Client* client,
                               uint32_t version,
                               uint32_t id,
                               drm_lease_connector_v1* connector);

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

class drm_lease_connector_v1_res::Private : public Wayland::Resource<drm_lease_connector_v1_res>
{
public:
    Private(Wayland::Client* client,
            uint32_t version,
            uint32_t id,
            drm_lease_connector_v1* connector,
            drm_lease_connector_v1_res* q_ptr);
    ~Private() override;

    drm_lease_connector_v1* connector;

private:
    static struct wp_drm_lease_connector_v1_interface const s_interface;
    drm_lease_connector_v1_res* q_ptr;
};

class drm_lease_connector_v1::Private : public QObject
{
    Q_OBJECT
public:
    Private(std::string name,
            std::string description,
            int id,
            drm_lease_device_v1* device,
            drm_lease_connector_v1* q_ptr);
    void add_resource(drm_lease_connector_v1_res* res);

    std::string name;
    std::string description;
    int connector_id;

    Server::output* output{nullptr};
    drm_lease_device_v1* device;
    std::vector<drm_lease_connector_v1_res*> resources;

    drm_lease_connector_v1* q_ptr;
};

class drm_lease_v1::Private : public Wayland::Resource<drm_lease_v1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            std::vector<drm_lease_connector_v1*> connectors,
            drm_lease_device_v1* device,
            drm_lease_v1* q_ptr);
    ~Private() override = default;

    int lessee_id{-1};
    std::vector<drm_lease_connector_v1*> connectors;
    drm_lease_device_v1* device;

private:
    static const struct wp_drm_lease_v1_interface s_interface;
};

}
