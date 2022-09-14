/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "appmenu.h"
#include "blur.h"
#include "compositor.h"
#include "contrast.h"
#include "data_control_v1.h"
#include "data_device_manager.h"
#include "dpms.h"
#include "drm_lease_v1.h"
#include "fake_input.h"
#include "idle_inhibit_v1.h"
#include "input_method_v2.h"
#include "kde_idle.h"
#include "keyboard_shortcuts_inhibit.h"
#include "keystate.h"
#include "layer_shell_v1.h"
#include "linux_dmabuf_v1.h"
#include "output_management_v1.h"
#include "plasma_activation_feedback.h"
#include "plasma_shell.h"
#include "plasma_virtual_desktop.h"
#include "plasma_window.h"
#include "pointer.h"
#include "pointer_constraints_v1.h"
#include "pointer_gestures_v1.h"
#include "presentation_time.h"
#include "primary_selection.h"
#include "relative_pointer_v1.h"
#include "seat.h"
#include "server_decoration_palette.h"
#include "shadow.h"
#include "slide.h"
#include "subcompositor.h"
#include "text_input_v2.h"
#include "text_input_v3.h"
#include "viewporter.h"
#include "virtual_keyboard_v1.h"
#include "xdg_activation_v1.h"
#include "xdg_decoration.h"
#include "xdg_foreign.h"
#include "xdg_output.h"
#include "xdg_shell.h"

#include <memory>
#include <vector>

namespace Wrapland::Server
{

struct globals {
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
    std::vector<std::unique_ptr<Server::Output>> outputs;
    std::unique_ptr<Server::XdgOutputManager> xdg_output_manager;
    std::unique_ptr<Server::DpmsManager> dpms_manager;
    std::unique_ptr<Server::OutputManagementV1> output_management_v1;

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
    std::unique_ptr<Server::kde_idle> kde_idle;
    std::unique_ptr<Server::drm_lease_device_v1> drm_lease_device_v1;
    std::unique_ptr<Server::IdleInhibitManagerV1> idle_inhibit_manager_v1;
};

}
