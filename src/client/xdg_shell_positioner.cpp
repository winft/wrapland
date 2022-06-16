/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_shell_positioner.h"

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
    bool isReactive;
    uint32_t parentConfigure;
    QSize parentSize;
};

XdgPositioner::XdgPositioner(const QSize& initialSize, const QRect& anchor)
    : d_ptr(new Private)
{
    d_ptr->initialSize = initialSize;
    d_ptr->anchorRect = anchor;
}

XdgPositioner::XdgPositioner(const XdgPositioner& other)
    : d_ptr(new Private)
{
    *d_ptr = *other.d_ptr;
}

XdgPositioner::~XdgPositioner()
{
}

void XdgPositioner::setInitialSize(const QSize& size)
{
    d_ptr->initialSize = size;
}

QSize XdgPositioner::initialSize() const
{
    return d_ptr->initialSize;
}

void XdgPositioner::setAnchorRect(const QRect& anchor)
{
    d_ptr->anchorRect = anchor;
}

QRect XdgPositioner::anchorRect() const
{
    return d_ptr->anchorRect;
}

void XdgPositioner::setAnchorEdge(Qt::Edges edge)
{
    d_ptr->anchorEdge = edge;
}

Qt::Edges XdgPositioner::anchorEdge() const
{
    return d_ptr->anchorEdge;
}

void XdgPositioner::setAnchorOffset(const QPoint& offset)
{
    d_ptr->anchorOffset = offset;
}

QPoint XdgPositioner::anchorOffset() const
{
    return d_ptr->anchorOffset;
}

void XdgPositioner::setGravity(Qt::Edges edge)
{
    d_ptr->gravity = edge;
}

Qt::Edges XdgPositioner::gravity() const
{
    return d_ptr->gravity;
}

void XdgPositioner::setConstraints(Constraints constraints)
{
    d_ptr->constraints = constraints;
}

XdgPositioner::Constraints XdgPositioner::constraints() const
{
    return d_ptr->constraints;
}

void XdgPositioner::setReactive(bool isReactive)
{
    d_ptr->isReactive = isReactive;
}

bool XdgPositioner::isReactive() const
{
    return d_ptr->isReactive;
}

void XdgPositioner::setParentSize(const QSize& parentSize)
{
    d_ptr->parentSize = parentSize;
}

QSize XdgPositioner::parentSize() const
{
    return d_ptr->parentSize;
}

void XdgPositioner::setParentConfigure(uint32_t parentConfigure)
{
    d_ptr->parentConfigure = parentConfigure;
}

uint32_t XdgPositioner::parentConfigure() const
{
    return d_ptr->parentConfigure;
}

}
