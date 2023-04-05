/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "output_p.h"
#include "wayland/resource.h"

#include "wayland-wlr-output-management-v1-server-protocol.h"

#include <QObject>

namespace Wrapland::Server
{

class Client;

class wlr_output_mode_v1 : public QObject
{
    Q_OBJECT
public:
    wlr_output_mode_v1(Client* client, uint32_t version, output_mode const& mode);

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

class wlr_output_mode_v1::Private : public Wayland::Resource<wlr_output_mode_v1>
{
public:
    Private(Client* client, uint32_t version, output_mode const& mode, wlr_output_mode_v1& q_ptr);
    void send_data();

    output_mode mode;

private:
    static struct zwlr_output_mode_v1_interface const s_interface;
};

}
