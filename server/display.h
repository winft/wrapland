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
namespace Wayland
{
class Display;
}

class Client;
class Display;

class Output;

//
// Legacy
class ClientConnection;

class CompositorInterface;
class DataDeviceManagerInterface;
class DpmsManagerInterface;
class IdleInterface;
enum class IdleInhibitManagerInterfaceVersion;
class RemoteAccessManagerInterface;
class IdleInhibitManagerInterface;
class FakeInputInterface;
class PlasmaShellInterface;
class PlasmaWindowManagementInterface;
class QtSurfaceExtensionInterface;
class SeatInterface;
class ShadowManagerInterface;
class BlurManagerInterface;
class ContrastManagerInterface;
class OutputConfigurationV1Interface;
class OutputDeviceV1Interface;
class OutputManagementV1Interface;
class ServerSideDecorationManagerInterface;
class SlideManagerInterface;
class ShellInterface;
class SubCompositorInterface;
enum class TextInputInterfaceVersion;
class TextInputManagerInterface;
class XdgShellV5Interface;
enum class XdgShellInterfaceVersion;
class XdgShellInterface;
enum class RelativePointerInterfaceVersion;
class RelativePointerManagerInterface;
enum class PointerGesturesInterfaceVersion;
class PointerGesturesInterface;
enum class PointerConstraintsInterfaceVersion;
class PointerConstraintsInterface;
class XdgForeignInterface;
class AppMenuManagerInterface;
class ServerSideDecorationPaletteManagerInterface;
class PlasmaVirtualDesktopManagementInterface;
class ViewporterInterface;
class XdgOutputManagerInterface;
class XdgDecorationManagerInterface;
class EglStreamControllerInterface;
class KeyStateInterface;
class LinuxDmabufUnstableV1Interface;
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

    //
    // Legacy
    void setSocketName(const QString& name);

    OutputDeviceV1Interface* createOutputDeviceV1(QObject* parent = nullptr);
    void removeOutputDevice(OutputDeviceV1Interface* outputDevice);
    QList<OutputDeviceV1Interface*> outputDevices() const;

    CompositorInterface* createCompositor(QObject* parent = nullptr);
    void createShm();
    ShellInterface* createShell(QObject* parent = nullptr);
    SeatInterface* createSeat(QObject* parent = nullptr);

    QVector<SeatInterface*> seats() const;
    SubCompositorInterface* createSubCompositor(QObject* parent = nullptr);
    DataDeviceManagerInterface* createDataDeviceManager(QObject* parent = nullptr);
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
    DpmsManagerInterface* createDpmsManager(QObject* parent = nullptr);

    KeyStateInterface* createKeyStateInterface(QObject* parent = nullptr);

    ServerSideDecorationManagerInterface* createServerSideDecorationManager(QObject* parent
                                                                            = nullptr);
    TextInputManagerInterface* createTextInputManager(const TextInputInterfaceVersion& version,
                                                      QObject* parent = nullptr);
    XdgShellInterface* createXdgShell(const XdgShellInterfaceVersion& version,
                                      QObject* parent = nullptr);
    RelativePointerManagerInterface*
    createRelativePointerManager(const RelativePointerInterfaceVersion& version,
                                 QObject* parent = nullptr);
    PointerGesturesInterface* createPointerGestures(const PointerGesturesInterfaceVersion& version,
                                                    QObject* parent = nullptr);
    PointerConstraintsInterface*
    createPointerConstraints(const PointerConstraintsInterfaceVersion& version,
                             QObject* parent = nullptr);
    XdgForeignInterface* createXdgForeignInterface(QObject* parent = nullptr);

    IdleInhibitManagerInterface*
    createIdleInhibitManager(const IdleInhibitManagerInterfaceVersion& version,
                             QObject* parent = nullptr);
    AppMenuManagerInterface* createAppMenuManagerInterface(QObject* parent = nullptr);

    ServerSideDecorationPaletteManagerInterface*
    createServerSideDecorationPaletteManager(QObject* parent = nullptr);
    LinuxDmabufUnstableV1Interface* createLinuxDmabufInterface(QObject* parent = nullptr);
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
    Client* createClientInternal(wl_client* wlClient);

    friend class Wayland::Display;
    class Private;
    Private* d_ptr;
};

}
}
