/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "output.h"
#include "wayland/resource.h"
#include "wlr_output_manager_v1_p.h"

#include "wayland-wlr-output-management-v1-server-protocol.h"

#include <QObject>
#include <memory>

namespace Wrapland::Server
{

class Client;
class wlr_output_head_v1_res;
class wlr_output_manager_v1;
class wlr_output_mode_v1;

double estimate_scale(output_state const& data);

class wlr_output_head_v1 : public QObject
{
public:
    wlr_output_head_v1(Server::output& output, wlr_output_manager_v1& manager);
    ~wlr_output_head_v1() override;

    wlr_output_head_v1_res* add_bind(wlr_output_manager_v1_bind& bind);
    void broadcast();

    Server::output& output;
    std::vector<wlr_output_head_v1_res*> resources;
    wlr_output_manager_v1& manager;

    double current_scale{1.};
};

class wlr_output_head_v1_res : public QObject
{
    Q_OBJECT
public:
    wlr_output_head_v1_res(Client* client, uint32_t version, wlr_output_head_v1& head);

    void add_mode(wlr_output_mode_v1& mode) const;
    void send_static_data(output_metadata const& data) const;
    void send_mutable_data(output_state const& data) const;

    void send_enabled(bool enabled) const;
    void send_current_mode(output_mode const& mode) const;
    void send_position(QPoint const& pos) const;
    void send_transform(output_transform transform) const;
    void send_scale(double scale) const;

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

class wlr_output_head_v1_res::Private : public Wayland::Resource<wlr_output_head_v1_res>
{
public:
    Private(Client* client,
            uint32_t version,
            wlr_output_head_v1& head,
            wlr_output_head_v1_res& q_ptr);
    ~Private() override;

    std::vector<wlr_output_mode_v1*> modes;
    wlr_output_head_v1* head;

private:
    static struct zwlr_output_head_v1_interface const s_interface;
};

}
