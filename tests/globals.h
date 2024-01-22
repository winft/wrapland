/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "../../server/appmenu.h"
#include "../../server/blur.h"
#include "../../server/compositor.h"
#include "../../server/contrast.h"
#include "../../server/data_control_v1.h"
#include "../../server/data_device_manager.h"
#include "../../server/dpms.h"
#include "../../server/drm_lease_v1.h"
#include "../../server/fake_input.h"
#include "../../server/idle_inhibit_v1.h"
#include "../../server/idle_notify_v1.h"
#include "../../server/input_method_v2.h"
#include "../../server/kde_idle.h"
#include "../../server/keyboard_shortcuts_inhibit.h"
#include "../../server/keystate.h"
#include "../../server/layer_shell_v1.h"
#include "../../server/linux_dmabuf_v1.h"
#include "../../server/output_manager.h"
#include "../../server/plasma_activation_feedback.h"
#include "../../server/plasma_shell.h"
#include "../../server/plasma_virtual_desktop.h"
#include "../../server/plasma_window.h"
#include "../../server/pointer.h"
#include "../../server/pointer_constraints_v1.h"
#include "../../server/pointer_gestures_v1.h"
#include "../../server/presentation_time.h"
#include "../../server/primary_selection.h"
#include "../../server/relative_pointer_v1.h"
#include "../../server/seat.h"
#include "../../server/security_context_v1.h"
#include "../../server/server_decoration_palette.h"
#include "../../server/shadow.h"
#include "../../server/slide.h"
#include "../../server/subcompositor.h"
#include "../../server/text_input_v2.h"
#include "../../server/text_input_v3.h"
#include "../../server/viewporter.h"
#include "../../server/virtual_keyboard_v1.h"
#include "../../server/wlr_output_manager_v1.h"
#include "../../server/xdg_activation_v1.h"
#include "../../server/xdg_decoration.h"
#include "../../server/xdg_foreign.h"
#include "../../server/xdg_output.h"
#include "../../server/xdg_shell.h"

#include <deque>
#include <memory>
#include <vector>

namespace Wrapland::Server
{

struct globals {
    globals() = default;
    globals(globals const&) = delete;
    globals& operator=(globals const&) = delete;
    globals(globals&&) noexcept = default;
    globals& operator=(globals&&) noexcept = default;

    /// Basic graphical operations
    std::unique_ptr<Server::Compositor> compositor;
    std::unique_ptr<Server::Subcompositor> subcompositor;
    std::unique_ptr<Server::linux_dmabuf_v1> linux_dmabuf_v1;
    std::unique_ptr<Server::Viewporter> viewporter;
    std::unique_ptr<Server::PresentationManager> presentation_manager;

    /// Additional graphical effects
    std::unique_ptr<Server::ShadowManager> shadow_manager;
    std::unique_ptr<Server::BlurManager> blur_manager;
    std::unique_ptr<Server::SlideManager> slide_manager;
    std::unique_ptr<Server::ContrastManager> contrast_manager;

    /// Graphical outputs and their management
    std::deque<std::unique_ptr<output>> outputs;
    std::unique_ptr<Server::output_manager> output_manager;
    std::unique_ptr<Server::DpmsManager> dpms_manager;

    /// Basic input
    std::vector<std::unique_ptr<Server::Seat>> seats;
    std::unique_ptr<Server::RelativePointerManagerV1> relative_pointer_manager_v1;
    std::unique_ptr<Server::PointerGesturesV1> pointer_gestures_v1;
    std::unique_ptr<Server::PointerConstraintsV1> pointer_constraints_v1;

    /// Input method support
    std::unique_ptr<Server::text_input_manager_v2> text_input_manager_v2;
    std::unique_ptr<Server::text_input_manager_v3> text_input_manager_v3;
    std::unique_ptr<Server::input_method_manager_v2> input_method_manager_v2;

    /// Global input state manipulation
    std::unique_ptr<Server::KeyState> key_state;
    std::unique_ptr<Server::FakeInput> fake_input;
    std::unique_ptr<Server::KeyboardShortcutsInhibitManagerV1>
        keyboard_shortcuts_inhibit_manager_v1;
    std::unique_ptr<Server::virtual_keyboard_manager_v1> virtual_keyboard_manager_v1;

    /// Data sharing between clients
    std::unique_ptr<Server::data_device_manager> data_device_manager;
    std::unique_ptr<Server::primary_selection_device_manager> primary_selection_device_manager;
    std::unique_ptr<Server::data_control_manager_v1> data_control_manager_v1;

    /// Individual windowing control
    std::unique_ptr<Server::XdgShell> xdg_shell;
    std::unique_ptr<Server::XdgForeign> xdg_foreign;
    std::unique_ptr<Server::XdgActivationV1> xdg_activation_v1;

    /// Individual window styling and functionality
    std::unique_ptr<Server::AppmenuManager> appmenu_manager;
    std::unique_ptr<Server::XdgDecorationManager> xdg_decoration_manager;
    std::unique_ptr<Server::ServerSideDecorationPaletteManager>
        server_side_decoration_palette_manager;

    /// Global windowing control
    std::unique_ptr<Server::LayerShellV1> layer_shell_v1;
    std::unique_ptr<Server::plasma_activation_feedback> plasma_activation_feedback;
    std::unique_ptr<Server::PlasmaVirtualDesktopManager> plasma_virtual_desktop_manager;
    std::unique_ptr<Server::PlasmaShell> plasma_shell;
    std::unique_ptr<Server::PlasmaWindowManager> plasma_window_manager;

    /// Hardware take-over and blocking
    std::unique_ptr<Server::idle_notifier_v1> idle_notifier_v1;
    std::unique_ptr<Server::kde_idle> kde_idle;
    std::unique_ptr<Server::drm_lease_device_v1> drm_lease_device_v1;
    std::unique_ptr<Server::IdleInhibitManagerV1> idle_inhibit_manager_v1;

    // Security
    std::unique_ptr<Server::security_context_manager_v1> security_context_manager_v1;
};

}
