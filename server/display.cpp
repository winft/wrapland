/********************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "display.h"

#include "client.h"
#include "client_p.h"

#include "wayland/client.h"
#include "wayland/display.h"

#include "appmenu.h"
#include "blur.h"
#include "compositor.h"
#include "contrast.h"
#include "data_control_v1.h"
#include "data_device_manager.h"
#include "dpms.h"
#include "drm_lease_v1.h"
#include "fake_input.h"
#include "idle_inhibit_v1.h"
#include "idle_notify_v1.h"
#include "input_method_v2.h"
#include "kde_idle.h"
#include "keyboard_shortcuts_inhibit.h"
#include "keystate.h"
#include "layer_shell_v1.h"
#include "linux_dmabuf_v1.h"
#include "plasma_activation_feedback.h"
#include "plasma_shell.h"
#include "plasma_virtual_desktop.h"
#include "plasma_window.h"
#include "pointer.h"
#include "pointer_constraints_v1.h"
#include "pointer_gestures_v1.h"
#include "presentation_time.h"
#include "primary_selection.h"
#include "relative_pointer_v1.h"
#include "seat.h"
#include "server_decoration_palette.h"
#include "shadow.h"
#include "slide.h"
#include "subcompositor.h"
#include "text_input_v2.h"
#include "text_input_v3.h"
#include "viewporter.h"
#include "virtual_keyboard_v1.h"
#include "wl_output_p.h"
#include "xdg_activation_v1.h"
#include "xdg_decoration.h"
#include "xdg_foreign.h"
#include "xdg_output.h"
#include "xdg_shell.h"

#include "logging.h"

#include "display_p.h"

#include <EGL/egl.h>

#include <algorithm>
#include <wayland-server.h>

namespace Wrapland::Server
{

Private* Private::castDisplay(Server::Display* display)
{
    return display->d_ptr.get();
}

Private::Private(Server::Display* display)
    : Wayland::Display(display)
    , q_ptr(display)
{
}

Wayland::Client* Private::castClientImpl(Server::Client* client)
{
    return client->d_ptr.get();
}

Client* Private::createClientHandle(wl_client* wlClient)
{
    if (auto* client = getClient(wlClient)) {
        return client->handle;
    }
    auto* clientHandle = new Client(wlClient, q_ptr);
    setupClient(clientHandle->d_ptr.get());
    return clientHandle;
}

Display::Display()
    : d_ptr(new Private(this))
{
}

Display::~Display() = default;

void Display::set_socket_name(std::string const& name)
{
    d_ptr->socket_name = name;
}

std::string Display::socket_name() const
{
    return d_ptr->socket_name;
}

void Display::add_socket_fd(int fd)
{
    d_ptr->add_socket_fd(fd);
}

void Display::start(StartMode mode)
{
    d_ptr->start(mode == StartMode::ConnectToSocket);
    Q_EMIT started();
}

void Display::startLoop()
{
    d_ptr->startLoop();
}

void Display::dispatchEvents(int msecTimeout)
{
    d_ptr->dispatchEvents(msecTimeout);
}

void Display::dispatch()
{
    d_ptr->dispatch();
}

void Display::flush()
{
    d_ptr->flush();
}

void Display::terminate()
{
    d_ptr->terminate();
}

void Display::createShm()
{
    Q_ASSERT(d_ptr->native());
    wl_display_init_shm(d_ptr->native());
}

quint32 Display::nextSerial()
{
    return wl_display_next_serial(d_ptr->native());
}

quint32 Display::serial()
{
    return wl_display_get_serial(d_ptr->native());
}

bool Display::running() const
{
    return d_ptr->running();
}

wl_display* Display::native() const
{
    return d_ptr->native();
}

Client* Display::getClient(wl_client* wlClient)
{
    return d_ptr->createClientHandle(wlClient);
}

std::vector<Client*> Display::clients() const
{
    std::vector<Client*> ret;
    for (auto* client : d_ptr->clients()) {
        ret.push_back(client->handle);
    }
    return ret;
}

Client* Display::createClient(int fd)
{
    return getClient(d_ptr->createClient(fd));
}

void Display::setEglDisplay(void* display)
{
    if (d_ptr->eglDisplay != EGL_NO_DISPLAY) {
        qCWarning(WRAPLAND_SERVER, "EGLDisplay cannot be changed");
        return;
    }
    d_ptr->eglDisplay = static_cast<EGLDisplay>(display);
}

void* Display::eglDisplay() const
{
    return d_ptr->eglDisplay;
}

}
