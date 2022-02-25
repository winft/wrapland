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
#include <vector>

struct wl_client;
struct wl_display;

namespace Wrapland::Server
{

class Client;
class Private;
class OutputDeviceV1;
class WlOutput;

class AppmenuManager;
class BlurManager;
class Compositor;
class ContrastManager;
class data_control_manager_v1;
class data_device_manager;
class DpmsManager;
class drm_lease_device_v1;
class FakeInput;
class IdleInhibitManagerV1;
class input_method_manager_v2;
class KdeIdle;
class KeyboardShortcutsInhibitManagerV1;
class KeyState;
class LayerShellV1;
class linux_dmabuf_v1;
class Output;
class OutputManagementV1;
class PlasmaShell;
class PlasmaVirtualDesktopManager;
class PlasmaWindowManager;
class PointerConstraintsV1;
class PointerGesturesV1;
class PresentationManager;
class primary_selection_device_manager;
class RelativePointerManagerV1;
class Seat;
class ServerSideDecorationPaletteManager;
class ShadowManager;
class SlideManager;
class Subcompositor;
class text_input_manager_v2;
class text_input_manager_v3;
class Viewporter;
class virtual_keyboard_manager_v1;
class XdgActivationV1;
class XdgDecorationManager;
class XdgForeign;
class XdgOutputManager;
class XdgShell;

class WRAPLANDSERVER_EXPORT Display : public QObject
{
    Q_OBJECT
public:
    Display();
    ~Display() override;

    void set_socket_name(std::string const& name);
    std::string socket_name() const;

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

    std::unique_ptr<Seat> createSeat();
    std::vector<Seat*>& seats() const;

    void add_socket_fd(int fd);

    void add_output_device_v1(OutputDeviceV1* output);
    void removeOutputDevice(OutputDeviceV1* outputDevice);
    std::vector<OutputDeviceV1*> outputDevices() const;

    std::unique_ptr<Compositor> createCompositor();
    void createShm();

    std::unique_ptr<Subcompositor> createSubCompositor();

    std::unique_ptr<data_control_manager_v1> create_data_control_manager_v1();
    std::unique_ptr<data_device_manager> createDataDeviceManager();

    std::unique_ptr<OutputManagementV1> createOutputManagementV1();
    std::unique_ptr<PlasmaShell> createPlasmaShell();
    std::unique_ptr<PlasmaWindowManager> createPlasmaWindowManager();
    std::unique_ptr<primary_selection_device_manager> createPrimarySelectionDeviceManager();
    std::unique_ptr<KdeIdle> createIdle();
    std::unique_ptr<FakeInput> createFakeInput();
    std::unique_ptr<LayerShellV1> createLayerShellV1();
    std::unique_ptr<ShadowManager> createShadowManager();
    std::unique_ptr<BlurManager> createBlurManager();
    std::unique_ptr<SlideManager> createSlideManager();
    std::unique_ptr<ContrastManager> createContrastManager();
    std::unique_ptr<DpmsManager> createDpmsManager();
    std::unique_ptr<drm_lease_device_v1> createDrmLeaseDeviceV1();

    std::unique_ptr<KeyState> createKeyState();
    std::unique_ptr<PresentationManager> createPresentationManager();

    std::unique_ptr<text_input_manager_v2> createTextInputManagerV2();
    std::unique_ptr<text_input_manager_v3> createTextInputManagerV3();
    std::unique_ptr<input_method_manager_v2> createInputMethodManagerV2();

    std::unique_ptr<XdgShell> createXdgShell();

    std::unique_ptr<RelativePointerManagerV1> createRelativePointerManager();
    std::unique_ptr<PointerGesturesV1> createPointerGestures();
    std::unique_ptr<PointerConstraintsV1> createPointerConstraints();

    std::unique_ptr<XdgForeign> createXdgForeign();

    std::unique_ptr<IdleInhibitManagerV1> createIdleInhibitManager();
    std::unique_ptr<KeyboardShortcutsInhibitManagerV1> createKeyboardShortcutsInhibitManager();
    std::unique_ptr<AppmenuManager> createAppmenuManager();

    std::unique_ptr<ServerSideDecorationPaletteManager> createServerSideDecorationPaletteManager();
    std::unique_ptr<Viewporter> createViewporter();
    std::unique_ptr<virtual_keyboard_manager_v1> create_virtual_keyboard_manager_v1();
    XdgOutputManager* xdgOutputManager() const;

    std::unique_ptr<PlasmaVirtualDesktopManager> createPlasmaVirtualDesktopManager();
    std::unique_ptr<XdgActivationV1> createXdgActivationV1();
    std::unique_ptr<XdgDecorationManager> createXdgDecorationManager(XdgShell* shell);

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
