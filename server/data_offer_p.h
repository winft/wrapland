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

#include "data_offer.h"
#include "data_source.h"

#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

class data_offer::Private : public Wayland::Resource<data_offer>
{
public:
    Private(Client* client, uint32_t version, data_source* source, data_offer* q_ptr);

    data_source* source;

    // Defaults are set to sensible values for < version 3 interfaces.
    dnd_actions supportedDnDActions = dnd_action::copy | dnd_action::move;
    dnd_action preferredDnDAction = dnd_action::copy;

    void send_source_actions();

private:
    static void acceptCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               uint32_t serial,
                               char const* mimeType);
    static void receive_callback(wl_client* wlClient,
                                 wl_resource* wlResource,
                                 char const* mimeType,
                                 int32_t fd);
    static void finishCallback(wl_client* wlClient, wl_resource* wlResource);
    static void setActionsCallback(wl_client* wlClient,
                                   wl_resource* wlResource,
                                   uint32_t dnd_actions,
                                   uint32_t preferred_action);

    static const struct wl_data_offer_interface s_interface;

    data_offer* q_ptr;
};

}
