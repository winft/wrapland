/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2018  David Edmundson <davidedmundson@kde.org>

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
//#include "compositor_interface.h"
//#include "output_configuration_v1_interface.h"
//#include "output_management_v1_interface.h"
//#include "output_device_v1_interface.h"
//#include "idleinhibit_interface_p.h"
//#include "remote_access_interface.h"
#include "fakeinput_interface.h"
#include "logging.h"
//#include "output_interface.h"
#include "plasmashell_interface.h"
//#include "plasmawindowmanagement_interface.h"
//#include "pointerconstraints_interface_p.h"
//#include "pointergestures_interface_p.h"
//#include "seat_interface.h"
//#include "shadow_interface.h"
//#include "blur_interface.h"
//#include "contrast_interface.h"
//#include "relativepointer_interface_p.h"
//#include "slide_interface.h"
//#include "shell_interface.h"
//#include "subcompositor_interface.h"
//#include "textinput_interface_p.h"
//#include "xdgshell_v5_interface_p.h"
//#include "viewporter_interface.h"
//#include "xdgforeign_interface.h"
//#include "xdgshell_v6_interface_p.h"
//#include "xdgshell_stable_interface_p.h"
//#include "appmenu_interface.h"
#include "server_decoration_palette_interface.h"
//#include "plasmavirtualdesktop_interface.h"
//#include "xdgoutput_interfce.h"
//#include "xdgdecoration_interface.h"
//#include "eglstream_controller_interface.h"
//#include "../../server/keystate.h"
//#include "linuxdmabuf_v1_interface.h"

//
// Remodel
#include "../../server/client.h"
#include "../../server/display.h"

#include "../../server/output.h"
//#include "../../server/seat.h"
//
//


#include <QCoreApplication>
#include <QDebug>
#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QThread>

#include <wayland-server.h>

#include <EGL/egl.h>

namespace Wrapland
{
namespace Server
{

class Display::Private
{
public:
    Private(Display *q);
    void flush();
    void dispatch();
    void setRunning(bool running);
    void installSocketNotifier();

    wl_display *display = nullptr;
    wl_event_loop *loop = nullptr;
    QString socketName = QStringLiteral("wayland-0");
    bool running = false;
    bool automaticSocketNaming = false;
//    QList<OutputInterface*> outputs;
//    QList<OutputDeviceV1Interface*> outputDevices;
//    QVector<SeatInterface*> seats;
    QVector<ClientConnection*> clients;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;

private:
    Display *q;
};

Display::Private::Private(Display *q)
    : q(q)
{
}

void Display::Private::installSocketNotifier()
{
    if (!QThread::currentThread()) {
        return;
    }
    int fd = wl_event_loop_get_fd(loop);
    if (fd == -1) {
        qCWarning(WRAPLAND_SERVER) << "Did not get the file descriptor for the event loop";
        return;
    }
    QSocketNotifier *m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, q);
    QObject::connect(m_notifier, &QSocketNotifier::activated, q, [this] { dispatch(); } );
    QObject::connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, q, [this] { flush(); });
    setRunning(true);
}

Display::Display(QObject *parent, bool newInvoked)
    : QObject(parent)
    , d(new Private(this))
{
    if (!newInvoked) {
        newDisplay = new Server::D_isplay(parent, true);
        newDisplay->legacy = this;
    }
}

Display::~Display()
{
    if (newDisplay) {
        newDisplay->deleteLegacy = false;
        delete newDisplay;
    }
}

void Display::Private::flush()
{
    q->newDisplay->flush();
//    if (!display || !loop) {
//        return;
//    }
//    wl_display_flush_clients(display);
}

void Display::Private::dispatch()
{
    q->newDisplay->dispatch();
//    if (!display || !loop) {
//        return;
//    }
//    if (wl_event_loop_dispatch(loop, 0) != 0) {
//        qCWarning(WRAPLAND_SERVER) << "Error on dispatching Wayland event loop";
//    }
}

void Display::setSocketName(const QString &name)
{
    std::string stdName = name.toUtf8().constData();

    if (stdName == newDisplay->socketName()) {
        return;
    }
    newDisplay->setSocketName(stdName);
    Q_EMIT socketNameChanged(name);
}

QString Display::socketName() const
{
    return QString::fromStdString(newDisplay->socketName());
}

void Display::setAutomaticSocketNaming(bool automaticSocketNaming)
{
    Q_UNUSED(automaticSocketNaming);
}

bool Display::automaticSocketNaming() const
{
    return true;
}

void Display::start(StartMode mode)
{
    newDisplay->start(mode == StartMode::ConnectToSocket ?
                          Server::D_isplay::StartMode::ConnectToSocket :
                          Server::D_isplay::StartMode::ConnectClientsOnly);
}

void Display::startLoop()
{
    newDisplay->startLoop();
}

void Display::dispatchEvents(int msecTimeout)
{
    newDisplay->dispatchEvents(msecTimeout);
}

void Display::terminate()
{
    newDisplay->terminate();
}

void Display::Private::setRunning(bool r)
{
//    newDisplay->setRunning(r);
//    Q_ASSERT(running != r);
//    running = r;
//    emit q->runningChanged(running);
}

//OutputInterface *Display::createOutput(QObject *parent)
//{
//    auto* output = newDisplay->createOutput(parent);
//    return output->legacy;
//}

//CompositorInterface *Display::createCompositor(QObject *parent)
//{
//    CompositorInterface *compositor = new CompositorInterface(this, parent);
//    connect(this, &Display::aboutToTerminate, compositor, [this,compositor] { delete compositor; });
//    return compositor;
//}

//ShellInterface *Display::createShell(QObject *parent)
//{
//    ShellInterface *shell = new ShellInterface(this, parent);
//    connect(this, &Display::aboutToTerminate, shell, [this,shell] { delete shell; });
//    return shell;
//}

//OutputDeviceV1Interface *Display::createOutputDeviceV1(QObject *parent)
//{
//    auto *output = new OutputDeviceV1Interface(this, parent);
//    connect(output, &QObject::destroyed, this, [this,output] { d->outputDevices.removeAll(output); });
//    connect(this, &Display::aboutToTerminate, output, [this,output] { removeOutputDevice(output); });
//    d->outputDevices << output;
//    return output;
//}

//OutputManagementV1Interface *Display::createOutputManagementV1(QObject *parent)
//{
//    auto *om = new OutputManagementV1Interface(this, parent);
//    connect(this, &Display::aboutToTerminate, om, [this,om] { delete om; });
//    return om;
//}

//SeatInterface *Display::createSeat(QObject *parent)
//{
//    auto seat = newDisplay->createSeat(parent);
//    auto legacy = seat->legacy;

//    d->seats << legacy;
//    return legacy;
//}

//SubCompositorInterface *Display::createSubCompositor(QObject *parent)
//{
//    auto c = new SubCompositorInterface(this, parent);
//    connect(this, &Display::aboutToTerminate, c, [this,c] { delete c; });
//    return c;
//}

//DataDeviceManagerInterface *Display::createDataDeviceManager(QObject *parent)
//{
//    auto m = new DataDeviceManagerInterface(this, parent);
//    connect(this, &Display::aboutToTerminate, m, [this,m] { delete m; });
//    return m;
//}

PlasmaShellInterface *Display::createPlasmaShell(QObject* parent)
{
    auto s = new PlasmaShellInterface(this, parent);
    connect(this, &Display::aboutToTerminate, s, [this, s] { delete s; });
    return s;
}
/*
PlasmaWindowManagement *Display::createPlasmaWindowManagement(QObject *parent)
{
    auto wm = new PlasmaWindowManagement(this, parent);
    connect(this, &Display::aboutToTerminate, wm, [this, wm] { delete wm; });
    return wm;
}
*/

//RemoteAccessManagerInterface *Display::createRemoteAccessManager(QObject *parent)
//{
//    auto i = new RemoteAccessManagerInterface(this, parent);
//    connect(this, &Display::aboutToTerminate, i, [this, i] { delete i; });
//    return i;
//}
/*
IdleInterface *Display::createIdle(QObject *parent)
{
    auto i = new IdleInterface(this, parent);
    connect(this, &Display::aboutToTerminate, i, [this, i] { delete i; });
    return i;
}
*/
FakeInputInterface *Display::createFakeInput(QObject *parent)
{
    auto i = new FakeInputInterface(this, parent);
    connect(this, &Display::aboutToTerminate, i, [this, i] { delete i; });
    return i;
}
/*
ShadowManagerInterface *Display::createShadowManager(QObject *parent)
{
    auto s = new ShadowManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, s, [this, s] { delete s; });
    return s;
}
*/
/*
BlurManagerInterface *Display::createBlurManager(QObject *parent)
{
    auto b = new BlurManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
    return b;
}
*/
/*
ContrastManagerInterface *Display::createContrastManager(QObject *parent)
{
    auto b = new ContrastManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
    return b;
}
*/
/*
SlideManagerInterface *Display::createSlideManager(QObject *parent)
{
    auto b = new SlideManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
    return b;
}
*/
/*
TextInputManagerInterface *Display::createTextInputManager(const TextInputVersion &version, QObject *parent)
{
    TextInputManagerInterface *t = nullptr;
    switch (version) {
    case TextInputVersion::UnstableV0:
        t = new TextInputManagerUnstableV0Interface(this, parent);
        break;
    case TextInputVersion::UnstableV1:
        // unsupported
        return nullptr;
    case TextInputVersion::UnstableV2:
        t = new TextInputManagerUnstableV2(this, parent);
    }
    connect(this, &Display::aboutToTerminate, t, [t] { delete t; });
    return t;
}
*/
//XdgShellInterface *Display::createXdgShell(const XdgShellInterfaceVersion &version, QObject *parent)
//{
//    XdgShellInterface *x = nullptr;
//    switch (version) {
//    case XdgShellInterfaceVersion::UnstableV5:
//        x = new XdgShellV5Interface(this, parent);
//        break;
//    case XdgShellInterfaceVersion::UnstableV6:
//        x = new XdgShellV6Interface(this, parent);
//        break;
//    case XdgShellInterfaceVersion::Stable:
//        x = new XdgShellStableInterface(this, parent);
//        break;
//    }
//    connect(this, &Display::aboutToTerminate, x, [x] { delete x; });
//    return x;
//}

//RelativePointerManagerInterface *Display::createRelativePointerManager(const RelativePointerInterfaceVersion &version, QObject *parent)
//{
//    return nullptr;
//    return newDisplay->createRelativePointerManager(parent);

//    RelativePointerManagerInterface *r = nullptr;
//    switch (version) {
//    case RelativePointerInterfaceVersion::UnstableV1:
//        r = new RelativePointerManagerUnstableV1Interface(this, parent);
//        break;
//    }
//    connect(this, &Display::aboutToTerminate, r, [r] { delete r; });
//    return r;
//}

//PointerGesturesInterface *Display::createPointerGestures(const PointerGesturesInterfaceVersion &version, QObject *parent)
//{
//    return nullptr;
//    return newDisplay->createPointerGestures(parent)->legacy;

//    PointerGesturesInterface *p = nullptr;
//    switch (version) {
//    case PointerGesturesInterfaceVersion::UnstableV1:
//        p = new PointerGesturesUnstableV1Interface(this, parent);
//        break;
//    }
//    connect(this, &Display::aboutToTerminate, p, [p] { delete p; });
//    return p;
//}

//PointerConstraintsInterface *Display::createPointerConstraints(const PointerConstraintsInterfaceVersion &version, QObject *parent)
//{
//    return nullptr;
//    return newDisplay->createPointerConstraints(parent);

//    PointerConstraintsInterface *p = nullptr;
//    switch (version) {
//    case PointerConstraintsInterfaceVersion::UnstableV1:
//        p = new PointerConstraintsUnstableV1Interface(this, parent);
//        break;
//    }
//    connect(this, &Display::aboutToTerminate, p, [p] { delete p; });
//    return p;
//}

//XdgForeignInterface *Display::createXdgForeignInterface(QObject *parent)
//{
//    XdgForeignInterface *foreign = new XdgForeignInterface(this, parent);
//    connect(this, &Display::aboutToTerminate, foreign, [this,foreign] { delete foreign; });
//    return foreign;
//}
/*
IdleInhibitManager *Display::createIdleInhibitManager(const IdleInhibitManagerInterfaceVersion &version, QObject *parent)
{
    IdleInhibitManager *i = nullptr;
    switch (version) {
    case IdleInhibitManagerInterfaceVersion::UnstableV1:
        i = new IdleInhibitManagerV1(this, parent);
        break;
    }
    connect(this, &Display::aboutToTerminate, i, [this,i] { delete i; });
    return i;
}
*/
/*
AppMenuManagerInterface *Display::createAppMenuManagerInterface(QObject *parent)
{
    auto b = new AppMenuManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
    return b;
}*/

ServerSideDecorationPaletteManagerInterface *Display::createServerSideDecorationPaletteManager(QObject *parent)
{
    auto b = new ServerSideDecorationPaletteManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
    return b;
}

//LinuxDmabufUnstableV1Interface *Display::createLinuxDmabufInterface(QObject *parent)
//{
//    auto b = new LinuxDmabufUnstableV1Interface(this, parent);
//    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
//    return b;
//}
/*
PlasmaVirtualDesktopManagementInterface *Display::createPlasmaVirtualDesktopManagement(QObject *parent)
{
    auto b = new PlasmaVirtualDesktopManagementInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
    return b;
}*/
/*
ViewporterInterface *Display::createViewporterInterface(QObject *parent)
{
    auto b = new ViewporterInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
    return b;
}*/
/*
XdgOutputManagerInterface *Display::createXdgOutputManager(QObject *parent)
{
    auto b = new XdgOutputManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [this, b] { delete b; });
    return b;
}
*/

//XdgDecorationManagerInterface *Display::createXdgDecorationManager(XdgShellInterface *shellInterface, QObject *parent)
//{
//    auto d = new XdgDecorationManagerInterface(this, shellInterface, parent);
//    connect(this, &Display::aboutToTerminate, d, [d] { delete d; });
//    return d;
//}
/*
EglStreamControllerInterface *Display::createEglStreamControllerInterface(QObject *parent)
{
    EglStreamControllerInterface *e = new EglStreamControllerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, e, [e] { delete e; });
    return e;
}*/
/*
KeyState *Display::createKeyState(QObject *parent)
{
    auto d = new KeyState(this, parent);
    connect(this, &Display::aboutToTerminate, d, [d] { delete d; });
    return d;
}
*/
void Display::createShm()
{
    newDisplay->createShm();
}

//void Display::removeOutput(OutputInterface *output)
//{
//    newDisplay->removeOutput(output->newOutput);
//}

//void Display::removeOutputDevice(OutputDeviceV1Interface *outputDevice)
//{
//    d->outputDevices.removeAll(outputDevice);
//    delete outputDevice;
//}

quint32 Display::nextSerial()
{
    return newDisplay->nextSerial();
}

quint32 Display::serial()
{
    return newDisplay->serial();
}

bool Display::isRunning() const
{
    return newDisplay->running();
}

Display::operator wl_display*()
{
    return newDisplay->display();
}

Display::operator wl_display*() const
{
    return newDisplay->display();
}

//QList<OutputInterface*> Display::outputs() const
//{
//    return d->outputs;
//}

//QList<OutputDeviceV1Interface*> Display::outputDevices() const
//{
//    return d->outputDevices;
//}

//QVector<SeatInterface*> Display::seats() const
//{
//    return d->seats;
//}

ClientConnection *Display::getConnection(wl_client *client)
{
    Server::Client* c = newDisplay->getClient(client);
    return c->legacy;
}

QVector< ClientConnection* > Display::connections() const
{
    QVector< ClientConnection* > ret;
    for (auto* client : newDisplay->clients()) {
        ret << client->legacy;
    }
    return ret;
}

ClientConnection *Display::createClient(int fd)
{
    Server::Client* client = newDisplay->createClient(fd);
    client->legacy = getConnection(client->client());
    client->legacy->newClient = client;
    return client->legacy;
}

void Display::setEglDisplay(void *display)
{
    newDisplay->setEglDisplay(display);
}

void *Display::eglDisplay() const
{
    return newDisplay->eglDisplay();
}

}
}
