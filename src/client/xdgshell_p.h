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

namespace Wrapland::Client
{

class Q_DECL_HIDDEN XdgShell::Private
{
public:
    virtual ~Private();
    virtual void setup(xdg_wm_base* xdgshell)
    {
        Q_UNUSED(xdgshell);
    }
    virtual void release() = 0;
    virtual bool isValid() const = 0;
    virtual operator xdg_wm_base*()
    {
        return nullptr;
    }
    virtual operator xdg_wm_base*() const
    {
        return nullptr;
    }

    virtual XdgShellSurface* getXdgSurface(Surface* surface, QObject* parent) = 0;

    virtual XdgShellPopup* getXdgPopup(Surface* surface,
                                       Surface* parentSurface,
                                       Seat* seat,
                                       quint32 serial,
                                       const QPoint& parentPos,
                                       QObject* parent)
    {
        Q_UNUSED(surface)
        Q_UNUSED(parentSurface)
        Q_UNUSED(seat)
        Q_UNUSED(serial)
        Q_UNUSED(parentPos)
        Q_UNUSED(parent)
        Q_ASSERT(false);
        return nullptr;
    };

    virtual XdgShellPopup* getXdgPopup(Surface* surface,
                                       XdgShellSurface* parentSurface,
                                       const XdgPositioner& positioner,
                                       QObject* parent)
    {
        Q_UNUSED(surface)
        Q_UNUSED(parentSurface)
        Q_UNUSED(positioner)
        Q_UNUSED(parent)
        Q_ASSERT(false);
        return nullptr;
    }

    virtual XdgShellPopup* getXdgPopup(Surface* surface,
                                       XdgShellPopup* parentSurface,
                                       const XdgPositioner& positioner,
                                       QObject* parent)
    {

        Q_UNUSED(surface)
        Q_UNUSED(parentSurface)
        Q_UNUSED(positioner)
        Q_UNUSED(parent)
        Q_ASSERT(false);
        return nullptr;
    }

    EventQueue* queue = nullptr;

protected:
    Private() = default;
};

class XdgShellStable : public XdgShell
{
    Q_OBJECT
public:
    explicit XdgShellStable(QObject* parent = nullptr);
    virtual ~XdgShellStable();

private:
    class Private;
};

class XdgTopLevelStable : public XdgShellSurface
{
    Q_OBJECT
public:
    virtual ~XdgTopLevelStable();

private:
    explicit XdgTopLevelStable(QObject* parent = nullptr);
    friend class XdgShellStable;
    class Private;
};

class Q_DECL_HIDDEN XdgShellSurface::Private
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

    virtual void setTransientFor(XdgShellSurface* parent) = 0;
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
    Private(XdgShellSurface* q);

    XdgShellSurface* q_ptr;
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
    friend class XdgShellStable;
    class Private;
};

}
