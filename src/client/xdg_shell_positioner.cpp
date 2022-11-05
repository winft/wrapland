/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_shell_positioner.h"

namespace Wrapland::Client
{

class xdg_shell_positioner::Private
{
public:
    xdg_shell_positioner_data data;
};

xdg_shell_positioner::xdg_shell_positioner(xdg_shell_positioner_data data)
    : d_ptr(new Private)
{
    d_ptr->data = std::move(data);
}

xdg_shell_positioner::xdg_shell_positioner(xdg_shell_positioner const& other)
    : d_ptr(new Private)
{
    *d_ptr = *other.d_ptr;
}

xdg_shell_positioner::~xdg_shell_positioner()
{
}

xdg_shell_positioner_data const& xdg_shell_positioner::get_data() const
{
    return d_ptr->data;
}

}
