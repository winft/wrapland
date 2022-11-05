/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "xdg_shell_surface.h"

#include <QRect>
#include <QSize>

namespace Wrapland::Server
{

class xdg_shell_positioner
{
public:
    struct {
        QRect rect;
        Qt::Edges edge;
        QPoint offset;
    } anchor;

    QSize size;
    Qt::Edges gravity;
    XdgShellSurface::ConstraintAdjustments constraint_adjustments;
};

}
