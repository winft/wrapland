/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <gsl/pointers>
#include <memory>
#include <vector>

namespace Wrapland::Server
{

class output;
class Display;
class wlr_output_manager_v1;
class XdgOutputManager;

class WRAPLANDSERVER_EXPORT output_manager
{
public:
    explicit output_manager(Display& display);
    virtual ~output_manager();

    void commit_changes() const;

    XdgOutputManager& create_xdg_manager();
    wlr_output_manager_v1& create_wlr_manager_v1();

    gsl::not_null<Display*> display;
    std::vector<output*> outputs;

    std::unique_ptr<XdgOutputManager> xdg_manager;
    std::unique_ptr<Server::wlr_output_manager_v1> wlr_manager_v1;
};

}
