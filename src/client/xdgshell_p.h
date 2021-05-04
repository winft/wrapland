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

class Q_DECL_HIDDEN XdgShellPopup::Private
{
public:
    Private(XdgShellPopup* q);
    virtual ~Private();

    EventQueue* queue = nullptr;

    virtual void setup(xdg_surface* s, xdg_popup* p)
    {
        Q_UNUSED(s)
        Q_UNUSED(p)
    }
    virtual void release() = 0;
    virtual bool isValid() const = 0;
    virtual void requestGrab(Seat* seat, quint32 serial)
    {
        Q_UNUSED(seat);
        Q_UNUSED(serial);
    };
    virtual void ackConfigure(quint32 serial)
    {
        Q_UNUSED(serial);
    }

    virtual void setWindowGeometry(const QRect& windowGeometry)
    {
        Q_UNUSED(windowGeometry);
    }

    virtual operator xdg_surface*()
    {
        return nullptr;
    }
    virtual operator xdg_surface*() const
    {
        return nullptr;
    }
    virtual operator xdg_popup*()
    {
        return nullptr;
    }
    virtual operator xdg_popup*() const
    {
        return nullptr;
    }

protected:
    XdgShellPopup* q_ptr;

private:
};

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

class XdgShellPopupStable : public XdgShellPopup
{
public:
    virtual ~XdgShellPopupStable();

private:
    explicit XdgShellPopupStable(QObject* parent = nullptr);
    friend class XdgShell;
    class Private;
};

}
