/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>
#include <vector>

namespace Wrapland::Server
{

class output;
class Display;
class OutputManagementV1;
class XdgOutputManager;

class WRAPLANDSERVER_EXPORT output_manager
{
public:
    explicit output_manager(Display& display);
    virtual ~output_manager();

    XdgOutputManager& create_xdg_manager();
    OutputManagementV1& create_management_v1();

    Display& display;
    std::vector<output*> outputs;

    std::unique_ptr<XdgOutputManager> xdg_manager;
    std::unique_ptr<OutputManagementV1> management_v1;
};

}
