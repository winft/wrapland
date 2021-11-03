/****************************************************************************
Copyright © 2020 Adrien Faveraux <ad1rie3@hotmail.fr>

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
****************************************************************************/
#pragma once
#include "text_input_v2.h"

#include <QPointer>
#include <QRect>
#include <QVector>
#include <vector>

#include <wayland-text-input-unstable-v2-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t text_input_manager_v2_version = 1;
using text_input_manager_v2_global
    = Wayland::Global<text_input_manager_v2, text_input_manager_v2_version>;
using text_input_manager_v2_bind = Wayland::Bind<text_input_manager_v2_global>;

class text_input_manager_v2::Private : public text_input_manager_v2_global
{
public:
    Private(Display* display, text_input_manager_v2* q);

private:
    static void
    get_text_input_callback(text_input_manager_v2_bind* bind, uint32_t id, wl_resource* wlSeat);

    static const struct zwp_text_input_manager_v2_interface s_interface;
};

class text_input_v2::Private : public Wayland::Resource<text_input_v2>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, text_input_v2* q);

    void sync(text_input_v2_state const& old);
    void send_enter(Surface* surface, quint32 serial);
    void send_leave(quint32 serial, Surface* surface);

    text_input_v2_state state;

    Seat* seat{nullptr};
    QPointer<Surface> surface;

    bool input_panel_visible{false};
    QRect overlapped_surface_area;

    std::string language;

private:
    static const struct zwp_text_input_v2_interface s_interface;

    static void enable_callback(wl_client* wlClient, wl_resource* wlResource, wl_resource* surface);
    static void
    disable_callback(wl_client* wlClient, wl_resource* wlResource, wl_resource* surface);
    static void update_state_callback(wl_client* wlClient,
                                      wl_resource* wlResource,
                                      uint32_t serial,
                                      uint32_t reason);
    static void show_input_panel_callback(wl_client* wlClient, wl_resource* wlResource);
    static void hide_input_panel_callback(wl_client* wlClient, wl_resource* wlResource);
    static void set_surrounding_text_callback(wl_client* wlClient,
                                              wl_resource* wlResource,
                                              char const* text,
                                              int32_t cursor,
                                              int32_t anchor);
    static void set_content_type_callback(wl_client* wlClient,
                                          wl_resource* wlResource,
                                          uint32_t hint,
                                          uint32_t purpose);
    static void set_cursor_rectangle_callback(wl_client* wlClient,
                                              wl_resource* wlResource,
                                              int32_t x,
                                              int32_t y,
                                              int32_t width,
                                              int32_t height);
    static void set_preferred_language_callback(wl_client* wlClient,
                                                wl_resource* wlResource,
                                                char const* language);

    void enable(Surface* s);
    void disable();

    text_input_v2* q_ptr;
};

}
