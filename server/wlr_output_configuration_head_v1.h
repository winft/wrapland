/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "output.h"

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{

class Client;
class wlr_output_configuration_v1;
class wlr_output_head_v1_res;

class WRAPLANDSERVER_EXPORT wlr_output_configuration_head_v1 : public QObject
{
    Q_OBJECT
public:
    ~wlr_output_configuration_head_v1() override;

    output& get_output() const;
    output_state const& get_state() const;

Q_SIGNALS:
    void resourceDestroyed();

private:
    wlr_output_configuration_head_v1(Client* client,
                                     uint32_t version,
                                     uint32_t id,
                                     wlr_output_head_v1_res& head);
    friend class wlr_output_configuration_v1_res;

    class Private;
    Private* d_ptr;
};

}
