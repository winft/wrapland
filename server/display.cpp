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
#include "data_device_manager.h"
#include "dpms.h"
#include "egl_stream_controller.h"
#include "fake_input.h"
#include "idle_inhibit_v1.h"
#include "kde_idle.h"
#include "keystate.h"
#include "linux_dmabuf_v1.h"
#include "output_configuration_v1.h"
#include "output_device_v1_p.h"
#include "output_management_v1.h"
#include "output_p.h"
#include "plasma_shell.h"
#include "plasma_virtual_desktop.h"
#include "plasma_window.h"
#include "pointer.h"
#include "pointer_constraints_v1.h"
#include "pointer_gestures_v1.h"
#include "primary_selection.h"
#include "relative_pointer_v1.h"
#include "remote_access.h"
#include "seat.h"
#include "server_decoration_palette.h"
#include "shadow.h"
#include "slide.h"
#include "subcompositor.h"
#include "text_input_v2.h"
#include "viewporter.h"
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
        return client->handle();
    }
    auto* clientHandle = new Client(wlClient, q_ptr);
    setupClient(clientHandle->d_ptr.get());
    return clientHandle;
}

Display::Display(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
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

void Display::setSocketName(const std::string& name)
{
    d_ptr->setSocketName(name);
}

void Display::setSocketName(const QString& name)
{
    d_ptr->setSocketName(name.toUtf8().constData());
}

std::string Display::socketName() const
{
    return d_ptr->socketName();
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

Output* Display::createOutput(QObject* parent)
{
    auto* output = new Output(this, parent);
    d_ptr->outputs.push_back(output);
    return output;
}

void Display::removeOutput(Output* output)
{
    // TODO(romangg): This does not clean up. But it should be also possible to just delete the
    //                output.
    d_ptr->outputs.erase(std::remove(d_ptr->outputs.begin(), d_ptr->outputs.end(), output),
                         d_ptr->outputs.end());
    // d_ptr->removeGlobal(output);
    // delete output;
}

void Display::removeOutputDevice(OutputDeviceV1* outputDevice)
{
    d_ptr->outputDevices.erase(
        std::remove(d_ptr->outputDevices.begin(), d_ptr->outputDevices.end(), outputDevice),
        d_ptr->outputDevices.end());
}

Compositor* Display::createCompositor(QObject* parent)
{
    return new Compositor(this, parent);
}

OutputDeviceV1* Display::createOutputDeviceV1(QObject* parent)
{
    auto device = new OutputDeviceV1(this, parent);
    d_ptr->outputDevices.push_back(device);
    return device;
}

OutputManagementV1* Display::createOutputManagementV1(QObject* parent)
{
    return new OutputManagementV1(this, parent);
}

Seat* Display::createSeat(QObject* parent)
{
    auto seat = new Seat(this, parent);
    d_ptr->seats.push_back(seat);
    connect(seat, &QObject::destroyed, this, [this, seat] {
        d_ptr->seats.erase(std::remove(d_ptr->seats.begin(), d_ptr->seats.end(), seat),
                           d_ptr->seats.end());
    });
    return seat;
}

Subcompositor* Display::createSubCompositor(QObject* parent)
{
    return new Subcompositor(this, parent);
}

DataDeviceManager* Display::createDataDeviceManager(QObject* parent)
{
    return new DataDeviceManager(this, parent);
}

PrimarySelectionDeviceManager* Display::createPrimarySelectionDeviceManager(QObject* parent)
{
    return new PrimarySelectionDeviceManager(this, parent);
}

PlasmaShell* Display::createPlasmaShell(QObject* parent)
{
    return new PlasmaShell(this, parent);
}

PlasmaWindowManager* Display::createPlasmaWindowManager(QObject* parent)
{
    return new PlasmaWindowManager(this, parent);
}

RemoteAccessManager* Display::createRemoteAccessManager(QObject* parent)
{
    return new RemoteAccessManager(this, parent);
}

KdeIdle* Display::createIdle(QObject* parent)
{
    return new KdeIdle(this, parent);
}

FakeInput* Display::createFakeInput(QObject* parent)
{
    return new FakeInput(this, parent);
}

ShadowManager* Display::createShadowManager(QObject* parent)
{
    return new ShadowManager(this, parent);
}

BlurManager* Display::createBlurManager(QObject* parent)
{
    return new BlurManager(this, parent);
}

ContrastManager* Display::createContrastManager(QObject* parent)
{
    return new ContrastManager(this, parent);
}

SlideManager* Display::createSlideManager(QObject* parent)
{
    return new SlideManager(this, parent);
}

DpmsManager* Display::createDpmsManager(QObject* parent)
{
    return new DpmsManager(this, parent);
}

TextInputManagerV2* Display::createTextInputManager(QObject* parent)
{
    return new TextInputManagerV2(this, parent);
}

XdgShell* Display::createXdgShell(QObject* parent)
{
    return new XdgShell(this, parent);
}

RelativePointerManagerV1* Display::createRelativePointerManager(QObject* parent)
{
    return new RelativePointerManagerV1(this, parent);
}

PointerGesturesV1* Display::createPointerGestures(QObject* parent)
{
    return new PointerGesturesV1(this, parent);
}

PointerConstraintsV1* Display::createPointerConstraints(QObject* parent)
{
    return new PointerConstraintsV1(this, parent);
}

XdgForeign* Display::createXdgForeign(QObject* parent)
{
    return new XdgForeign(this, parent);
}

IdleInhibitManagerV1* Display::createIdleInhibitManager(QObject* parent)
{
    return new IdleInhibitManagerV1(this, parent);
}

AppmenuManager* Display::createAppmenuManager(QObject* parent)
{
    return new AppmenuManager(this, parent);
}

ServerSideDecorationPaletteManager*
Display::createServerSideDecorationPaletteManager(QObject* parent)
{
    return new ServerSideDecorationPaletteManager(this, parent);
}

LinuxDmabufV1* Display::createLinuxDmabuf(QObject* parent)
{
    return new LinuxDmabufV1(this, parent);
}

PlasmaVirtualDesktopManager* Display::createPlasmaVirtualDesktopManager(QObject* parent)
{
    return new PlasmaVirtualDesktopManager(this, parent);
}

Viewporter* Display::createViewporter(QObject* parent)
{
    return new Viewporter(this, parent);
}

XdgOutputManager* Display::createXdgOutputManager(QObject* parent)
{
    return new XdgOutputManager(this, parent);
}

XdgDecorationManager* Display::createXdgDecorationManager(XdgShell* shell, QObject* parent)
{
    return new XdgDecorationManager(this, shell, parent);
}

EglStreamController* Display::createEglStreamController(QObject* parent)
{
    return new EglStreamController(this, parent);
}

KeyState* Display::createKeyState(QObject* parent)
{
    return new KeyState(this, parent);
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

std::vector<Output*>& Display::outputs() const
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
        ret.push_back(client->handle());
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
