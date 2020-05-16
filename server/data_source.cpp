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

#include <unistd.h>

namespace Wrapland
{
namespace Server
{

DataSource::Private::Private(Client* client, uint32_t version, uint32_t id, DataSource* q)
    : Wayland::Resource<DataSource>(client, version, id, &wl_data_source_interface, &s_interface, q)
    , q_ptr{q}
{
    if (version < WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        supportedDnDActions = DataDeviceManager::DnDAction::Copy;
    }
}

const struct wl_data_source_interface DataSource::Private::s_interface = {
    offerCallback,
    destroyCallback,
    setActionsCallback,
};

void DataSource::Private::offerCallback([[maybe_unused]] wl_client* wlClient,
                                        wl_resource* wlResource,
                                        const char* mimeType)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->offer(mimeType);
}

void DataSource::Private::offer(std::string mimeType)
{
    mimeTypes.push_back(mimeType);
    Q_EMIT q_ptr->mimeTypeOffered(mimeType);
}

void DataSource::Private::setActionsCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             uint32_t dnd_actions)
{
    DataDeviceManager::DnDActions supportedActions;
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
        supportedActions |= DataDeviceManager::DnDAction::Copy;
    }
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
        supportedActions |= DataDeviceManager::DnDAction::Move;
    }
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
        supportedActions |= DataDeviceManager::DnDAction::Ask;
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
        Q_EMIT priv->q_ptr->supportedDragAndDropActionsChanged();
    }
}

DataSource::DataSource(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}

void DataSource::accept(std::string mimeType)
{
    // TODO: does this require a sanity check on the possible mimeType?
    d_ptr->send<wl_data_source_send_target>(mimeType.empty() ? nullptr : mimeType.c_str());
}

void DataSource::requestData(std::string mimeType, int32_t fd)
{
    // TODO: does this require a sanity check on the possible mimeType?
    d_ptr->send<wl_data_source_send_send>(mimeType.c_str(), fd);
    close(fd);
}

void DataSource::cancel()
{
    d_ptr->send<wl_data_source_send_cancelled>();
    d_ptr->client()->flush();
}

std::vector<std::string> DataSource::mimeTypes() const
{
    return d_ptr->mimeTypes;
}

DataDeviceManager::DnDActions DataSource::supportedDragAndDropActions() const
{
    return d_ptr->supportedDnDActions;
}

void DataSource::dropPerformed()
{
    d_ptr->send<wl_data_source_send_dnd_drop_performed,
                WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION>();
}

void DataSource::dndFinished()
{
    d_ptr->send<wl_data_source_send_dnd_finished, WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION>();
}

void DataSource::dndAction(DataDeviceManager::DnDAction action)
{
    uint32_t wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

    if (action == DataDeviceManager::DnDAction::Copy) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    } else if (action == DataDeviceManager::DnDAction::Move) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    } else if (action == DataDeviceManager::DnDAction::Ask) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }

    d_ptr->send<wl_data_source_send_action, WL_DATA_SOURCE_ACTION_SINCE_VERSION>(wlAction);
}

}
}
