/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "output_manager.h"

#include "display.h"
#include "output.h"
#include "output_management_v1.h"
#include "xdg_output.h"

namespace Wrapland::Server
{

output_manager::output_manager(Display& display)
    : display{display}
{
}

output_manager::~output_manager()
{
    assert(outputs.empty());
}

XdgOutputManager& output_manager::create_xdg_manager()
{
    assert(!xdg_manager);
    xdg_manager = std::make_unique<XdgOutputManager>(&display);
    return *xdg_manager;
}

OutputManagementV1& output_manager::create_management_v1()
{
    assert(!management_v1);
    management_v1 = std::make_unique<OutputManagementV1>(&display);
    return *management_v1;
}

}
