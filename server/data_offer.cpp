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
#include "selection_p.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

const struct wl_data_offer_interface DataOffer::Private::s_interface = {
    acceptCallback,
    receive_callback,
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

void DataOffer::Private::receive_callback(wl_client* /*wlClient*/,
                                          wl_resource* wlResource,
                                          char const* mimeType,
                                          int32_t fd)
{
    auto handle = Resource::handle(wlResource);
    receive_mime_type_offer(handle->d_ptr->source, mimeType, fd);
}

void DataOffer::Private::finishCallback([[maybe_unused]] wl_client* wlClient,
                                        wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->source->send_dnd_finished();
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

    dnd_action preferredAction = dnd_action::none;
    if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
        preferredAction = dnd_action::copy;
    } else if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
        preferredAction = dnd_action::move;
    } else if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
        preferredAction = dnd_action::ask;
    }

    auto priv = handle(wlResource)->d_ptr;
    priv->supportedDnDActions = supportedActions;
    priv->preferredDnDAction = preferredAction;
    Q_EMIT priv->q_ptr->dnd_actions_changed();
}

void DataOffer::Private::sendSourceActions()
{
    if (!source) {
        return;
    }

    uint32_t wlActions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    auto const actions = source->supported_dnd_actions();
    if (actions.testFlag(dnd_action::copy)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    }
    if (actions.testFlag(dnd_action::move)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    }
    if (actions.testFlag(dnd_action::ask)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }
    send<wl_data_offer_send_source_actions, WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION>(wlActions);
}

DataOffer::DataOffer(Client* client, uint32_t version, DataSource* source)
    : d_ptr(new Private(client, version, source, this))
{
    assert(source);
    connect(source, &DataSource::mime_type_offered, this, [this](std::string const& mimeType) {
        d_ptr->send<wl_data_offer_send_offer>(mimeType.c_str());
    });
    QObject::connect(
        source, &DataSource::resourceDestroyed, this, [this] { d_ptr->source = nullptr; });
}

void DataOffer::send_all_offers()
{
    for (auto const& mimeType : d_ptr->source->mime_types()) {
        d_ptr->send<wl_data_offer_send_offer>(mimeType.c_str());
    }
}

dnd_actions DataOffer::supported_dnd_actions() const
{
    return d_ptr->supportedDnDActions;
}

dnd_action DataOffer::preferred_dnd_action() const
{
    return d_ptr->preferredDnDAction;
}

void DataOffer::send_action(dnd_action action)
{
    uint32_t wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (action == dnd_action::copy) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    } else if (action == dnd_action::move) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    } else if (action == dnd_action::ask) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }
    d_ptr->send<wl_data_offer_send_action, WL_DATA_OFFER_ACTION_SINCE_VERSION>(wlAction);
}

}
