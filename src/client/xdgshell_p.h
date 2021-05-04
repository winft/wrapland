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

class XdgTopLevelStable : public XdgShellToplevel
{
    Q_OBJECT
public:
    virtual ~XdgTopLevelStable();

private:
    explicit XdgTopLevelStable(QObject* parent = nullptr);
    friend class XdgShell;
    class Private;
};

class Q_DECL_HIDDEN XdgShellToplevel::Private
{
public:
    virtual ~Private();
    EventQueue* queue = nullptr;
    QSize size;

    virtual void setup(xdg_surface* surface, xdg_toplevel* toplevel)
    {
        Q_UNUSED(surface)
        Q_UNUSED(toplevel)
    }
    virtual void release() = 0;
    virtual bool isValid() const = 0;
    virtual operator xdg_surface*()
    {
        return nullptr;
    }
    virtual operator xdg_surface*() const
    {
        return nullptr;
    }
    virtual operator xdg_toplevel*()
    {
        return nullptr;
    }
    virtual operator xdg_toplevel*() const
    {
        return nullptr;
    }

    virtual void setTransientFor(XdgShellToplevel* parent) = 0;
    virtual void setTitle(const QString& title) = 0;
    virtual void setAppId(const QByteArray& appId) = 0;
    virtual void showWindowMenu(Seat* seat, quint32 serial, qint32 x, qint32 y) = 0;
    virtual void move(Seat* seat, quint32 serial) = 0;
    virtual void resize(Seat* seat, quint32 serial, Qt::Edges edges) = 0;
    virtual void ackConfigure(quint32 serial) = 0;
    virtual void setMaximized() = 0;
    virtual void unsetMaximized() = 0;
    virtual void setFullscreen(Output* output) = 0;
    virtual void unsetFullscreen() = 0;
    virtual void setMinimized() = 0;
    virtual void setMaxSize(const QSize& size) = 0;
    virtual void setMinSize(const QSize& size) = 0;
    virtual void setWindowGeometry(const QRect& windowGeometry)
    {
        Q_UNUSED(windowGeometry);
    }

protected:
    Private(XdgShellToplevel* q);

    XdgShellToplevel* q_ptr;
};

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
