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
#include "registry.h"
#include "appmenu.h"
#include "blur.h"
#include "compositor.h"
#include "connection_thread.h"
#include "contrast.h"
#include "data_control_v1.h"
#include "datadevicemanager.h"
#include "dpms.h"
#include "drm_lease_v1_p.h"
#include "event_queue.h"
#include "fakeinput.h"
#include "fullscreen_shell.h"
#include "idle.h"
#include "idle_notify_v1.h"
#include "idleinhibit.h"
#include "input_method_v2.h"
#include "keyboard_shortcuts_inhibit.h"
#include "keystate.h"
#include "layer_shell_v1.h"
#include "linux_dmabuf_v1.h"
#include "logging.h"
#include "output.h"
#include "plasma_activation_feedback.h"
#include "plasmashell.h"
#include "plasmavirtualdesktop.h"
#include "plasmawindowmanagement.h"
#include "pointerconstraints.h"
#include "pointergestures.h"
#include "presentation_time.h"
#include "primary_selection.h"
#include "relativepointer.h"
#include "seat.h"
#include "security_context_v1.h"
#include "server_decoration_palette.h"
#include "shadow.h"
#include "shell.h"
#include "shm_pool.h"
#include "slide.h"
#include "subcompositor.h"
#include "text_input_v2_p.h"
#include "text_input_v3_p.h"
#include "viewporter.h"
#include "virtual_keyboard_v1_p.h"
#include "wayland_pointer_p.h"
#include "wlr_output_manager_v1.h"
#include "xdg_activation_v1.h"
#include "xdg_shell.h"
#include "xdgdecoration.h"
#include "xdgforeign_v2.h"
#include "xdgoutput.h"
// Qt
#include <QDebug>
// wayland
#include <wayland-appmenu-client-protocol.h>
#include <wayland-blur-client-protocol.h>
#include <wayland-client-protocol.h>
#include <wayland-contrast-client-protocol.h>
#include <wayland-dpms-client-protocol.h>
#include <wayland-drm-lease-v1-client-protocol.h>
#include <wayland-ext-idle-notify-v1-client-protocol.h>
#include <wayland-fake-input-client-protocol.h>
#include <wayland-fullscreen-shell-client-protocol.h>
#include <wayland-idle-client-protocol.h>
#include <wayland-idle-inhibit-unstable-v1-client-protocol.h>
#include <wayland-input-method-v2-client-protocol.h>
#include <wayland-keyboard-shortcuts-inhibit-client-protocol.h>
#include <wayland-keystate-client-protocol.h>
#include <wayland-linux-dmabuf-unstable-v1-client-protocol.h>
#include <wayland-plasma-shell-client-protocol.h>
#include <wayland-plasma-virtual-desktop-client-protocol.h>
#include <wayland-plasma-window-management-client-protocol.h>
#include <wayland-pointer-constraints-unstable-v1-client-protocol.h>
#include <wayland-pointer-gestures-unstable-v1-client-protocol.h>
#include <wayland-presentation-time-client-protocol.h>
#include <wayland-primary-selection-unstable-v1-client-protocol.h>
#include <wayland-relativepointer-unstable-v1-client-protocol.h>
#include <wayland-security-context-v1-client-protocol.h>
#include <wayland-server-decoration-palette-client-protocol.h>
#include <wayland-shadow-client-protocol.h>
#include <wayland-slide-client-protocol.h>
#include <wayland-text-input-v2-client-protocol.h>
#include <wayland-text-input-v3-client-protocol.h>
#include <wayland-viewporter-client-protocol.h>
#include <wayland-virtual-keyboard-v1-client-protocol.h>
#include <wayland-wlr-data-control-v1-client-protocol.h>
#include <wayland-wlr-layer-shell-client-protocol.h>
#include <wayland-wlr-output-management-v1-client-protocol.h>
#include <wayland-xdg-activation-v1-client-protocol.h>
#include <wayland-xdg-decoration-unstable-v1-client-protocol.h>
#include <wayland-xdg-foreign-unstable-v2-client-protocol.h>
#include <wayland-xdg-output-unstable-v1-client-protocol.h>
#include <wayland-xdg-shell-client-protocol.h>

/*****
 * How to add another interface:
 * * define a new enum value in Registry::Interface
 * * define the bind<InterfaceName> method
 * * define the create<InterfaceName> method
 * * define the <interfaceName>Announced signal
 * * define the <interfaceName>Removed signal
 * * add a block to s_interfaces
 * * add the BIND macro for the new bind<InterfaceName>
 * * add the CREATE macro for the new create<InterfaceName>
 * * extend registry unit test to verify that it works
 ****/

namespace Wrapland
{
namespace Client
{

namespace
{
struct SuppertedInterfaceData {
    quint32 maxVersion;
    QByteArray name;
    wl_interface const* interface;
    void (Registry::*announcedSignal)(quint32, quint32);
    void (Registry::*removedSignal)(quint32);
};

static QMap<Registry::Interface, SuppertedInterfaceData> const s_interfaces = {
    {
        Registry::Interface::Compositor,
        {
            4,
            QByteArrayLiteral("wl_compositor"),
            &wl_compositor_interface,
            &Registry::compositorAnnounced,
            &Registry::compositorRemoved,
        },
    },
    {
        Registry::Interface::DataControlManagerV1,
        {
            2,
            QByteArrayLiteral("zwlr_data_control_manager_v1"),
            &zwlr_data_control_manager_v1_interface,
            &Registry::dataControlManagerV1Announced,
            &Registry::dataControlManagerV1Removed,
        },
    },
    {
        Registry::Interface::DataDeviceManager,
        {
            3,
            QByteArrayLiteral("wl_data_device_manager"),
            &wl_data_device_manager_interface,
            &Registry::dataDeviceManagerAnnounced,
            &Registry::dataDeviceManagerRemoved,
        },
    },
    {
        Registry::Interface::DrmLeaseDeviceV1,
        {
            1,
            QByteArrayLiteral("wp_drm_lease_device_v1"),
            &wp_drm_lease_device_v1_interface,
            &Registry::drmLeaseDeviceV1Announced,
            &Registry::drmLeaseDeviceV1Removed,
        },
    },
    {
        Registry::Interface::PrimarySelectionDeviceManager,
        {1,
         QByteArrayLiteral("zwp_primary_selection_device_manager_v1"),
         &zwp_primary_selection_device_manager_v1_interface,
         &Registry::primarySelectionDeviceManagerAnnounced,
         &Registry::primarySelectionDeviceManagerRemoved},
    },
    {
        Registry::Interface::Output,
        {
            3,
            QByteArrayLiteral("wl_output"),
            &wl_output_interface,
            &Registry::outputAnnounced,
            &Registry::outputRemoved,
        },
    },
    {
        Registry::Interface::Shm,
        {
            1,
            QByteArrayLiteral("wl_shm"),
            &wl_shm_interface,
            &Registry::shmAnnounced,
            &Registry::shmRemoved,
        },
    },
    {
        Registry::Interface::Seat,
        {
            5,
            QByteArrayLiteral("wl_seat"),
            &wl_seat_interface,
            &Registry::seatAnnounced,
            &Registry::seatRemoved,
        },
    },
    {
        Registry::Interface::SecurityContextManagerV1,
        {
            1,
            QByteArrayLiteral("wp_security_context_manager_v1"),
            &wp_security_context_manager_v1_interface,
            &Registry::securityContextManagerV1Announced,
            &Registry::securityContextManagerV1Removed,
        },
    },
    {
        Registry::Interface::Shell,
        {
            1,
            QByteArrayLiteral("wl_shell"),
            &wl_shell_interface,
            &Registry::shellAnnounced,
            &Registry::shellRemoved,
        },
    },
    {
        Registry::Interface::SubCompositor,
        {
            1,
            QByteArrayLiteral("wl_subcompositor"),
            &wl_subcompositor_interface,
            &Registry::subCompositorAnnounced,
            &Registry::subCompositorRemoved,
        },
    },
    {
        Registry::Interface::PlasmaActivationFeedback,
        {
            1,
            QByteArrayLiteral("org_kde_plasma_activation_feedback"),
            &org_kde_plasma_activation_feedback_interface,
            &Registry::plasmaActivationFeedbackAnnounced,
            &Registry::plasmaActivationFeedbackRemoved,
        },
    },
    {
        Registry::Interface::PlasmaShell,
        {
            8,
            QByteArrayLiteral("org_kde_plasma_shell"),
            &org_kde_plasma_shell_interface,
            &Registry::plasmaShellAnnounced,
            &Registry::plasmaShellRemoved,
        },
    },
    {
        Registry::Interface::PlasmaVirtualDesktopManagement,
        {
            2,
            QByteArrayLiteral("org_kde_plasma_virtual_desktop_management"),
            &org_kde_plasma_virtual_desktop_management_interface,
            &Registry::plasmaVirtualDesktopManagementAnnounced,
            &Registry::plasmaVirtualDesktopManagementRemoved,
        },
    },
    {
        Registry::Interface::PlasmaWindowManagement,
        {
            16,
            QByteArrayLiteral("org_kde_plasma_window_management"),
            &org_kde_plasma_window_management_interface,
            &Registry::plasmaWindowManagementAnnounced,
            &Registry::plasmaWindowManagementRemoved,
        },
    },
    {
        Registry::Interface::Idle,
        {
            1,
            QByteArrayLiteral("org_kde_kwin_idle"),
            &org_kde_kwin_idle_interface,
            &Registry::idleAnnounced,
            &Registry::idleRemoved,
        },
    },
    {
        Registry::Interface::IdleNotifierV1,
        {
            1,
            QByteArrayLiteral("ext_idle_notifier_v1"),
            &ext_idle_notifier_v1_interface,
            &Registry::idleNotifierV1Announced,
            &Registry::idleNotifierV1Removed,
        },
    },
    {
        Registry::Interface::InputMethodManagerV2,
        {
            1,
            QByteArrayLiteral("zwp_input_method_manager_v2"),
            &zwp_input_method_manager_v2_interface,
            &Registry::inputMethodManagerV2Announced,
            &Registry::inputMethodManagerV2Removed,
        },
    },
    {
        Registry::Interface::FakeInput,
        {
            4,
            QByteArrayLiteral("org_kde_kwin_fake_input"),
            &org_kde_kwin_fake_input_interface,
            &Registry::fakeInputAnnounced,
            &Registry::fakeInputRemoved,
        },
    },
    {
        Registry::Interface::LayerShellV1,
        {
            4,
            QByteArrayLiteral("zwlr_layer_shell_v1"),
            &zwlr_layer_shell_v1_interface,
            &Registry::layerShellV1Announced,
            &Registry::layerShellV1Removed,
        },
    },
    {
        Registry::Interface::WlrOutputManagerV1,
        {
            4,
            QByteArrayLiteral("zwlr_output_manager_v1"),
            &zwlr_output_manager_v1_interface,
            &Registry::wlrOutputManagerV1Announced,
            &Registry::wlrOutputManagerV1Removed,
        },
    },
    {
        Registry::Interface::Shadow,
        {
            2,
            QByteArrayLiteral("org_kde_kwin_shadow_manager"),
            &org_kde_kwin_shadow_manager_interface,
            &Registry::shadowAnnounced,
            &Registry::shadowRemoved,
        },
    },
    {
        Registry::Interface::Blur,
        {
            1,
            QByteArrayLiteral("org_kde_kwin_blur_manager"),
            &org_kde_kwin_blur_manager_interface,
            &Registry::blurAnnounced,
            &Registry::blurRemoved,
        },
    },
    {
        Registry::Interface::Contrast,
        {
            1,
            QByteArrayLiteral("org_kde_kwin_contrast_manager"),
            &org_kde_kwin_contrast_manager_interface,
            &Registry::contrastAnnounced,
            &Registry::contrastRemoved,
        },
    },
    {
        Registry::Interface::Slide,
        {
            1,
            QByteArrayLiteral("org_kde_kwin_slide_manager"),
            &org_kde_kwin_slide_manager_interface,
            &Registry::slideAnnounced,
            &Registry::slideRemoved,
        },
    },
    {
        Registry::Interface::FullscreenShell,
        {
            1,
            QByteArrayLiteral("_wl_fullscreen_shell"),
            &_wl_fullscreen_shell_interface,
            &Registry::fullscreenShellAnnounced,
            &Registry::fullscreenShellRemoved,
        },
    },
    {
        Registry::Interface::Dpms,
        {
            1,
            QByteArrayLiteral("org_kde_kwin_dpms_manager"),
            &org_kde_kwin_dpms_manager_interface,
            &Registry::dpmsAnnounced,
            &Registry::dpmsRemoved,
        },
    },
    {
        Registry::Interface::TextInputManagerV2,
        {
            1,
            QByteArrayLiteral("zwp_text_input_manager_v2"),
            &zwp_text_input_manager_v2_interface,
            &Registry::textInputManagerV2Announced,
            &Registry::textInputManagerV2Removed,
        },
    },
    {
        Registry::Interface::TextInputManagerV3,
        {
            1,
            QByteArrayLiteral("zwp_text_input_manager_v3"),
            &zwp_text_input_manager_v3_interface,
            &Registry::textInputManagerV3Announced,
            &Registry::textInputManagerV3Removed,
        },
    },
    {
        Registry::Interface::Viewporter,
        {
            1,
            QByteArrayLiteral("wp_viewporter"),
            &wp_viewporter_interface,
            &Registry::viewporterAnnounced,
            &Registry::viewporterRemoved,
        },
    },
    {
        Registry::Interface::VirtualKeyboardManagerV1,
        {
            1,
            QByteArrayLiteral("zwp_virtual_keyboard_manager_v1"),
            &zwp_virtual_keyboard_manager_v1_interface,
            &Registry::virtualKeyboardManagerV1Announced,
            &Registry::virtualKeyboardManagerV1Removed,
        },
    },
    {
        Registry::Interface::RelativePointerManagerUnstableV1,
        {
            1,
            QByteArrayLiteral("zwp_relative_pointer_manager_v1"),
            &zwp_relative_pointer_manager_v1_interface,
            &Registry::relativePointerManagerUnstableV1Announced,
            &Registry::relativePointerManagerUnstableV1Removed,
        },
    },
    {
        Registry::Interface::PointerGesturesUnstableV1,
        {
            3,
            QByteArrayLiteral("zwp_pointer_gestures_v1"),
            &zwp_pointer_gestures_v1_interface,
            &Registry::pointerGesturesUnstableV1Announced,
            &Registry::pointerGesturesUnstableV1Removed,
        },
    },
    {
        Registry::Interface::PointerConstraintsUnstableV1,
        {
            1,
            QByteArrayLiteral("zwp_pointer_constraints_v1"),
            &zwp_pointer_constraints_v1_interface,
            &Registry::pointerConstraintsUnstableV1Announced,
            &Registry::pointerConstraintsUnstableV1Removed,
        },
    },
    {
        Registry::Interface::PresentationManager,
        {
            1,
            QByteArrayLiteral("wp_presentation"),
            &wp_presentation_interface,
            &Registry::presentationManagerAnnounced,
            &Registry::presentationManagerRemoved,
        },
    },
    {
        Registry::Interface::XdgActivationV1,
        {
            1,
            QByteArrayLiteral("xdg_activation_v1"),
            &xdg_activation_v1_interface,
            &Registry::xdgActivationV1Announced,
            &Registry::xdgActivationV1Removed,
        },
    },
    {
        Registry::Interface::XdgExporterUnstableV2,
        {
            1,
            QByteArrayLiteral("zxdg_exporter_v2"),
            &zxdg_exporter_v2_interface,
            &Registry::exporterUnstableV2Announced,
            &Registry::exporterUnstableV2Removed,
        },
    },
    {
        Registry::Interface::XdgImporterUnstableV2,
        {
            1,
            QByteArrayLiteral("zxdg_importer_v2"),
            &zxdg_importer_v2_interface,
            &Registry::importerUnstableV2Announced,
            &Registry::importerUnstableV2Removed,
        },
    },
    {
        Registry::Interface::IdleInhibitManagerUnstableV1,
        {
            1,
            QByteArrayLiteral("zwp_idle_inhibit_manager_v1"),
            &zwp_idle_inhibit_manager_v1_interface,
            &Registry::idleInhibitManagerUnstableV1Announced,
            &Registry::idleInhibitManagerUnstableV1Removed,
        },
    },
    {
        Registry::Interface::AppMenu,
        {
            1,
            QByteArrayLiteral("org_kde_kwin_appmenu_manager"),
            &org_kde_kwin_appmenu_manager_interface,
            &Registry::appMenuAnnounced,
            &Registry::appMenuRemoved,
        },
    },
    {
        Registry::Interface::ServerSideDecorationPalette,
        {
            1,
            QByteArrayLiteral("org_kde_kwin_server_decoration_palette_manager"),
            &org_kde_kwin_server_decoration_palette_manager_interface,
            &Registry::serverSideDecorationPaletteManagerAnnounced,
            &Registry::serverSideDecorationPaletteManagerRemoved,
        },
    },
    {
        Registry::Interface::XdgOutputUnstableV1,
        {
            3,
            QByteArrayLiteral("zxdg_output_manager_v1"),
            &zxdg_output_manager_v1_interface,
            &Registry::xdgOutputAnnounced,
            &Registry::xdgOutputRemoved,
        },
    },
    {
        Registry::Interface::XdgShell,
        {
            5,
            QByteArrayLiteral("xdg_wm_base"),
            &xdg_wm_base_interface,
            &Registry::xdgShellAnnounced,
            &Registry::xdgShellRemoved,
        },
    },
    {
        Registry::Interface::XdgDecorationUnstableV1,
        {
            1,
            QByteArrayLiteral("zxdg_decoration_manager_v1"),
            &zxdg_decoration_manager_v1_interface,
            &Registry::xdgDecorationAnnounced,
            &Registry::xdgDecorationRemoved,
        },
    },
    {
        Registry::Interface::Keystate,
        {
            1,
            QByteArrayLiteral("org_kde_kwin_keystate"),
            &org_kde_kwin_keystate_interface,
            &Registry::keystateAnnounced,
            &Registry::keystateRemoved,
        },
    },
    {
        Registry::Interface::KeyboardShortcutsInhibitManagerV1,
        {
            1,
            QByteArrayLiteral("zwp_keyboard_shortcuts_inhibit_manager_v1"),
            &zwp_keyboard_shortcuts_inhibit_manager_v1_interface,
            &Registry::keyboardShortcutsInhibitManagerAnnounced,
            &Registry::keyboardShortcutsInhibitManagerRemoved,
        },
    },
    {
        Registry::Interface::LinuxDmabufV1,
        {
            3,
            QByteArrayLiteral("zwp_linux_dmabuf_v1"),
            &zwp_linux_dmabuf_v1_interface,
            &Registry::LinuxDmabufV1Announced,
            &Registry::LinuxDmabufV1Removed,
        },
    },
};

static quint32 maxVersion(Registry::Interface const& interface)
{
    auto it = s_interfaces.find(interface);
    if (it != s_interfaces.end()) {
        return it.value().maxVersion;
    }
    return 0;
}
}

class Q_DECL_HIDDEN Registry::Private
{
public:
    Private(Registry* q);
    void setup();
    bool hasInterface(Interface interface) const;
    AnnouncedInterface interface(Interface interface) const;
    QVector<AnnouncedInterface> interfaces(Interface interface) const;
    Interface interfaceForName(quint32 name) const;
    template<typename T>
    T* bind(Interface interface, uint32_t name, uint32_t version) const;
    template<class T, typename WL>
    T* create(quint32 name,
              quint32 version,
              QObject* parent,
              WL* (Registry::*bindMethod)(uint32_t, uint32_t) const);

    WaylandPointer<wl_registry, wl_registry_destroy> registry;
    static const struct wl_callback_listener s_callbackListener;
    WaylandPointer<wl_callback, wl_callback_destroy> callback;
    EventQueue* queue = nullptr;

private:
    void handleAnnounce(uint32_t name, char const* interface, uint32_t version);
    void handleRemove(uint32_t name);
    void handleGlobalSync();
    static void globalAnnounce(void* data,
                               struct wl_registry* registry,
                               uint32_t name,
                               char const* interface,
                               uint32_t version);
    static void globalRemove(void* data, struct wl_registry* registry, uint32_t name);
    static void globalSync(void* data, struct wl_callback* callback, uint32_t serial);

    Registry* q;
    struct InterfaceData {
        Interface interface;
        uint32_t name;
        uint32_t version;
    };
    QList<InterfaceData> m_interfaces;
    static const struct wl_registry_listener s_registryListener;
};

Registry::Private::Private(Registry* q)
    : q(q)
{
}

void Registry::Private::setup()
{
    wl_registry_add_listener(registry, &s_registryListener, this);
    wl_callback_add_listener(callback, &s_callbackListener, this);
}

Registry::Registry(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

Registry::~Registry()
{
    release();
}

void Registry::release()
{
    d->registry.release();
    d->callback.release();
}

void Registry::create(wl_display* display)
{
    Q_ASSERT(display);
    Q_ASSERT(!isValid());
    d->registry.setup(wl_display_get_registry(display));
    d->callback.setup(wl_display_sync(display));
    if (d->queue) {
        d->queue->addProxy(d->registry);
        d->queue->addProxy(d->callback);
    }
}

void Registry::create(ConnectionThread* connection)
{
    create(connection->display());
    connect(connection, &ConnectionThread::establishedChanged, this, [this](bool established) {
        if (!established) {
            Q_EMIT registryReleased();
            release();
        }
    });
}

void Registry::setup()
{
    Q_ASSERT(isValid());
    d->setup();
}

void Registry::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
    if (!queue) {
        return;
    }
    if (d->registry) {
        d->queue->addProxy(d->registry);
    }
    if (d->callback) {
        d->queue->addProxy(d->callback);
    }
}

EventQueue* Registry::eventQueue()
{
    return d->queue;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct wl_registry_listener Registry::Private::s_registryListener = {
    globalAnnounce,
    globalRemove,
};

const struct wl_callback_listener Registry::Private::s_callbackListener = {globalSync};
#endif

void Registry::Private::globalAnnounce(void* data,
                                       wl_registry* registry,
                                       uint32_t name,
                                       char const* interface,
                                       uint32_t version)
{
    auto r = reinterpret_cast<Registry::Private*>(data);
    Q_ASSERT(registry == r->registry);
    r->handleAnnounce(name, interface, version);
}

void Registry::Private::globalRemove(void* data, wl_registry* registry, uint32_t name)
{
    auto r = reinterpret_cast<Registry::Private*>(data);
    Q_ASSERT(registry == r->registry);
    r->handleRemove(name);
}

void Registry::Private::globalSync(void* data, wl_callback* callback, uint32_t serial)
{
    Q_UNUSED(serial)
    auto r = reinterpret_cast<Registry::Private*>(data);
    Q_ASSERT(r->callback == callback);
    r->handleGlobalSync();
    r->callback.release();
}

void Registry::Private::handleGlobalSync()
{
    Q_EMIT q->interfacesAnnounced();
}

namespace
{
static Registry::Interface nameToInterface(char const* interface)
{
    for (auto it = s_interfaces.constBegin(); it != s_interfaces.constEnd(); ++it) {
        if (qstrcmp(interface, it.value().name) == 0) {
            return it.key();
        }
    }
    return Registry::Interface::Unknown;
}
}

void Registry::Private::handleAnnounce(uint32_t name, char const* interface, uint32_t version)
{
    Interface i = nameToInterface(interface);
    Q_EMIT q->interfaceAnnounced(QByteArray(interface), name, version);
    if (i == Interface::Unknown) {
        qCDebug(WRAPLAND_CLIENT) << "Unknown interface announced: " << interface << "/" << name
                                 << "/" << version;
        return;
    }
    qCDebug(WRAPLAND_CLIENT) << "Wayland Interface: " << interface << "/" << name << "/" << version;
    m_interfaces.append({i, name, version});
    auto it = s_interfaces.constFind(i);
    if (it != s_interfaces.end()) {
        Q_EMIT(q->*it.value().announcedSignal)(name, version);
    }
}

void Registry::Private::handleRemove(uint32_t name)
{
    auto it = std::find_if(m_interfaces.begin(),
                           m_interfaces.end(),
                           [name](InterfaceData const& data) { return data.name == name; });
    if (it != m_interfaces.end()) {
        InterfaceData data = *(it);
        m_interfaces.erase(it);
        auto sit = s_interfaces.find(data.interface);
        if (sit != s_interfaces.end()) {
            Q_EMIT(q->*sit.value().removedSignal)(data.name);
        }
    }
    Q_EMIT q->interfaceRemoved(name);
}

bool Registry::Private::hasInterface(Registry::Interface interface) const
{
    auto it = std::find_if(
        m_interfaces.constBegin(), m_interfaces.constEnd(), [interface](InterfaceData const& data) {
            return data.interface == interface;
        });
    return it != m_interfaces.constEnd();
}

QVector<Registry::AnnouncedInterface> Registry::Private::interfaces(Interface interface) const
{
    QVector<Registry::AnnouncedInterface> retVal;
    for (auto it = m_interfaces.constBegin(); it != m_interfaces.constEnd(); ++it) {
        auto const& data = *it;
        if (data.interface == interface) {
            retVal << AnnouncedInterface{data.name, data.version};
        }
    }
    return retVal;
}

Registry::AnnouncedInterface Registry::Private::interface(Interface interface) const
{
    auto const all = interfaces(interface);
    if (!all.isEmpty()) {
        return all.last();
    }
    return AnnouncedInterface{0, 0};
}

Registry::Interface Registry::Private::interfaceForName(quint32 name) const
{
    auto it = std::find_if(m_interfaces.constBegin(),
                           m_interfaces.constEnd(),
                           [name](InterfaceData const& data) { return data.name == name; });
    if (it == m_interfaces.constEnd()) {
        return Interface::Unknown;
    }
    return (*it).interface;
}

bool Registry::hasInterface(Registry::Interface interface) const
{
    return d->hasInterface(interface);
}

QVector<Registry::AnnouncedInterface> Registry::interfaces(Interface interface) const
{
    return d->interfaces(interface);
}

Registry::AnnouncedInterface Registry::interface(Interface interface) const
{
    return d->interface(interface);
}

#define BIND2(__NAME__, __INAME__, __WL__)                                                         \
    __WL__* Registry::bind##__NAME__(uint32_t name, uint32_t version) const                        \
    {                                                                                              \
        return d->bind<__WL__>(                                                                    \
            Interface::__INAME__, name, qMin(maxVersion(Interface::__INAME__), version));          \
    }

#define BIND(__NAME__, __WL__) BIND2(__NAME__, __NAME__, __WL__)

BIND(Compositor, wl_compositor)
BIND(Output, wl_output)
BIND(Seat, wl_seat)
BIND(Shell, wl_shell)
BIND(Shm, wl_shm)
BIND(SubCompositor, wl_subcompositor)
BIND(FullscreenShell, _wl_fullscreen_shell)
BIND(DataControlManagerV1, zwlr_data_control_manager_v1)
BIND(DataDeviceManager, wl_data_device_manager)
BIND(DrmLeaseDeviceV1, wp_drm_lease_device_v1)
BIND(PlasmaActivationFeedback, org_kde_plasma_activation_feedback)
BIND(PlasmaShell, org_kde_plasma_shell)
BIND(PlasmaVirtualDesktopManagement, org_kde_plasma_virtual_desktop_management)
BIND(PlasmaWindowManagement, org_kde_plasma_window_management)
BIND(Idle, org_kde_kwin_idle)
BIND(IdleNotifierV1, ext_idle_notifier_v1)
BIND(InputMethodManagerV2, zwp_input_method_manager_v2)
BIND(FakeInput, org_kde_kwin_fake_input)
BIND(LayerShellV1, zwlr_layer_shell_v1)
BIND(WlrOutputManagerV1, zwlr_output_manager_v1)
BIND(TextInputManagerV2, zwp_text_input_manager_v2)
BIND(TextInputManagerV3, zwp_text_input_manager_v3)
BIND(Viewporter, wp_viewporter)
BIND(VirtualKeyboardManagerV1, zwp_virtual_keyboard_manager_v1)
BIND(XdgShell, xdg_wm_base)
BIND(RelativePointerManagerUnstableV1, zwp_relative_pointer_manager_v1)
BIND(PointerGesturesUnstableV1, zwp_pointer_gestures_v1)
BIND(PointerConstraintsUnstableV1, zwp_pointer_constraints_v1)
BIND(PresentationManager, wp_presentation)
BIND(PrimarySelectionDeviceManager, zwp_primary_selection_device_manager_v1)
BIND(SecurityContextManagerV1, wp_security_context_manager_v1)
BIND(XdgActivationV1, xdg_activation_v1)
BIND(XdgExporterUnstableV2, zxdg_exporter_v2)
BIND(XdgImporterUnstableV2, zxdg_importer_v2)
BIND(IdleInhibitManagerUnstableV1, zwp_idle_inhibit_manager_v1)
BIND(Keystate, org_kde_kwin_keystate)
BIND2(ShadowManager, Shadow, org_kde_kwin_shadow_manager)
BIND2(BlurManager, Blur, org_kde_kwin_blur_manager)
BIND2(ContrastManager, Contrast, org_kde_kwin_contrast_manager)
BIND2(SlideManager, Slide, org_kde_kwin_slide_manager)
BIND2(DpmsManager, Dpms, org_kde_kwin_dpms_manager)
BIND2(AppMenuManager, AppMenu, org_kde_kwin_appmenu_manager)
BIND2(ServerSideDecorationPaletteManager,
      ServerSideDecorationPalette,
      org_kde_kwin_server_decoration_palette_manager)
BIND(XdgOutputUnstableV1, zxdg_output_manager_v1)
BIND(XdgDecorationUnstableV1, zxdg_decoration_manager_v1)
BIND(KeyboardShortcutsInhibitManagerV1, zwp_keyboard_shortcuts_inhibit_manager_v1)
BIND(LinuxDmabufV1, zwp_linux_dmabuf_v1)
#undef BIND
#undef BIND2

template<class T, typename WL>
T* Registry::Private::create(quint32 name,
                             quint32 version,
                             QObject* parent,
                             WL* (Registry::*bindMethod)(uint32_t, uint32_t) const)
{
    T* t = new T(parent);
    t->setEventQueue(queue);
    t->setup((q->*bindMethod)(name, version));
    QObject::connect(q, &Registry::interfaceRemoved, t, [t, name](quint32 removed) {
        if (name == removed) {
            Q_EMIT t->removed();
        }
    });
    QObject::connect(q, &Registry::registryReleased, t, &T::release);
    return t;
}

#define CREATE2(__NAME__, __BINDNAME__)                                                            \
    __NAME__* Registry::create##__NAME__(quint32 name, quint32 version, QObject* parent)           \
    {                                                                                              \
        return d->create<__NAME__>(name, version, parent, &Registry::bind##__BINDNAME__);          \
    }

#define CREATE(__NAME__) CREATE2(__NAME__, __NAME__)

CREATE(Compositor)
CREATE(Seat)
CREATE(Shell)
CREATE(SubCompositor)
CREATE(FullscreenShell)
CREATE(Output)
CREATE(DataDeviceManager)
CREATE(PlasmaShell)
CREATE(PlasmaVirtualDesktopManagement)
CREATE(PlasmaWindowManagement)
CREATE(PrimarySelectionDeviceManager)
CREATE(Idle)
CREATE(FakeInput)
CREATE(LayerShellV1)
CREATE(WlrOutputManagerV1)
CREATE(ShadowManager)
CREATE(BlurManager)
CREATE(ContrastManager)
CREATE(SlideManager)
CREATE(DpmsManager)
CREATE2(ShmPool, Shm)
CREATE(AppMenuManager)
CREATE(Keystate)
CREATE(ServerSideDecorationPaletteManager)
CREATE(Viewporter)
CREATE(KeyboardShortcutsInhibitManagerV1)
CREATE(PresentationManager)
CREATE(XdgActivationV1)
CREATE(XdgShell)

#undef CREATE
#undef CREATE2

drm_lease_device_v1*
Registry::createDrmLeaseDeviceV1(quint32 name, quint32 version, QObject* parent)
{
    auto dev
        = d->create<drm_lease_device_v1>(name, version, parent, &Registry::bindDrmLeaseDeviceV1);
    auto priv_dev = dev->d_ptr;

    // Cleanup private when no handshake is possible anymore (global removal, registry destroyed).
    QObject::connect(
        this, &Registry::interfaceRemoved, priv_dev, [priv_dev, name](quint32 removed) {
            if (name == removed) {
                priv_dev->q_ptr = nullptr;
                delete priv_dev;
            }
        });
    QObject::connect(this, &Registry::destroyed, priv_dev, [priv_dev, name] {
        priv_dev->q_ptr = nullptr;
        delete priv_dev;
    });
    return dev;
}

XdgExporter* Registry::createXdgExporter(quint32 name, quint32 version, QObject* parent)
{
    // only V1 supported for now
    return d->create<XdgExporterUnstableV2>(
        name, version, parent, &Registry::bindXdgExporterUnstableV2);
}

XdgImporter* Registry::createXdgImporter(quint32 name, quint32 version, QObject* parent)
{
    // only V1 supported for now
    return d->create<XdgImporterUnstableV2>(
        name, version, parent, &Registry::bindXdgImporterUnstableV2);
}

idle_notifier_v1* Registry::createIdleNotifierV1(quint32 name, quint32 version, QObject* parent)
{
    return d->create<idle_notifier_v1>(name, version, parent, &Registry::bindIdleNotifierV1);
}

input_method_manager_v2*
Registry::createInputMethodManagerV2(quint32 name, quint32 version, QObject* parent)
{
    return d->create<input_method_manager_v2>(
        name, version, parent, &Registry::bindInputMethodManagerV2);
}

security_context_manager_v1*
Registry::createSecurityContextManagerV1(quint32 name, quint32 version, QObject* parent)
{
    return d->create<security_context_manager_v1>(
        name, version, parent, &Registry::bindSecurityContextManagerV1);
}

text_input_manager_v3*
Registry::createTextInputManagerV3(quint32 name, quint32 version, QObject* parent)
{
    return d->create<text_input_manager_v3>(
        name, version, parent, &Registry::bindTextInputManagerV3);
}

TextInputManagerV2*
Registry::createTextInputManagerV2(quint32 name, quint32 version, QObject* parent)
{
    return d->create<TextInputManagerV2>(name, version, parent, &Registry::bindTextInputManagerV2);
}

data_control_manager_v1*
Registry::createDataControlManagerV1(quint32 name, quint32 version, QObject* parent)
{
    return d->create<data_control_manager_v1>(
        name, version, parent, &Registry::bindDataControlManagerV1);
}

plasma_activation_feedback*
Registry::createPlasmaActivationFeedback(quint32 name, quint32 version, QObject* parent)
{
    return d->create<plasma_activation_feedback>(
        name, version, parent, &Registry::bindPlasmaActivationFeedback);
}

RelativePointerManager*
Registry::createRelativePointerManager(quint32 name, quint32 version, QObject* parent)
{
    switch (d->interfaceForName(name)) {
    case Interface::RelativePointerManagerUnstableV1:
        return d->create<RelativePointerManager>(
            name, version, parent, &Registry::bindRelativePointerManagerUnstableV1);
    default:
        return nullptr;
    }
}

PointerGestures* Registry::createPointerGestures(quint32 name, quint32 version, QObject* parent)
{
    switch (d->interfaceForName(name)) {
    case Interface::PointerGesturesUnstableV1:
        return d->create<PointerGestures>(
            name, version, parent, &Registry::bindPointerGesturesUnstableV1);
    default:
        return nullptr;
    }
}

PointerConstraints*
Registry::createPointerConstraints(quint32 name, quint32 version, QObject* parent)
{
    switch (d->interfaceForName(name)) {
    case Interface::PointerConstraintsUnstableV1:
        return d->create<PointerConstraints>(
            name, version, parent, &Registry::bindPointerConstraintsUnstableV1);
    default:
        return nullptr;
    }
}

IdleInhibitManager*
Registry::createIdleInhibitManager(quint32 name, quint32 version, QObject* parent)
{
    switch (d->interfaceForName(name)) {
    case Interface::IdleInhibitManagerUnstableV1:
        return d->create<IdleInhibitManager>(
            name, version, parent, &Registry::bindIdleInhibitManagerUnstableV1);
    default:
        return nullptr;
    }
}

virtual_keyboard_manager_v1*
Registry::createVirtualKeyboardManagerV1(quint32 name, quint32 version, QObject* parent)
{
    return d->create<virtual_keyboard_manager_v1>(
        name, version, parent, &Registry::bindVirtualKeyboardManagerV1);
}

XdgOutputManager* Registry::createXdgOutputManager(quint32 name, quint32 version, QObject* parent)
{
    switch (d->interfaceForName(name)) {
    case Interface::XdgOutputUnstableV1:
        return d->create<XdgOutputManager>(
            name, version, parent, &Registry::bindXdgOutputUnstableV1);
    default:
        return nullptr;
    }
}

XdgDecorationManager*
Registry::createXdgDecorationManager(quint32 name, quint32 version, QObject* parent)
{
    switch (d->interfaceForName(name)) {
    case Interface::XdgDecorationUnstableV1:
        return d->create<XdgDecorationManager>(
            name, version, parent, &Registry::bindXdgDecorationUnstableV1);
    default:
        return nullptr;
    }
}

LinuxDmabufV1* Registry::createLinuxDmabufV1(quint32 name, quint32 version, QObject* parent)
{
    switch (d->interfaceForName(name)) {
    case Interface::LinuxDmabufV1:
        return d->create<LinuxDmabufV1>(name, version, parent, &Registry::bindLinuxDmabufV1);
    default:
        return nullptr;
    }
}

namespace
{
static wl_interface const* wlInterface(Registry::Interface interface)
{
    auto it = s_interfaces.find(interface);
    if (it != s_interfaces.end()) {
        return it.value().interface;
    }
    return nullptr;
}
}

template<typename T>
T* Registry::Private::bind(Registry::Interface interface, uint32_t name, uint32_t version) const
{
    auto it = std::find_if(
        m_interfaces.constBegin(), m_interfaces.constEnd(), [=](InterfaceData const& data) {
            return data.interface == interface && data.name == name && data.version >= version;
        });
    if (it == m_interfaces.constEnd()) {
        qCDebug(WRAPLAND_CLIENT) << "Don't have interface " << int(interface) << "with name "
                                 << name << "and minimum version" << version;
        return nullptr;
    }
    auto t
        = reinterpret_cast<T*>(wl_registry_bind(registry, name, wlInterface(interface), version));
    if (queue) {
        queue->addProxy(t);
    }
    return t;
}

bool Registry::isValid() const
{
    return d->registry.isValid();
}

wl_registry* Registry::registry()
{
    return d->registry;
}

Registry::operator wl_registry*() const
{
    return d->registry;
}

Registry::operator wl_registry*()
{
    return d->registry;
}

}
}
