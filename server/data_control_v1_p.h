/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "data_control_v1.h"

#include "data_source_p.h"
#include "primary_selection_p.h"

#include "selection_device_manager_p.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-wlr-data-control-v1-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t data_control_manager_v1_version = 2;
using data_control_manager_v1_global
    = Wayland::Global<data_control_manager_v1, data_control_manager_v1_version>;

class data_control_manager_v1::Private : public device_manager<data_control_manager_v1_global>
{
public:
    Private(data_control_manager_v1* q_ptr, Display* display);

private:
    static struct zwlr_data_control_manager_v1_interface const s_interface;
};

class data_control_offer_v1_res;

struct selection_source_holder {
    data_control_source_v1_res* source{nullptr};
    QMetaObject::Connection destroyed_notifier;
};

class data_control_device_v1::impl : public Wayland::Resource<data_control_device_v1>
{
public:
    using source_res_t = Wrapland::Server::data_control_source_v1_res;

    impl(Client* client, uint32_t version, uint32_t id, Seat* seat, data_control_device_v1* q_ptr);

    template<typename Source>
    Source* get_selection(selection_source_holder const& holder);

    Seat* m_seat;

    selection_source_holder selection;
    selection_source_holder primary_selection;

    data_control_offer_v1_res* send_data_offer(data_source* source);
    data_control_offer_v1_res* send_data_offer(primary_selection_source* source);

private:
    static void
    set_selection_callback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlSource);
    static void set_primary_selection_callback(wl_client* wlClient,
                                               wl_resource* wlResource,
                                               wl_resource* wlSource);

    template<typename Source>
    data_control_offer_v1_res* send_data_offer_impl(Source source);

    template<typename Source, typename Devices>
    static void set_selection_impl(Devices& seat_devices,
                                   selection_source_holder& selection_holder,
                                   data_control_device_v1* handle,
                                   wl_resource* wlSource);

    static struct zwlr_data_control_device_v1_interface const s_interface;
};

class data_control_source_v1_res : public QObject
{
    Q_OBJECT
public:
    data_control_source_v1_res(Client* client, uint32_t version, uint32_t id);
    ~data_control_source_v1_res() override;

    void request_data(std::string const& mime_type, qint32 fd) const;
    void cancel() const;

    data_control_source_v1_res* src();

    std::vector<std::string> early_mime_types;
    std::variant<std::monostate,
                 std::unique_ptr<data_source>,
                 std::unique_ptr<primary_selection_source>>
        data_src;

    class res_impl;
    res_impl* impl;

Q_SIGNALS:
    void resourceDestroyed();
};

class data_control_source_v1_res::res_impl : public Wayland::Resource<data_control_source_v1_res>
{
public:
    res_impl(Client* client, uint32_t version, uint32_t id, data_control_source_v1_res* q_ptr);

    data_control_source_v1_res* q_ptr;

private:
    static void offer_callback(wl_client* wlClient, wl_resource* wlResource, char const* mimeType);

    static struct zwlr_data_control_source_v1_interface const s_interface;
};

class data_control_offer_v1_res_impl : public Wayland::Resource<data_control_offer_v1_res>
{
public:
    data_control_offer_v1_res_impl(Client* client,
                                   uint32_t version,
                                   data_control_offer_v1_res* q_ptr);

    std::variant<std::monostate, data_source*, primary_selection_source*> src;

private:
    static void receive_callback(wl_client* wlClient,
                                 wl_resource* wlResource,
                                 char const* mime_type,
                                 int32_t fd);
    static const struct zwlr_data_control_offer_v1_interface s_interface;
};

class WRAPLANDSERVER_EXPORT data_control_offer_v1_res : public QObject
{
    Q_OBJECT
public:
    data_control_offer_v1_res(Client* client, uint32_t version, data_source* source);
    data_control_offer_v1_res(Client* client, uint32_t version, primary_selection_source* source);

    void send_offers() const;

    data_control_offer_v1_res_impl* impl;

Q_SIGNALS:
    void resourceDestroyed();
};

}
