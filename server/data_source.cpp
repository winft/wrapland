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
#include "data_source.h"
#include "data_source_p.h"

#include "data_device_manager.h"
#include "selection_p.h"

#include "wayland/resource.h"

#include <unistd.h>

namespace Wrapland::Server
{

DataSource::Private::Private(Client* client, uint32_t version, uint32_t id, DataSource* q)
    : Wayland::Resource<DataSource>(client, version, id, &wl_data_source_interface, &s_interface, q)
    , q_ptr{q}
{
    if (version < WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        supportedDnDActions = dnd_action::copy;
    }
}

const struct wl_data_source_interface DataSource::Private::s_interface = {
    offer_callback,
    destroyCallback,
    setActionsCallback,
};

void DataSource::Private::offer_callback(wl_client* /*wlClient*/,
                                         wl_resource* wlResource,
                                         char const* mimeType)
{
    auto handle = Resource::handle(wlResource);
    offer_mime_type(handle, handle->d_ptr, mimeType);
}

void DataSource::Private::setActionsCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             uint32_t dnd_actions)
{
    Server::dnd_actions supportedActions;
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
        supportedActions |= dnd_action::copy;
    }
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
        supportedActions |= dnd_action::move;
    }
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
        supportedActions |= dnd_action::ask;
    }
    // verify that the no other actions are sent
    if (dnd_actions
        & ~(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY | WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE
            | WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)) {
        wl_resource_post_error(
            wlResource, WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK, "Invalid action mask");
        return;
    }

    auto priv = handle(wlResource)->d_ptr;
    if (priv->supportedDnDActions != supportedActions) {
        priv->supportedDnDActions = supportedActions;
        Q_EMIT priv->q_ptr->supported_dnd_actions_changed();
    }
}

DataSource::DataSource(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}

void DataSource::accept(std::string const& mimeType)
{
    // TODO(unknown author): does this require a sanity check on the possible mimeType?
    d_ptr->send<wl_data_source_send_target>(mimeType.empty() ? nullptr : mimeType.c_str());
}

void DataSource::request_data(std::string const& mimeType, int32_t fd)
{
    // TODO(unknown author): does this require a sanity check on the possible mimeType?
    d_ptr->send<wl_data_source_send_send>(mimeType.c_str(), fd);
    close(fd);
}

void DataSource::cancel()
{
    d_ptr->send<wl_data_source_send_cancelled>();
    d_ptr->client()->flush();
}

std::vector<std::string> DataSource::mime_types() const
{
    return d_ptr->mimeTypes;
}

dnd_actions DataSource::supported_dnd_actions() const
{
    return d_ptr->supportedDnDActions;
}

void DataSource::send_dnd_drop_performed()
{
    d_ptr->send<wl_data_source_send_dnd_drop_performed,
                WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION>();
}

void DataSource::send_dnd_finished()
{
    d_ptr->send<wl_data_source_send_dnd_finished, WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION>();
}

void DataSource::send_action(dnd_action action)
{
    uint32_t wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

    if (action == dnd_action::copy) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    } else if (action == dnd_action::move) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    } else if (action == dnd_action::ask) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }

    d_ptr->send<wl_data_source_send_action, WL_DATA_SOURCE_ACTION_SINCE_VERSION>(wlAction);
}

Client* DataSource::client() const
{
    return d_ptr->client()->handle();
}

}
