/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "xdgshell.h"

#include <QDebug>
#include <QRect>
#include <QSize>

#include <wayland-xdg-shell-client-protocol.h>

namespace Wrapland::Client
{

class XdgPositioner::Private
{
public:
    QSize initialSize;
    QRect anchorRect;
    Qt::Edges gravity;
    Qt::Edges anchorEdge;
    XdgPositioner::Constraints constraints;
    QPoint anchorOffset;
};

}
