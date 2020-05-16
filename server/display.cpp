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

Private* Private::castDisplay(D_isplay* display)
{
    return display->d_ptr.get();
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

D_isplay::D_isplay(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

D_isplay::~D_isplay()
{
    for (auto output : d_ptr->outputs) {
        output->d_ptr->displayHandle = nullptr;
    }
    for (auto output : d_ptr->outputDevices) {
        output->d_ptr->displayHandle = nullptr;
    }
}

void D_isplay::setSocketName(const std::string& name)
{
    d_ptr->setSocketName(name);
}

void D_isplay::setSocketName(const QString& name)
{
    d_ptr->setSocketName(name.toUtf8().constData());
}

std::string const D_isplay::socketName() const
{
    return d_ptr->socketName();
}

void D_isplay::start(StartMode mode)
{
    d_ptr->start(mode == StartMode::ConnectToSocket);
    Q_EMIT started();
}

void D_isplay::startLoop()
{
    d_ptr->startLoop();
}

void D_isplay::dispatchEvents(int msecTimeout)
{
    d_ptr->dispatchEvents(msecTimeout);
}

void D_isplay::dispatch()
{
    d_ptr->dispatch();
}

void D_isplay::flush()
{
    d_ptr->flush();
}

void D_isplay::terminate()
{
    d_ptr->terminate();
}

Output* D_isplay::createOutput(QObject* parent)
{
    auto* output = new Output(this, parent);
    d_ptr->outputs.push_back(output);
    return output;
}

void D_isplay::removeOutput(Output* output)
{
    // TODO: this does not clean up. But it should be also possible to just delete the output.
    d_ptr->outputs.erase(std::remove(d_ptr->outputs.begin(), d_ptr->outputs.end(), output),
                         d_ptr->outputs.end());
    // d_ptr->removeGlobal(output);
    // delete output;
}

void D_isplay::removeOutputDevice(OutputDeviceV1* outputDevice)
{
    d_ptr->outputDevices.erase(
        std::remove(d_ptr->outputDevices.begin(), d_ptr->outputDevices.end(), outputDevice),
        d_ptr->outputDevices.end());
}

Compositor* D_isplay::createCompositor(QObject* parent)
{
    return new Compositor(this, parent);
}

OutputDeviceV1* D_isplay::createOutputDeviceV1(QObject* parent)
{
    auto device = new OutputDeviceV1(this, parent);
    d_ptr->outputDevices.push_back(device);
    return device;
}

OutputManagementV1* D_isplay::createOutputManagementV1(QObject* parent)
{
    return new OutputManagementV1(this, parent);
}

Seat* D_isplay::createSeat(QObject* parent)
{
    auto seat = new Seat(this, parent);
    d_ptr->seats.push_back(seat);
    connect(seat, &QObject::destroyed, this, [this, seat] {
        d_ptr->seats.erase(std::remove(d_ptr->seats.begin(), d_ptr->seats.end(), seat),
                           d_ptr->seats.end());
    });
    return seat;
}

Subcompositor* D_isplay::createSubCompositor(QObject* parent)
{
    return new Subcompositor(this, parent);
}

DataDeviceManager* D_isplay::createDataDeviceManager(QObject* parent)
{
    return new DataDeviceManager(this, parent);
}

PlasmaShell* D_isplay::createPlasmaShell(QObject* parent)
{
    return new PlasmaShell(this, parent);
}

PlasmaWindowManager* D_isplay::createPlasmaWindowManager(QObject* parent)
{
    return new PlasmaWindowManager(this, parent);
}

RemoteAccessManager* D_isplay::createRemoteAccessManager(QObject* parent)
{
    return new RemoteAccessManager(this, parent);
}

KdeIdle* D_isplay::createIdle(QObject* parent)
{
    return new KdeIdle(this, parent);
}

FakeInput* D_isplay::createFakeInput(QObject* parent)
{
    return new FakeInput(this, parent);
}

ShadowManager* D_isplay::createShadowManager(QObject* parent)
{
    return new ShadowManager(this, parent);
}

BlurManager* D_isplay::createBlurManager(QObject* parent)
{
    return new BlurManager(this, parent);
}

ContrastManager* D_isplay::createContrastManager(QObject* parent)
{
    return new ContrastManager(this, parent);
}

SlideManager* D_isplay::createSlideManager(QObject* parent)
{
    return new SlideManager(this, parent);
}

DpmsManager* D_isplay::createDpmsManager(QObject* parent)
{
    return new DpmsManager(this, parent);
}

TextInputManagerV2* D_isplay::createTextInputManager(QObject* parent)
{
    return new TextInputManagerV2(this, parent);
}

XdgShell* D_isplay::createXdgShell(QObject* parent)
{
    return new XdgShell(this, parent);
}

RelativePointerManagerV1* D_isplay::createRelativePointerManager(QObject* parent)
{
    return new RelativePointerManagerV1(this, parent);
}

PointerGesturesV1* D_isplay::createPointerGestures(QObject* parent)
{
    return new PointerGesturesV1(this, parent);
}

PointerConstraintsV1* D_isplay::createPointerConstraints(QObject* parent)
{
    return new PointerConstraintsV1(this, parent);
}

XdgForeign* D_isplay::createXdgForeign(QObject* parent)
{
    return new XdgForeign(this, parent);
}

IdleInhibitManagerV1* D_isplay::createIdleInhibitManager(QObject* parent)
{
    return new IdleInhibitManagerV1(this, parent);
}

AppmenuManager* D_isplay::createAppmenuManager(QObject* parent)
{
    return new AppmenuManager(this, parent);
}

ServerSideDecorationPaletteManager*
D_isplay::createServerSideDecorationPaletteManager(QObject* parent)
{
    return new ServerSideDecorationPaletteManager(this, parent);
}

LinuxDmabufV1* D_isplay::createLinuxDmabuf(QObject* parent)
{
    return new LinuxDmabufV1(this, parent);
}

PlasmaVirtualDesktopManager* D_isplay::createPlasmaVirtualDesktopManager(QObject* parent)
{
    return new PlasmaVirtualDesktopManager(this, parent);
}

Viewporter* D_isplay::createViewporter(QObject* parent)
{
    return new Viewporter(this, parent);
}

XdgOutputManager* D_isplay::createXdgOutputManager(QObject* parent)
{
    return new XdgOutputManager(this, parent);
}

XdgDecorationManager* D_isplay::createXdgDecorationManager(XdgShell* shell, QObject* parent)
{
    return new XdgDecorationManager(this, shell, parent);
}

EglStreamController* D_isplay::createEglStreamController(QObject* parent)
{
    return new EglStreamController(this, parent);
}

KeyState* D_isplay::createKeyState(QObject* parent)
{
    return new KeyState(this, parent);
}

void D_isplay::createShm()
{
    Q_ASSERT(d_ptr->native());
    wl_display_init_shm(d_ptr->native());
}

quint32 D_isplay::nextSerial()
{
    return wl_display_next_serial(d_ptr->native());
}

quint32 D_isplay::serial()
{
    return wl_display_get_serial(d_ptr->native());
}

bool D_isplay::running() const
{
    return d_ptr->running();
}

wl_display* D_isplay::native() const
{
    return d_ptr->native();
}

std::vector<Output*>& D_isplay::outputs() const
{
    return d_ptr->outputs;
}

std::vector<Seat*>& D_isplay::seats() const
{
    return d_ptr->seats;
}

Client* D_isplay::getClient(wl_client* wlClient)
{
    return d_ptr->createClientHandle(wlClient);
}

std::vector<Client*> D_isplay::clients() const
{
    std::vector<Client*> ret;
    for (auto* client : d_ptr->clients()) {
        ret.push_back(client->handle());
    }
    return ret;
}

Client* D_isplay::createClient(int fd)
{
    return getClient(d_ptr->createClient(fd));
}

void D_isplay::setEglDisplay(void* display)
{
    if (d_ptr->eglDisplay != EGL_NO_DISPLAY) {
        qCWarning(WRAPLAND_SERVER) << "EGLDisplay cannot be changed";
        return;
    }
    d_ptr->eglDisplay = (EGLDisplay)display;
}

void* D_isplay::eglDisplay() const
{
    return d_ptr->eglDisplay;
}

}
