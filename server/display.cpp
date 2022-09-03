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
#include "input_method_v2.h"
#include "kde_idle.h"
#include "keyboard_shortcuts_inhibit.h"
#include "keystate.h"
#include "layer_shell_v1.h"
#include "linux_dmabuf_v1.h"
#include "output_configuration_v1.h"
#include "output_device_v1_p.h"
#include "output_management_v1.h"
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

Display::~Display()
{
    for (auto output : d_ptr->outputs) {
        output->d_ptr->displayHandle = nullptr;
    }
    for (auto output : d_ptr->outputDevices) {
        output->d_ptr->displayHandle = nullptr;
    }
}

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

void Display::add_output_device_v1(OutputDeviceV1* output)
{
    if (!d_ptr->xdg_output_manager) {
        d_ptr->xdg_output_manager = std::make_unique<XdgOutputManager>(this);
    }

    d_ptr->outputDevices.push_back(output);
}

void Display::add_wl_output(WlOutput* output)
{
    d_ptr->outputs.push_back(output);
}

void Display::removeOutput(WlOutput* output)
{
    d_ptr->outputs.erase(std::remove(d_ptr->outputs.begin(), d_ptr->outputs.end(), output),
                         d_ptr->outputs.end());
}

void Display::removeOutputDevice(OutputDeviceV1* outputDevice)
{
    d_ptr->outputDevices.erase(
        std::remove(d_ptr->outputDevices.begin(), d_ptr->outputDevices.end(), outputDevice),
        d_ptr->outputDevices.end());
}

std::unique_ptr<Compositor> Display::createCompositor()
{
    return std::make_unique<Compositor>(this);
}

std::unique_ptr<OutputManagementV1> Display::createOutputManagementV1()
{
    return std::make_unique<OutputManagementV1>(this);
}

std::unique_ptr<Seat> Display::createSeat()
{
    auto seat = std::make_unique<Seat>(this);
    d_ptr->seats.push_back(seat.get());
    connect(seat.get(), &QObject::destroyed, this, [this, seat_ptr = seat.get()] {
        d_ptr->seats.erase(std::remove(d_ptr->seats.begin(), d_ptr->seats.end(), seat_ptr),
                           d_ptr->seats.end());
    });
    return seat;
}

std::unique_ptr<Subcompositor> Display::createSubCompositor()
{
    return std::make_unique<Subcompositor>(this);
}

std::unique_ptr<data_control_manager_v1> Display::create_data_control_manager_v1()
{
    return std::make_unique<data_control_manager_v1>(this);
}

std::unique_ptr<data_device_manager> Display::createDataDeviceManager()
{
    return std::make_unique<data_device_manager>(this);
}

std::unique_ptr<primary_selection_device_manager> Display::createPrimarySelectionDeviceManager()
{
    return std::make_unique<primary_selection_device_manager>(this);
}

std::unique_ptr<plasma_activation_feedback> Display::create_plasma_activation_feedback()
{
    return std::make_unique<plasma_activation_feedback>(this);
}

std::unique_ptr<PlasmaShell> Display::createPlasmaShell()
{
    return std::make_unique<PlasmaShell>(this);
}

std::unique_ptr<PlasmaWindowManager> Display::createPlasmaWindowManager()
{
    return std::make_unique<PlasmaWindowManager>(this);
}

std::unique_ptr<KdeIdle> Display::createIdle()
{
    return std::make_unique<KdeIdle>(this);
}

std::unique_ptr<KeyboardShortcutsInhibitManagerV1> Display::createKeyboardShortcutsInhibitManager()
{
    return std::make_unique<KeyboardShortcutsInhibitManagerV1>(this);
}

std::unique_ptr<FakeInput> Display::createFakeInput()
{
    return std::make_unique<FakeInput>(this);
}

std::unique_ptr<LayerShellV1> Display::createLayerShellV1()
{
    return std::make_unique<LayerShellV1>(this);
}

std::unique_ptr<ShadowManager> Display::createShadowManager()
{
    return std::make_unique<ShadowManager>(this);
}

std::unique_ptr<BlurManager> Display::createBlurManager()
{
    return std::make_unique<BlurManager>(this);
}

std::unique_ptr<ContrastManager> Display::createContrastManager()
{
    return std::make_unique<ContrastManager>(this);
}

std::unique_ptr<SlideManager> Display::createSlideManager()
{
    return std::make_unique<SlideManager>(this);
}

std::unique_ptr<DpmsManager> Display::createDpmsManager()
{
    return std::make_unique<DpmsManager>(this);
}

std::unique_ptr<drm_lease_device_v1> Display::createDrmLeaseDeviceV1()
{
    return std::make_unique<drm_lease_device_v1>(this);
}

std::unique_ptr<text_input_manager_v2> Display::createTextInputManagerV2()
{
    return std::make_unique<text_input_manager_v2>(this);
}

std::unique_ptr<text_input_manager_v3> Display::createTextInputManagerV3()
{
    return std::make_unique<text_input_manager_v3>(this);
}

std::unique_ptr<XdgShell> Display::createXdgShell()
{
    return std::make_unique<XdgShell>(this);
}

std::unique_ptr<RelativePointerManagerV1> Display::createRelativePointerManager()
{
    return std::make_unique<RelativePointerManagerV1>(this);
}

std::unique_ptr<PointerGesturesV1> Display::createPointerGestures()
{
    return std::make_unique<PointerGesturesV1>(this);
}

std::unique_ptr<PointerConstraintsV1> Display::createPointerConstraints()
{
    return std::make_unique<PointerConstraintsV1>(this);
}

std::unique_ptr<XdgForeign> Display::createXdgForeign()
{
    return std::make_unique<XdgForeign>(this);
}

std::unique_ptr<IdleInhibitManagerV1> Display::createIdleInhibitManager()
{
    return std::make_unique<IdleInhibitManagerV1>(this);
}

std::unique_ptr<input_method_manager_v2> Display::createInputMethodManagerV2()
{
    return std::make_unique<input_method_manager_v2>(this);
}

std::unique_ptr<AppmenuManager> Display::createAppmenuManager()
{
    return std::make_unique<AppmenuManager>(this);
}

std::unique_ptr<ServerSideDecorationPaletteManager>
Display::createServerSideDecorationPaletteManager()
{
    return std::make_unique<ServerSideDecorationPaletteManager>(this);
}

std::unique_ptr<PlasmaVirtualDesktopManager> Display::createPlasmaVirtualDesktopManager()
{
    return std::make_unique<PlasmaVirtualDesktopManager>(this);
}

std::unique_ptr<Viewporter> Display::createViewporter()
{
    return std::make_unique<Viewporter>(this);
}

std::unique_ptr<virtual_keyboard_manager_v1> Display::create_virtual_keyboard_manager_v1()
{
    return std::make_unique<virtual_keyboard_manager_v1>(this);
}

XdgOutputManager* Display::xdgOutputManager() const
{
    return d_ptr->xdg_output_manager.get();
}

std::unique_ptr<XdgActivationV1> Display::createXdgActivationV1()
{
    return std::make_unique<XdgActivationV1>(this);
}

std::unique_ptr<XdgDecorationManager> Display::createXdgDecorationManager(XdgShell* shell)
{
    return std::make_unique<XdgDecorationManager>(this, shell);
}

std::unique_ptr<KeyState> Display::createKeyState()
{
    return std::make_unique<KeyState>(this);
}

std::unique_ptr<PresentationManager> Display::createPresentationManager()
{
    return std::make_unique<PresentationManager>(this);
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

std::vector<WlOutput*>& Display::outputs() const
{
    return d_ptr->outputs;
}

std::vector<Seat*>& Display::seats() const
{
    return d_ptr->seats;
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
