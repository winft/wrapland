/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "primary_selection.h"

#include "seat.h"
#include "selection_device_manager_p.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-primary-selection-unstable-v1-server-protocol.h>

namespace Wrapland::Server
{
class data_control_source_v1_res;

constexpr uint32_t primary_selection_device_manager_version = 1;
using primary_selection_device_manager_global
    = Wayland::Global<primary_selection_device_manager, primary_selection_device_manager_version>;

class primary_selection_device_manager::Private
    : public device_manager<primary_selection_device_manager_global>
{
public:
    Private(Display* display, primary_selection_device_manager* q);
    ~Private() override;

private:
    static const struct zwp_primary_selection_device_manager_v1_interface s_interface;
};

class primary_selection_source_res;

class primary_selection_device::Private : public Wayland::Resource<primary_selection_device>
{
public:
    using source_res_t = Wrapland::Server::primary_selection_source_res;

    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Seat* seat,
            primary_selection_device* qptr);
    ~Private() override;

    Seat* m_seat;

    primary_selection_source* selection = nullptr;
    QMetaObject::Connection selectionDestroyedConnection;

    primary_selection_offer* sendDataOffer(primary_selection_source* source);

private:
    static void set_selection_callback(wl_client* wlClient,
                                       wl_resource* wlResource,
                                       wl_resource* wlSource,
                                       uint32_t id);
    static const struct zwp_primary_selection_device_v1_interface s_interface;
};

class primary_selection_offer::Private : public Wayland::Resource<primary_selection_offer>
{
public:
    Private(Client* client,
            uint32_t version,
            primary_selection_source* source,
            primary_selection_offer* qptr);
    ~Private() override;

    primary_selection_source* source;

private:
    static void receive_callback(wl_client* wlClient,
                                 wl_resource* wlResource,
                                 char const* mimeType,
                                 int32_t fd);
    static const struct zwp_primary_selection_offer_v1_interface s_interface;
};

class primary_selection_source::Private
{
public:
    explicit Private(primary_selection_source* q);

    std::vector<std::string> mimeTypes;

    std::variant<primary_selection_source_res*, data_control_source_v1_res*> res;
    primary_selection_source* q_ptr;
};

class primary_selection_source_res_impl : public Wayland::Resource<primary_selection_source_res>
{
public:
    primary_selection_source_res_impl(Client* client,
                                      uint32_t version,
                                      uint32_t id,
                                      primary_selection_source_res* q_ptr);

    primary_selection_source_res* q_ptr;

private:
    static void offer_callback(wl_client* wlClient, wl_resource* wlResource, char const* mimeType);

    static struct zwp_primary_selection_source_v1_interface const s_interface;
};

class primary_selection_source_res : public QObject
{
    Q_OBJECT
public:
    primary_selection_source_res(Client* client, uint32_t version, uint32_t id);

    void cancel() const;
    void request_data(std::string const& mimeType, qint32 fd) const;

    primary_selection_source* src() const;
    primary_selection_source::Private* src_priv() const;

    std::unique_ptr<primary_selection_source> pub_src;
    primary_selection_source_res_impl* impl;

Q_SIGNALS:
    void resourceDestroyed();
};

}
