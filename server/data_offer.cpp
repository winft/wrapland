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
#include "data_offer.h"
#include "data_offer_p.h"

#include "data_device.h"
#include "data_source.h"

#include <QStringList>

#include <wayland-server.h>

#include <unistd.h>

namespace Wrapland::Server
{

const struct wl_data_offer_interface DataOffer::Private::s_interface = {
    acceptCallback,
    receiveCallback,
    destroyCallback,
    finishCallback,
    setActionsCallback,
};

DataOffer::Private::Private(Client* client, uint32_t version, DataSource* source, DataOffer* q)
    : Wayland::Resource<DataOffer>(client, version, 0, &wl_data_offer_interface, &s_interface, q)
    , source(source)
    , q_ptr{q}
{
    // TODO(unknown author): connect to new selections.
}

void DataOffer::Private::acceptCallback([[maybe_unused]] wl_client* wlClient,
                                        wl_resource* wlResource,
                                        [[maybe_unused]] uint32_t serial,
                                        char const* mimeType)
{
    // TODO(unknown author): verify serial?
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->source) {
        return;
    }
    priv->source->accept(mimeType ? mimeType : std::string());
}

void DataOffer::Private::receiveCallback([[maybe_unused]] wl_client* wlClient,
                                         wl_resource* wlResource,
                                         char const* mimeType,
                                         int32_t fd)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->receive(mimeType, fd);
}

void DataOffer::Private::receive(std::string const& mimeType, int32_t fd) const
{
    if (!source) {
        close(fd);
        return;
    }
    source->requestData(mimeType, fd);
}

void DataOffer::Private::finishCallback([[maybe_unused]] wl_client* wlClient,
                                        wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->source->dndFinished();
    // TODO(unknown author): It is a client error to perform other requests than
    //                       wl_data_offer.destroy after this one.
}

void DataOffer::Private::setActionsCallback(wl_client* wlClient,
                                            wl_resource* wlResource,
                                            uint32_t dnd_actions,
                                            uint32_t preferred_action)
{
    // TODO(unknown author): check it's drag and drop, otherwise send error
    Q_UNUSED(wlClient)
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
            wlResource, WL_DATA_OFFER_ERROR_INVALID_ACTION_MASK, "Invalid action mask");
        return;
    }
    if (preferred_action != WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY
        && preferred_action != WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE
        && preferred_action != WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK
        && preferred_action != WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE) {
        wl_resource_post_error(
            wlResource, WL_DATA_OFFER_ERROR_INVALID_ACTION, "Invalid preferred action");
        return;
    }

    DataDeviceManager::DnDAction preferredAction = DataDeviceManager::DnDAction::None;
    if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
        preferredAction = DataDeviceManager::DnDAction::Copy;
    } else if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
        preferredAction = DataDeviceManager::DnDAction::Move;
    } else if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
        preferredAction = DataDeviceManager::DnDAction::Ask;
    }

    auto priv = handle(wlResource)->d_ptr;
    priv->supportedDnDActions = supportedActions;
    priv->preferredDnDAction = preferredAction;
    Q_EMIT priv->q_ptr->dragAndDropActionsChanged();
}

void DataOffer::Private::sendSourceActions()
{
    if (!source) {
        return;
    }

    uint32_t wlActions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    auto const actions = source->supportedDragAndDropActions();
    if (actions.testFlag(DataDeviceManager::DnDAction::Copy)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    }
    if (actions.testFlag(DataDeviceManager::DnDAction::Move)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    }
    if (actions.testFlag(DataDeviceManager::DnDAction::Ask)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }
    send<wl_data_offer_send_source_actions, WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION>(wlActions);
}

DataOffer::DataOffer(Client* client, uint32_t version, DataSource* source)
    : d_ptr(new Private(client, version, source, this))
{
    assert(source);
    connect(source, &DataSource::mimeTypeOffered, this, [this](std::string const& mimeType) {
        d_ptr->send<wl_data_offer_send_offer>(mimeType.c_str());
    });
    QObject::connect(
        source, &DataSource::resourceDestroyed, this, [this] { d_ptr->source = nullptr; });
}

void DataOffer::sendAllOffers()
{
    for (auto const& mimeType : d_ptr->source->mimeTypes()) {
        d_ptr->send<wl_data_offer_send_offer>(mimeType.c_str());
    }
}

DataDeviceManager::DnDActions DataOffer::supportedDragAndDropActions() const
{
    return d_ptr->supportedDnDActions;
}

DataDeviceManager::DnDAction DataOffer::preferredDragAndDropAction() const
{
    return d_ptr->preferredDnDAction;
}

void DataOffer::dndAction(DataDeviceManager::DnDAction action)
{
    uint32_t wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (action == DataDeviceManager::DnDAction::Copy) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    } else if (action == DataDeviceManager::DnDAction::Move) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    } else if (action == DataDeviceManager::DnDAction::Ask) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }
    d_ptr->send<wl_data_offer_send_action, WL_DATA_OFFER_ACTION_SINCE_VERSION>(wlAction);
}

}
