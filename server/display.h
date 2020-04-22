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
#pragma once

#include <QObject>

#include <QDebug>

//
// Legacy
#include <QVector>
//
//

#include <Wrapland/Server/wraplandserver_export.h>

#include <cstdint>
#include <string>

struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_resource;

namespace Wrapland
{
namespace Server
{

class Client;
class Display;
class Private;

class DataDeviceManager;
class DpmsManager;
class Output;
class Seat;

//
// Legacy
class ClientConnection;

class Compositor;
class IdleInterface;
enum class IdleInhibitManagerInterfaceVersion;
class RemoteAccessManagerInterface;
class IdleInhibitManagerInterface;
class FakeInputInterface;
class PlasmaShellInterface;
class PlasmaWindowManagementInterface;
class QtSurfaceExtensionInterface;
class ShadowManagerInterface;
class BlurManagerInterface;
class ContrastManagerInterface;
class OutputConfigurationV1Interface;
class OutputDeviceV1Interface;
class OutputManagementV1Interface;
class ServerSideDecorationManagerInterface;
class SlideManagerInterface;
class ShellInterface;
class Subcompositor;
enum class TextInputInterfaceVersion;
class TextInputManagerInterface;
class XdgShellV5Interface;
enum class XdgShellInterfaceVersion;
class XdgShellInterface;
class RelativePointerManagerV1;
class PointerGesturesV1;
class PointerConstraintsV1;
class XdgForeign;
class AppMenuManagerInterface;
class ServerSideDecorationPaletteManagerInterface;
class PlasmaVirtualDesktopManagementInterface;
class ViewporterInterface;
class XdgOutputManagerInterface;
class XdgDecorationManagerInterface;
class EglStreamControllerInterface;
class KeyState;
class LinuxDmabufV1;
//
//

class WRAPLANDSERVER_EXPORT D_isplay : public QObject
{
    Q_OBJECT
public:
    explicit D_isplay(QObject* parent = nullptr, bool legacyInvoked = false);
    virtual ~D_isplay();

    void setSocketName(const std::string& name);
    const std::string socketName() const;

    uint32_t serial();
    uint32_t nextSerial();

    enum class StartMode {
        ConnectToSocket,
        ConnectClientsOnly,
    };

    void start(StartMode mode = StartMode::ConnectToSocket);
    void terminate();

    void startLoop();

    void dispatchEvents(int msecTimeout = -1);

    wl_display* display() const;
    operator wl_display*();
    operator wl_display*() const;
    bool running() const;

    Output* createOutput(QObject* parent = nullptr);
    void removeOutput(Output* output);
    std::vector<Output*>& outputs() const;

    Seat* createSeat(QObject* parent = nullptr);
    std::vector<Seat*>& seats() const;

    //
    // Legacy
    void setSocketName(const QString& name);

    OutputDeviceV1Interface* createOutputDeviceV1(QObject* parent = nullptr);
    void removeOutputDevice(OutputDeviceV1Interface* outputDevice);
    QList<OutputDeviceV1Interface*> outputDevices() const;

    Compositor* createCompositor(QObject* parent = nullptr);
    void createShm();
    ShellInterface* createShell(QObject* parent = nullptr);

    Subcompositor* createSubCompositor(QObject* parent = nullptr);
    DataDeviceManager* createDataDeviceManager(QObject* parent = nullptr);
    OutputManagementV1Interface* createOutputManagementV1(QObject* parent = nullptr);
    PlasmaShellInterface* createPlasmaShell(QObject* parent = nullptr);
    PlasmaWindowManagementInterface* createPlasmaWindowManagement(QObject* parent = nullptr);
    QtSurfaceExtensionInterface* createQtSurfaceExtension(QObject* parent = nullptr);
    IdleInterface* createIdle(QObject* parent = nullptr);
    RemoteAccessManagerInterface* createRemoteAccessManager(QObject* parent = nullptr);
    FakeInputInterface* createFakeInput(QObject* parent = nullptr);
    ShadowManagerInterface* createShadowManager(QObject* parent = nullptr);
    BlurManagerInterface* createBlurManager(QObject* parent = nullptr);
    ContrastManagerInterface* createContrastManager(QObject* parent = nullptr);
    SlideManagerInterface* createSlideManager(QObject* parent = nullptr);
    DpmsManager* createDpmsManager(QObject* parent = nullptr);

    KeyState* createKeyState(QObject* parent = nullptr);

    ServerSideDecorationManagerInterface* createServerSideDecorationManager(QObject* parent
                                                                            = nullptr);
    TextInputManagerInterface* createTextInputManager(const TextInputInterfaceVersion& version,
                                                      QObject* parent = nullptr);
    XdgShellInterface* createXdgShell(const XdgShellInterfaceVersion& version,
                                      QObject* parent = nullptr);

    RelativePointerManagerV1* createRelativePointerManager(QObject* parent = nullptr);
    PointerGesturesV1* createPointerGestures(QObject* parent = nullptr);
    PointerConstraintsV1* createPointerConstraints(QObject* parent = nullptr);

    XdgForeign* createXdgForeign(QObject* parent = nullptr);

    IdleInhibitManagerInterface*
    createIdleInhibitManager(const IdleInhibitManagerInterfaceVersion& version,
                             QObject* parent = nullptr);
    AppMenuManagerInterface* createAppMenuManagerInterface(QObject* parent = nullptr);

    ServerSideDecorationPaletteManagerInterface*
    createServerSideDecorationPaletteManager(QObject* parent = nullptr);
    LinuxDmabufV1* createLinuxDmabuf(QObject* parent = nullptr);
    ViewporterInterface* createViewporterInterface(QObject* parent = nullptr);
    XdgOutputManagerInterface* createXdgOutputManager(QObject* parent = nullptr);

    PlasmaVirtualDesktopManagementInterface* createPlasmaVirtualDesktopManagement(QObject* parent
                                                                                  = nullptr);
    XdgDecorationManagerInterface* createXdgDecorationManager(XdgShellInterface* shellInterface,
                                                              QObject* parent = nullptr);
    EglStreamControllerInterface* createEglStreamControllerInterface(QObject* parent = nullptr);

    Server::ClientConnection* getClientLegacy(wl_client* client);

    Server::Display* legacy = nullptr;
    bool deleteLegacy = false;
    void dispatch();
    void flush();
    //
    //

    Client* createClient(int fd);
    Client* getClient(wl_client* client);
    std::vector<Client*> clients() const;

    void setEglDisplay(void* display);

    void* eglDisplay() const;

Q_SIGNALS:
    void clientConnected(Wrapland::Server::Client*);
    void clientDisconnected(Wrapland::Server::Client*);

private:
    friend class Private;
    Private* d_ptr;
};

}
}
