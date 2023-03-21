/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "wayland/global.h"
#include "wlr_output_manager_v1.h"

#include "wayland-wlr-output-management-v1-server-protocol.h"

#include <QObject>
#include <memory>

namespace Wrapland::Server
{

class wlr_output_configuration_v1;
class wlr_output_head_v1;

constexpr uint32_t wlr_output_manager_v1_version = 2;
using wlr_output_manager_v1_global
    = Wayland::Global<wlr_output_manager_v1, wlr_output_manager_v1_version>;
using wlr_output_manager_v1_bind = Wayland::Bind<wlr_output_manager_v1_global>;

class wlr_output_manager_v1::Private : public wlr_output_manager_v1_global
{
public:
    Private(wlr_output_manager_v1* q_ptr, Display* display);
    ~Private() override;

    void bindInit(wlr_output_manager_v1_bind* bind) override;
    void prepareUnbind(wlr_output_manager_v1_bind* bind) override;

    void add_head(wlr_output_head_v1& head);
    bool is_finished(wlr_output_manager_v1_bind* bind) const;

    bool changed{false};
    uint32_t serial{0};

    std::vector<wlr_output_head_v1*> heads;
    std::vector<wlr_output_configuration_v1*> configurations;
    std::vector<wlr_output_manager_v1_bind*> finished_binds;

private:
    static void
    create_configuration_callback(wlr_output_manager_v1_bind* bind, uint32_t id, uint32_t serial);
    static void stop_callback(wlr_output_manager_v1_bind* bind);

    static struct zwlr_output_manager_v1_interface const s_interface;
};

}
