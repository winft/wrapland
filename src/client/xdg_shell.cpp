/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "xdg_shell.h"

#include "xdg_shell_popup.h"
#include "xdg_shell_positioner.h"
#include "xdg_shell_toplevel.h"

#include "event_queue.h"
#include "seat.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <wayland-xdg-shell-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN XdgShell::Private
{
public:
    virtual ~Private();

    void setup(xdg_wm_base* shell);
    void release();
    bool isValid() const;

    operator xdg_wm_base*()
    {
        return xdg_shell_base;
    }
    operator xdg_wm_base*() const
    {
        return xdg_shell_base;
    }

    XdgShellToplevel* getXdgToplevel(Surface* surface, QObject* parent);
    XdgShellPopup* get_xdg_popup(Surface* surface,
                                 xdg_surface* parentSurface,
                                 xdg_shell_positioner_data positioner_data,
                                 QObject* parent);
    xdg_shell_positioner* get_xdg_positioner(xdg_shell_positioner_data data, QObject* parent);

    EventQueue* queue = nullptr;

private:
    static void pingCallback(void* data, struct xdg_wm_base* shell, uint32_t serial);

    WaylandPointer<xdg_wm_base, xdg_wm_base_destroy> xdg_shell_base;
    static const struct xdg_wm_base_listener s_shellListener;
};

XdgShell::Private::~Private() = default;

struct xdg_wm_base_listener const XdgShell::Private::s_shellListener = {
    pingCallback,
};

void XdgShell::Private::pingCallback(void* data, struct xdg_wm_base* shell, uint32_t serial)
{
    Q_UNUSED(data)
    xdg_wm_base_pong(shell, serial);
}

void XdgShell::Private::setup(xdg_wm_base* shell)
{
    Q_ASSERT(shell);
    Q_ASSERT(!xdg_shell_base);
    xdg_shell_base.setup(shell);
    xdg_wm_base_add_listener(shell, &s_shellListener, this);
}

void XdgShell::Private::release()
{
    xdg_shell_base.release();
}

bool XdgShell::Private::isValid() const
{
    return xdg_shell_base.isValid();
}

XdgShellToplevel* XdgShell::Private::getXdgToplevel(Surface* surface, QObject* parent)
{
    Q_ASSERT(isValid());
    auto ss = xdg_wm_base_get_xdg_surface(xdg_shell_base, *surface);

    if (!ss) {
        return nullptr;
    }

    auto s = new XdgShellToplevel(parent);
    auto toplevel = xdg_surface_get_toplevel(ss);
    if (queue) {
        queue->addProxy(ss);
        queue->addProxy(toplevel);
    }
    s->setup(ss, toplevel);
    return s;
}

XdgShellPopup* XdgShell::Private::get_xdg_popup(Surface* surface,
                                                xdg_surface* parentSurface,
                                                xdg_shell_positioner_data positioner_data,
                                                QObject* parent)
{
    Q_ASSERT(isValid());
    auto ss = xdg_wm_base_get_xdg_surface(xdg_shell_base, *surface);
    if (!ss) {
        return nullptr;
    }

    XdgShellPopup* s = new XdgShellPopup(parent);
    auto positioner = get_xdg_positioner(positioner_data, nullptr);
    auto popup = xdg_surface_get_popup(ss, parentSurface, *positioner);
    if (queue) {
        // deliberately not adding the positioner because the positioner has no events sent to it
        queue->addProxy(ss);
        queue->addProxy(popup);
    }
    s->setup(ss, popup);
    delete positioner;

    return s;
}

xdg_shell_positioner* XdgShell::Private::get_xdg_positioner(xdg_shell_positioner_data data,
                                                            QObject* parent)
{
    auto positioner = new xdg_shell_positioner(parent);
    positioner->setup(xdg_wm_base_create_positioner(xdg_shell_base));
    positioner->set_data(std::move(data));

    // Deliberately not adding the positioner to the queue because it has no events sent to it.
    return positioner;
}

XdgShell::XdgShell(QObject* parent)
    : QObject(parent)
    , d_ptr{new Private}
{
}

XdgShell::~XdgShell()
{
    release();
}

void XdgShell::setup(xdg_wm_base* xdg_wm_base)
{
    d_ptr->setup(xdg_wm_base);
}

void XdgShell::release()
{
    d_ptr->release();
}

void XdgShell::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* XdgShell::eventQueue()
{
    return d_ptr->queue;
}

XdgShell::operator xdg_wm_base*()
{
    return *(d_ptr.get());
}

XdgShell::operator xdg_wm_base*() const
{
    return *(d_ptr.get());
}

bool XdgShell::isValid() const
{
    return d_ptr->isValid();
}

XdgShellToplevel* XdgShell::create_toplevel(Surface* surface, QObject* parent)
{
    return d_ptr->getXdgToplevel(surface, parent);
}

XdgShellPopup* XdgShell::create_popup(Surface* surface,
                                      XdgShellToplevel* parentSurface,
                                      xdg_shell_positioner_data positioner_data,
                                      QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, *parentSurface, std::move(positioner_data), parent);
}

XdgShellPopup* XdgShell::create_popup(Surface* surface,
                                      XdgShellPopup* parentSurface,
                                      xdg_shell_positioner_data positioner_data,
                                      QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, *parentSurface, std::move(positioner_data), parent);
}

XdgShellPopup*
XdgShell::create_popup(Surface* surface, xdg_shell_positioner_data positioner_data, QObject* parent)
{
    return d_ptr->get_xdg_popup(surface, nullptr, std::move(positioner_data), parent);
}

xdg_shell_positioner* XdgShell::create_positioner(xdg_shell_positioner_data data, QObject* parent)
{
    return d_ptr->get_xdg_positioner(std::move(data), parent);
}

}
