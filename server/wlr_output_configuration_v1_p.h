/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "wayland/resource.h"
#include "wlr_output_configuration_head_v1.h"
#include "wlr_output_configuration_v1.h"

#include "wayland-wlr-output-management-v1-server-protocol.h"

namespace Wrapland::Server
{

class wlr_output_configuration_v1_res : public QObject
{
    Q_OBJECT
public:
    wlr_output_configuration_v1_res(Client* client,
                                    uint32_t version,
                                    uint32_t id,
                                    wlr_output_configuration_v1& front);

    ~wlr_output_configuration_v1_res() override;

    std::vector<wlr_output_configuration_head_v1*> enabled_heads() const;

    void send_succeeded() const;
    void send_failed() const;
    void send_cancelled() const;

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

class wlr_output_configuration_v1_res::Private
    : public Wayland::Resource<wlr_output_configuration_v1_res>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            wlr_output_configuration_v1& front,
            wlr_output_configuration_v1_res& q_ptr);
    ~Private() override;

    std::vector<wlr_output_configuration_head_v1*> enabled_heads;
    std::vector<wlr_output_head_v1_res*> disabled_heads;

    wlr_output_configuration_v1* front;
    bool is_cancelled{false};
    bool is_used{false};

private:
    bool check_head_enablement(wlr_output_head_v1_res* head);
    bool check_all_heads_configured();
    bool check_already_used();

    static void enable_head_callback(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t id,
                                     wl_resource* wlrHead);
    static void
    disable_head_callback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlrHead);
    static void apply_callback(wl_client* wlClient, wl_resource* wlResource);
    static void test_callback(wl_client* wlClient, wl_resource* wlResource);

    static struct zwlr_output_configuration_v1_interface const s_interface;
};

class wlr_output_configuration_v1::Private
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            wlr_output_manager_v1& manager,
            wlr_output_configuration_v1& q_ptr);

    wlr_output_manager_v1* manager;
    wlr_output_configuration_v1_res* res;
};

}
