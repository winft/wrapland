/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "xdg_shell_popup.h"
#include "xdg_shell_positioner.h"
#include "xdg_shell_toplevel.h"

#include <QObject>
#include <QRect>
#include <QSize>

#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct xdg_wm_base;

namespace Wrapland::Client
{

class EventQueue;
class Surface;

/**
 * @short Wrapper for the xdg_shell interface.
 *
 * This class provides a convenient wrapper for the xdg_shell interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the XdgShell interface:
 * @code
 * XdgShell *c = registry->createXdgShell(name, version);
 * @endcode
 *
 * This creates the XdgShell and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * XdgShell *c = new XdgShell;
 * c->setup(registry->bindXdgShell(name, version));
 * @endcode
 *
 * The XdgShell can be used as a drop-in replacement for any xdg_shell
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.0.525
 **/
class WRAPLANDCLIENT_EXPORT XdgShell : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new XdgShell.
     * Note: after constructing the XdgShell it is not yet valid and one needs
     * to call setup. In order to get a ready to use XdgShell prefer using
     * Registry::createXdgShell.
     **/
    explicit XdgShell(QObject* parent = nullptr);
    virtual ~XdgShell();

    /**
     * Setup this XdgShell to manage the @p xdg_wm_base.
     * When using Registry::createXdgShell there is no need to call this
     * method.
     **/
    void setup(xdg_wm_base* xdg_wm_base);
    /**
     * @returns @c true if managing a xdg_shell.
     **/
    bool isValid() const;
    /**
     * Releases the xdg_shell interface.
     * After the interface has been released the XdgShell instance is no
     * longer valid and can be setup with another xdg_shell interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this XdgShell.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this XdgShell.
     **/
    EventQueue* eventQueue();

    /**
     * Creates a new XdgShellToplevel for the given @p surface.
     **/
    XdgShellToplevel* create_toplevel(Surface* surface, QObject* parent = nullptr);

    /**
     * Creates a new XdgShellPopup for the given @p surface on top of @p parentSurface with the
     * given @p positioner.
     * @since 0.0.539
     **/
    XdgShellPopup* create_popup(Surface* surface,
                                XdgShellToplevel* parentSurface,
                                XdgPositioner const& positioner,
                                QObject* parent = nullptr);

    /**
     * Creates a new XdgShellPopup for the given @p surface on top of @p parentSurface with the
     * given @p positioner.
     * @since 0.0.539
     **/
    XdgShellPopup* create_popup(Surface* surface,
                                XdgShellPopup* parentSurface,
                                XdgPositioner const& positioner,
                                QObject* parent = nullptr);

    /**
     * Creates a new XdgShellPopup for the given @p surface with no parent directly specified and
     * with the given @p positioner.
     * @since 0.522.0
     **/
    XdgShellPopup*
    create_popup(Surface* surface, XdgPositioner const& positioner, QObject* parent = nullptr);

    operator xdg_wm_base*();
    operator xdg_wm_base*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the XdgShell got created by
     * Registry::createXdgShell
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
