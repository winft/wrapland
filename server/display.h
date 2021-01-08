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

#include <Wrapland/Server/wraplandserver_export.h>

#include <cstdint>
#include <memory>
#include <string>

struct wl_client;
struct wl_display;

namespace Wrapland::Server
{

class Client;
class Private;

class DataDeviceManager;
class DpmsManager;
class WlOutput;
class OutputConfigurationV1;
class OutputDeviceV1;
class OutputManagementV1;
class PlasmaVirtualDesktopManager;
class PlasmaWindowManager;
class PresentationManager;
class Seat;
class XdgShell;

class Compositor;
class KdeIdle;
class IdleInhibitManagerV1;
class FakeInput;
class PlasmaShell;
class ShadowManager;
class BlurManager;
class ContrastManager;
class SlideManager;
class Subcompositor;
class TextInputManagerV2;
class RelativePointerManagerV1;
class PointerGesturesV1;
class PointerConstraintsV1;
class XdgForeign;
class AppmenuManager;
class ServerSideDecorationPaletteManager;
class Viewporter;
class XdgOutputManager;
class XdgDecorationManager;
class EglStreamController;
class KeyState;
class LinuxDmabufV1;
class KeyboardShortcutsInhibitManagerV1;

class WRAPLANDSERVER_EXPORT Display : public QObject
{
    Q_OBJECT
public:
    explicit Display(QObject* parent = nullptr);

    ~Display() override;

    void setSocketName(const std::string& name);
    std::string socketName() const;

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

    wl_display* native() const;
    bool running() const;

    void add_wl_output(WlOutput* output);
    void removeOutput(WlOutput* output);
    std::vector<WlOutput*>& outputs() const;

    Seat* createSeat(QObject* parent = nullptr);
    std::vector<Seat*>& seats() const;

    void setSocketName(const QString& name);
    void add_socket_fd(int fd);

    void add_output_device_v1(OutputDeviceV1* output);
    void removeOutputDevice(OutputDeviceV1* outputDevice);
    std::vector<OutputDeviceV1*> outputDevices() const;

    Compositor* createCompositor(QObject* parent = nullptr);
    void createShm();

    Subcompositor* createSubCompositor(QObject* parent = nullptr);
    DataDeviceManager* createDataDeviceManager(QObject* parent = nullptr);
    OutputManagementV1* createOutputManagementV1(QObject* parent = nullptr);
    PlasmaShell* createPlasmaShell(QObject* parent = nullptr);
    PlasmaWindowManager* createPlasmaWindowManager(QObject* parent = nullptr);
    KdeIdle* createIdle(QObject* parent = nullptr);
    FakeInput* createFakeInput(QObject* parent = nullptr);
    ShadowManager* createShadowManager(QObject* parent = nullptr);
    BlurManager* createBlurManager(QObject* parent = nullptr);
    SlideManager* createSlideManager(QObject* parent = nullptr);
    ContrastManager* createContrastManager(QObject* parent = nullptr);
    DpmsManager* createDpmsManager(QObject* parent = nullptr);

    KeyState* createKeyState(QObject* parent = nullptr);
    PresentationManager* createPresentationManager(QObject* parent = nullptr);

    TextInputManagerV2* createTextInputManager(QObject* parent = nullptr);

    XdgShell* createXdgShell(QObject* parent = nullptr);

    RelativePointerManagerV1* createRelativePointerManager(QObject* parent = nullptr);
    PointerGesturesV1* createPointerGestures(QObject* parent = nullptr);
    PointerConstraintsV1* createPointerConstraints(QObject* parent = nullptr);

    XdgForeign* createXdgForeign(QObject* parent = nullptr);

    IdleInhibitManagerV1* createIdleInhibitManager(QObject* parent = nullptr);
    KeyboardShortcutsInhibitManagerV1* createKeyboardShortcutsInhibitManager(QObject* parent
                                                                             = nullptr);
    AppmenuManager* createAppmenuManager(QObject* parent = nullptr);

    ServerSideDecorationPaletteManager* createServerSideDecorationPaletteManager(QObject* parent
                                                                                 = nullptr);
    LinuxDmabufV1* createLinuxDmabuf(QObject* parent = nullptr);
    Viewporter* createViewporter(QObject* parent = nullptr);
    XdgOutputManager* xdgOutputManager() const;

    PlasmaVirtualDesktopManager* createPlasmaVirtualDesktopManager(QObject* parent = nullptr);
    XdgDecorationManager* createXdgDecorationManager(XdgShell* shell, QObject* parent = nullptr);
    EglStreamController* createEglStreamController(QObject* parent = nullptr);

    void dispatch();
    void flush();

    Client* createClient(int fd);
    Client* getClient(wl_client* client);
    std::vector<Client*> clients() const;

    void setEglDisplay(void* display);

    void* eglDisplay() const;

Q_SIGNALS:
    void started();
    void clientConnected(Wrapland::Server::Client*);
    void clientDisconnected(Wrapland::Server::Client*);

private:
    friend class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(uint32_t)
