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

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
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
class idle_notifier_v1;
class input_method_manager_v2;
class kde_idle;
class KeyboardShortcutsInhibitManagerV1;
class KeyState;
class LayerShellV1;
class linux_dmabuf_v1;
class output;
class plasma_activation_feedback;
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
class wlr_output_manager_v1;
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

    void start();
    void terminate();

    void startLoop();

    void dispatchEvents(int msecTimeout = -1);

    wl_display* native() const;
    bool running() const;

    void createShm();

    void dispatch();
    void flush();

    Client* getClient(wl_client* client) const;
    std::vector<Client*> clients() const;

    Client* createClient(wl_client* client);
    Client* createClient(int fd);

    void setEglDisplay(void* display);
    void* eglDisplay() const;

    struct {
        /// Basic graphical operations
        Server::Compositor* compositor{nullptr};
        Server::Subcompositor* subcompositor{nullptr};
        Server::linux_dmabuf_v1* linux_dmabuf_v1{nullptr};
        Server::Viewporter* viewporter{nullptr};
        Server::PresentationManager* presentation_manager{nullptr};

        /// Additional graphical effects
        Server::ShadowManager* shadow_manager{nullptr};
        Server::BlurManager* blur_manager{nullptr};
        Server::SlideManager* slide_manager{nullptr};
        Server::ContrastManager* contrast_manager{nullptr};

        /// Graphical output management
        Server::XdgOutputManager* xdg_output_manager{nullptr};
        Server::DpmsManager* dpms_manager{nullptr};
        Server::wlr_output_manager_v1* wlr_output_manager_v1{nullptr};

        /// Basic input
        std::vector<Server::Seat*> seats;
        Server::RelativePointerManagerV1* relative_pointer_manager_v1{nullptr};
        Server::PointerGesturesV1* pointer_gestures_v1{nullptr};
        Server::PointerConstraintsV1* pointer_constraints_v1{nullptr};

        /// Input method support
        Server::text_input_manager_v2* text_input_manager_v2{nullptr};
        Server::text_input_manager_v3* text_input_manager_v3{nullptr};
        Server::input_method_manager_v2* input_method_manager_v2{nullptr};

        /// Global input state manipulation
        Server::KeyState* key_state{nullptr};
        Server::FakeInput* fake_input{nullptr};
        Server::KeyboardShortcutsInhibitManagerV1* keyboard_shortcuts_inhibit_manager_v1{nullptr};
        Server::virtual_keyboard_manager_v1* virtual_keyboard_manager_v1{nullptr};

        /// Data sharing between clients
        Server::data_device_manager* data_device_manager{nullptr};
        Server::primary_selection_device_manager* primary_selection_device_manager{nullptr};
        Server::data_control_manager_v1* data_control_manager_v1{nullptr};

        /// Individual windowing control
        Server::XdgShell* xdg_shell{nullptr};
        Server::XdgForeign* xdg_foreign{nullptr};
        Server::XdgActivationV1* xdg_activation_v1{nullptr};

        /// Individual window styling and functionality
        Server::AppmenuManager* appmenu_manager{nullptr};
        Server::XdgDecorationManager* xdg_decoration_manager{nullptr};
        Server::ServerSideDecorationPaletteManager* server_side_decoration_palette_manager{nullptr};

        /// Global windowing control
        Server::LayerShellV1* layer_shell_v1{nullptr};
        Server::plasma_activation_feedback* plasma_activation_feedback{nullptr};
        Server::PlasmaVirtualDesktopManager* plasma_virtual_desktop_manager{nullptr};
        Server::PlasmaShell* plasma_shell{nullptr};
        Server::PlasmaWindowManager* plasma_window_manager{nullptr};

        /// Hardware take-over and blocking
        Server::idle_notifier_v1* idle_notifier_v1{nullptr};
        Server::kde_idle* kde_idle{nullptr};
        Server::drm_lease_device_v1* drm_lease_device_v1{nullptr};
        Server::IdleInhibitManagerV1* idle_inhibit_manager_v1{nullptr};
    } globals;

Q_SIGNALS:
    void started();
    void clientConnected(Wrapland::Server::Client*);
    void clientDisconnected(Wrapland::Server::Client*);

private:
    friend class Private;
    std::unique_ptr<Private> d_ptr;
};

}
