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

#include "dpms.h"
#include "output.h"
#include "seat.h"

// Legacy
#include "../src/server/display.h"

#include "../src/server/appmenu_interface.h"
#include "../src/server/blur_interface.h"
#include "../src/server/compositor_interface.h"
#include "../src/server/contrast_interface.h"
#include "../src/server/datadevicemanager_interface.h"
#include "../src/server/eglstream_controller_interface.h"
#include "../src/server/fakeinput_interface.h"
#include "../src/server/idle_interface.h"
#include "../src/server/idleinhibit_interface_p.h"
#include "../src/server/keystate_interface.h"
#include "../src/server/linuxdmabuf_v1_interface.h"
#include "../src/server/output_configuration_v1_interface.h"
#include "../src/server/output_device_v1_interface.h"
#include "../src/server/output_interface.h"
#include "../src/server/output_management_v1_interface.h"
#include "../src/server/plasmashell_interface.h"
#include "../src/server/plasmavirtualdesktop_interface.h"
#include "../src/server/plasmawindowmanagement_interface.h"
#include "../src/server/pointerconstraints_interface_p.h"
#include "../src/server/pointergestures_interface_p.h"
#include "../src/server/qtsurfaceextension_interface.h"
#include "../src/server/relativepointer_interface_p.h"
#include "../src/server/remote_access_interface.h"
#include "../src/server/server_decoration_interface.h"
#include "../src/server/server_decoration_palette_interface.h"
#include "../src/server/shadow_interface.h"
#include "../src/server/shell_interface.h"
#include "../src/server/slide_interface.h"
#include "../src/server/subcompositor_interface.h"
#include "../src/server/textinput_interface_p.h"
#include "../src/server/viewporter_interface.h"
#include "../src/server/xdgdecoration_interface.h"
#include "../src/server/xdgforeign_interface.h"
#include "../src/server/xdgoutput_interface.h"
#include "../src/server/xdgshell_stable_interface_p.h"
#include "../src/server/xdgshell_v5_interface_p.h"
#include "../src/server/xdgshell_v6_interface_p.h"
//
//

#include "logging.h"

#include "display_p.h"

#include <EGL/egl.h>

#include <algorithm>
#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{

Private* Private::castDisplay(D_isplay* display)
{
    return display->d_ptr;
}

Wayland::Client* Private::castClientImpl(Server::Client* client)
{
    return client->d_ptr;
}

Client* Private::createClientHandle(wl_client* wlClient)
{
    if (auto* client = getClient(wlClient)) {
        return client->handle();
    }
    auto* clientHandle = new Client(wlClient, q_ptr);
    setupClient(clientHandle->d_ptr);
    return clientHandle;
}

D_isplay::D_isplay(QObject* parent, bool legacyInvoked)
    : QObject(parent)
    , d_ptr(new Private(this))
{
    if (!legacyInvoked) {
        legacy = new Server::Display(nullptr, true);
        legacy->newDisplay = this;
        deleteLegacy = true;
    }
}

D_isplay::~D_isplay()
{
    delete d_ptr;

    if (deleteLegacy) {
        legacy->newDisplay = nullptr;
        delete legacy;
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

void D_isplay::removeOutputDevice(OutputDeviceV1Interface* outputDevice)
{
    legacy->removeOutputDevice(outputDevice);
}

CompositorInterface* D_isplay::createCompositor(QObject* parent)
{
    return legacy->createCompositor(parent);
}

ShellInterface* D_isplay::createShell(QObject* parent)
{
    return legacy->createShell(parent);
}

OutputDeviceV1Interface* D_isplay::createOutputDeviceV1(QObject* parent)
{
    return legacy->createOutputDeviceV1(parent);
}

OutputManagementV1Interface* D_isplay::createOutputManagementV1(QObject* parent)
{
    return legacy->createOutputManagementV1(parent);
}

Seat* D_isplay::createSeat(QObject* parent)
{
    auto seat = new Seat(this, parent);
    d_ptr->seats.push_back(seat);
    return seat;
}

SubCompositorInterface* D_isplay::createSubCompositor(QObject* parent)
{
    return legacy->createSubCompositor(parent);
}

DataDeviceManagerInterface* D_isplay::createDataDeviceManager(QObject* parent)
{
    return legacy->createDataDeviceManager(parent);
}

PlasmaShellInterface* D_isplay::createPlasmaShell(QObject* parent)
{
    return legacy->createPlasmaShell(parent);
}

PlasmaWindowManagementInterface* D_isplay::createPlasmaWindowManagement(QObject* parent)
{
    return legacy->createPlasmaWindowManagement(parent);
}

QtSurfaceExtensionInterface* D_isplay::createQtSurfaceExtension(QObject* parent)
{
    return legacy->createQtSurfaceExtension(parent);
}

RemoteAccessManagerInterface* D_isplay::createRemoteAccessManager(QObject* parent)
{
    return legacy->createRemoteAccessManager(parent);
}

IdleInterface* D_isplay::createIdle(QObject* parent)
{
    return legacy->createIdle(parent);
}

FakeInputInterface* D_isplay::createFakeInput(QObject* parent)
{
    return legacy->createFakeInput(parent);
}

ShadowManagerInterface* D_isplay::createShadowManager(QObject* parent)
{
    return legacy->createShadowManager(parent);
}

BlurManagerInterface* D_isplay::createBlurManager(QObject* parent)
{
    return legacy->createBlurManager(parent);
}

ContrastManagerInterface* D_isplay::createContrastManager(QObject* parent)
{
    return legacy->createContrastManager(parent);
}

SlideManagerInterface* D_isplay::createSlideManager(QObject* parent)
{
    return legacy->createSlideManager(parent);
}

DpmsManager* D_isplay::createDpmsManager(QObject* parent)
{
    return new DpmsManager(this, parent);
}

ServerSideDecorationManagerInterface* D_isplay::createServerSideDecorationManager(QObject* parent)
{
    return legacy->createServerSideDecorationManager(parent);
}

TextInputManagerInterface*
D_isplay::createTextInputManager(const TextInputInterfaceVersion& version, QObject* parent)
{
    return legacy->createTextInputManager(version, parent);
}

XdgShellInterface* D_isplay::createXdgShell(const XdgShellInterfaceVersion& version,
                                            QObject* parent)
{
    return legacy->createXdgShell(version, parent);
}

RelativePointerManagerInterface*
D_isplay::createRelativePointerManager(const RelativePointerInterfaceVersion& version,
                                       QObject* parent)
{
    return legacy->createRelativePointerManager(version, parent);
}

PointerGesturesInterface*
D_isplay::createPointerGestures(const PointerGesturesInterfaceVersion& version, QObject* parent)
{
    return legacy->createPointerGestures(version, parent);
}

PointerConstraintsInterface*
D_isplay::createPointerConstraints(const PointerConstraintsInterfaceVersion& version,
                                   QObject* parent)
{
    return legacy->createPointerConstraints(version, parent);
}

XdgForeignInterface* D_isplay::createXdgForeignInterface(QObject* parent)
{
    return legacy->createXdgForeignInterface(parent);
}

IdleInhibitManagerInterface*
D_isplay::createIdleInhibitManager(const IdleInhibitManagerInterfaceVersion& version,
                                   QObject* parent)
{
    return legacy->createIdleInhibitManager(version, parent);
}

AppMenuManagerInterface* D_isplay::createAppMenuManagerInterface(QObject* parent)
{
    return legacy->createAppMenuManagerInterface(parent);
}

ServerSideDecorationPaletteManagerInterface*
D_isplay::createServerSideDecorationPaletteManager(QObject* parent)
{
    return legacy->createServerSideDecorationPaletteManager(parent);
}

LinuxDmabufUnstableV1Interface* D_isplay::createLinuxDmabufInterface(QObject* parent)
{
    return legacy->createLinuxDmabufInterface(parent);
}

PlasmaVirtualDesktopManagementInterface*
D_isplay::createPlasmaVirtualDesktopManagement(QObject* parent)
{
    return legacy->createPlasmaVirtualDesktopManagement(parent);
}

ViewporterInterface* D_isplay::createViewporterInterface(QObject* parent)
{
    return legacy->createViewporterInterface(parent);
}

XdgOutputManagerInterface* D_isplay::createXdgOutputManager(QObject* parent)
{
    return legacy->createXdgOutputManager(parent);
}

XdgDecorationManagerInterface*
D_isplay::createXdgDecorationManager(XdgShellInterface* shellInterface, QObject* parent)
{
    return legacy->createXdgDecorationManager(shellInterface, parent);
}

EglStreamControllerInterface* D_isplay::createEglStreamControllerInterface(QObject* parent)
{
    return legacy->createEglStreamControllerInterface(parent);
}

KeyStateInterface* D_isplay::createKeyStateInterface(QObject* parent)
{
    return legacy->createKeyStateInterface(parent);
}

void D_isplay::createShm()
{
    Q_ASSERT(d_ptr->display());
    wl_display_init_shm(d_ptr->display());
}

quint32 D_isplay::nextSerial()
{
    return wl_display_next_serial(d_ptr->display());
}

quint32 D_isplay::serial()
{
    return wl_display_get_serial(d_ptr->display());
}

bool D_isplay::running() const
{
    return d_ptr->running();
}

wl_display* D_isplay::display() const
{
    return d_ptr->display();
}

D_isplay::operator wl_display*()
{
    return d_ptr->display();
}

D_isplay::operator wl_display*() const
{
    return d_ptr->display();
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
}
