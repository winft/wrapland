/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{

class Client;
class wlr_output_configuration_head_v1;
class wlr_output_manager_v1;

class WRAPLANDSERVER_EXPORT wlr_output_configuration_v1 : public QObject
{
    Q_OBJECT
public:
    ~wlr_output_configuration_v1() override;

    std::vector<wlr_output_configuration_head_v1*> enabled_heads() const;

    void send_succeeded();
    void send_failed();
    void send_cancelled();

private:
    wlr_output_configuration_v1(Client* client,
                                uint32_t version,
                                uint32_t id,
                                wlr_output_manager_v1& manager);
    friend class wlr_output_configuration_v1_res;
    friend class wlr_output_manager_v1;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::wlr_output_configuration_v1*)
