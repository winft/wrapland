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

#include "bind.h"
#include "display.h"
#include "nucleus.h"
#include "resource.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <vector>

#include <wayland-server.h>

namespace Wrapland::Server
{

class Client;
class Display;

namespace Wayland
{

class Client;

template<typename Global>
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto& global_get_display_ref(Global& global)
{
    using Handle = decltype(global.handle);
    auto& globals = global.display()->handle->globals;

    if constexpr (std::is_same_v<Handle, decltype(globals.compositor)>) {
        return globals.compositor;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.subcompositor)>) {
        return globals.subcompositor;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.linux_dmabuf_v1)>) {
        return globals.linux_dmabuf_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.viewporter)>) {
        return globals.viewporter;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.presentation_manager)>) {
        return globals.presentation_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.shadow_manager)>) {
        return globals.shadow_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.blur_manager)>) {
        return globals.blur_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.slide_manager)>) {
        return globals.slide_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.contrast_manager)>) {
        return globals.contrast_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.xdg_output_manager)>) {
        return globals.xdg_output_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.dpms_manager)>) {
        return globals.dpms_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.relative_pointer_manager_v1)>) {
        return globals.relative_pointer_manager_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.pointer_gestures_v1)>) {
        return globals.pointer_gestures_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.pointer_constraints_v1)>) {
        return globals.pointer_constraints_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.text_input_manager_v2)>) {
        return globals.text_input_manager_v2;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.text_input_manager_v3)>) {
        return globals.text_input_manager_v3;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.input_method_manager_v2)>) {
        return globals.input_method_manager_v2;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.key_state)>) {
        return globals.key_state;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.fake_input)>) {
        return globals.fake_input;
    } else if constexpr (std::is_same_v<Handle,
                                        decltype(globals.keyboard_shortcuts_inhibit_manager_v1)>) {
        return globals.keyboard_shortcuts_inhibit_manager_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.virtual_keyboard_manager_v1)>) {
        return globals.virtual_keyboard_manager_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.data_device_manager)>) {
        return globals.data_device_manager;
    } else if constexpr (std::is_same_v<Handle,
                                        decltype(globals.primary_selection_device_manager)>) {
        return globals.primary_selection_device_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.data_control_manager_v1)>) {
        return globals.data_control_manager_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.xdg_shell)>) {
        return globals.xdg_shell;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.xdg_foreign)>) {
        return globals.xdg_foreign;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.xdg_activation_v1)>) {
        return globals.xdg_activation_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.appmenu_manager)>) {
        return globals.appmenu_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.xdg_decoration_manager)>) {
        return globals.xdg_decoration_manager;
    } else if constexpr (std::is_same_v<Handle,
                                        decltype(globals.server_side_decoration_palette_manager)>) {
        return globals.server_side_decoration_palette_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.layer_shell_v1)>) {
        return globals.layer_shell_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.plasma_activation_feedback)>) {
        return globals.plasma_activation_feedback;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.plasma_virtual_desktop_manager)>) {
        return globals.plasma_virtual_desktop_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.plasma_shell)>) {
        return globals.plasma_shell;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.plasma_window_manager)>) {
        return globals.plasma_window_manager;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.idle_notifier_v1)>) {
        return globals.idle_notifier_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.kde_idle)>) {
        return globals.kde_idle;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.drm_lease_device_v1)>) {
        return globals.drm_lease_device_v1;
    } else if constexpr (std::is_same_v<Handle, decltype(globals.idle_inhibit_manager_v1)>) {
        return globals.idle_inhibit_manager_v1;
    } else {
        // Return a nonsensical reference for exclusion with concepts.
        return global;
    }
}

template<typename Global>
void global_register_display(Global& global)
{
    if constexpr (std::same_as<std::remove_reference_t<decltype(global_get_display_ref(global))>,
                               decltype(global.handle)>) {
        global_get_display_ref(global) = global.handle;
    }
}

template<typename Global>
void global_unregister_display(Global& global)
{
    if (!global.display()) {
        return;
    }

    if constexpr (std::same_as<std::remove_reference_t<decltype(global_get_display_ref(global))>,
                               decltype(global.handle)>) {
        if (auto& ptr = global_get_display_ref(global); ptr == global.handle) {
            ptr = nullptr;
        }
    }
}

template<typename Handle, int Version = 1>
class Global
{
public:
    using type = Global<Handle, Version>;
    static int constexpr version = Version;

    Global(Global const&) = delete;
    Global& operator=(Global const&) = delete;
    Global(Global&&) noexcept = delete;
    Global& operator=(Global&&) noexcept = delete;

    virtual ~Global()
    {
        global_unregister_display(*this);
        nucleus->remove();
    }

    void create()
    {
        nucleus->create();
    }

    Display* display()
    {
        return nucleus->display;
    }

    static Handle* get_handle(wl_resource* wlResource)
    {
        auto bind = static_cast<Bind<type>*>(wl_resource_get_user_data(wlResource));

        if (auto global = bind->global()) {
            return global->handle;
        }

        // If we are here the global has been removed while not yet destroyed.
        return nullptr;
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Bind<type>* bind, Args&&... args)
    {
        // See Vandevoorde et al.: C++ Templates - The Complete Guide p.79
        // or https://stackoverflow.com/a/4942746.
        bind->template send<sender, minVersion>(std::forward<Args>(args)...);
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Client* client, Args&&... args)
    {
        for (auto bind : nucleus->binds()) {
            if (bind->client() == client) {
                bind->template send<sender, minVersion>(std::forward<Args>(args)...);
            }
        }
    }

    template<auto sender, uint32_t minVersion = 0, typename... Args>
    void send(Args&&... args)
    {
        for (auto bind : nucleus->binds) {
            bind->template send<sender, minVersion>(std::forward<Args>(args)...);
        }
    }

    Bind<type>* getBind(wl_resource* wlResource)
    {
        for (auto bind : nucleus->binds) {
            if (bind->resource == wlResource) {
                return bind;
            }
        }
        return nullptr;
    }

    std::vector<Bind<type>*> getBinds()
    {
        return nucleus->binds;
    }

    std::vector<Bind<type>*> getBinds(Server::Client* client)
    {
        std::vector<Bind<type>*> ret;
        for (auto bind : nucleus->binds) {
            if (bind->client->handle == client) {
                ret.push_back(bind);
            }
        }
        return ret;
    }

    virtual void bindInit([[maybe_unused]] Bind<type>* bind)
    {
    }

    virtual void prepareUnbind([[maybe_unused]] Bind<type>* bind)
    {
    }

    Handle* handle;

protected:
    Global(Handle* handle,
           Server::Display* display,
           wl_interface const* interface,
           void const* implementation)
        : handle{handle}
        , nucleus{new Nucleus<type>(this, display, interface, implementation)}
    {
        global_register_display(*this);
        // TODO(romangg): allow to create and destroy Globals while keeping the object existing (but
        //                always create on ctor call?).
    }

    static void resourceDestroyCallback(wl_client* wlClient, wl_resource* wlResource)
    {
        Bind<type>::destroy_callback(wlClient, wlResource);
    }

    template<auto callback, typename... Args>
    static void cb([[maybe_unused]] wl_client* client, wl_resource* resource, Args... args)
    {
        // The global might be destroyed already on the compositor side.
        if (get_handle(resource)) {
            auto bind = static_cast<Bind<type>*>(wl_resource_get_user_data(resource));
            callback(bind, std::forward<Args>(args)...);
        }
    }

private:
    Nucleus<type>* nucleus;
};

}
}
