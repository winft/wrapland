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

class Display;
class Output;
class wlr_output_configuration_v1;

class WRAPLANDSERVER_EXPORT wlr_output_manager_v1 : public QObject
{
    Q_OBJECT
public:
    explicit wlr_output_manager_v1(Display* display);
    ~wlr_output_manager_v1() override;

    void done();

Q_SIGNALS:
    void apply_config(Wrapland::Server::wlr_output_configuration_v1* configuration);
    void test_config(Wrapland::Server::wlr_output_configuration_v1* configuration);

private:
    friend class wlr_output_configuration_v1;
    friend class wlr_output_configuration_v1_res;
    friend class wlr_output_head_v1;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
