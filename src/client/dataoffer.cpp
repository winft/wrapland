/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "dataoffer.h"
#include "datadevice.h"
#include "selection_offer_p.h"
#include "wayland_pointer_p.h"
// Qt
#include <QMimeDatabase>
#include <QMimeType>
// Wayland
#include <wayland-client-protocol.h>

namespace Wrapland
{

namespace Client
{

class Q_DECL_HIDDEN DataOffer::Private
{
public:
    Private(wl_data_offer* offer, DataOffer* q);
    WaylandPointer<wl_data_offer, wl_data_offer_destroy> dataOffer;
    QList<QMimeType> mimeTypes;
    DataDeviceManager::DnDActions sourceActions = DataDeviceManager::DnDAction::None;
    DataDeviceManager::DnDAction selectedAction = DataDeviceManager::DnDAction::None;

    void setAction(DataDeviceManager::DnDAction action);
    static void
    sourceActionsCallback(void* data, wl_data_offer* wl_data_offer, uint32_t source_actions);
    static void actionCallback(void* data, wl_data_offer* wl_data_offer, uint32_t dnd_action);
    DataOffer* q;

    static const struct wl_data_offer_listener s_listener;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct wl_data_offer_listener DataOffer::Private::s_listener
    = {offer_callback<Private>, sourceActionsCallback, actionCallback};
#endif

DataOffer::Private::Private(wl_data_offer* offer, DataOffer* q)
    : q(q)
{
    dataOffer.setup(offer);
    wl_data_offer_add_listener(offer, &s_listener, this);
}

void DataOffer::Private::sourceActionsCallback(void* data,
                                               wl_data_offer* wl_data_offer,
                                               uint32_t source_actions)
{
    Q_UNUSED(wl_data_offer)
    DataDeviceManager::DnDActions actions;
    if (source_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
        actions |= DataDeviceManager::DnDAction::Copy;
    }
    if (source_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
        actions |= DataDeviceManager::DnDAction::Move;
    }
    if (source_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
        actions |= DataDeviceManager::DnDAction::Ask;
    }
    auto d = reinterpret_cast<Private*>(data);
    if (d->sourceActions != actions) {
        d->sourceActions = actions;
        emit d->q->sourceDragAndDropActionsChanged();
    }
}

void DataOffer::Private::actionCallback(void* data,
                                        wl_data_offer* wl_data_offer,
                                        uint32_t dnd_action)
{
    Q_UNUSED(wl_data_offer)
    auto d = reinterpret_cast<Private*>(data);
    switch (dnd_action) {
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
        d->setAction(DataDeviceManager::DnDAction::Copy);
        break;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE:
        d->setAction(DataDeviceManager::DnDAction::Move);
        break;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK:
        d->setAction(DataDeviceManager::DnDAction::Ask);
        break;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE:
        d->setAction(DataDeviceManager::DnDAction::None);
        break;
    default:
        Q_UNREACHABLE();
    }
}

void DataOffer::Private::setAction(DataDeviceManager::DnDAction action)
{
    if (action == selectedAction) {
        return;
    }
    selectedAction = action;
    emit q->selectedDragAndDropActionChanged();
}

DataOffer::DataOffer(DataDevice* parent, wl_data_offer* dataOffer)
    : QObject(parent)
    , d(new Private(dataOffer, this))
{
}

DataOffer::~DataOffer()
{
    release();
}

void DataOffer::release()
{
    d->dataOffer.release();
}

bool DataOffer::isValid() const
{
    return d->dataOffer.isValid();
}

QList<QMimeType> DataOffer::offeredMimeTypes() const
{
    return d->mimeTypes;
}

void DataOffer::receive(QMimeType const& mimeType, qint32 fd)
{
    receive(mimeType.name(), fd);
}

void DataOffer::receive(QString const& mimeType, qint32 fd)
{
    Q_ASSERT(isValid());
    wl_data_offer_receive(d->dataOffer, mimeType.toUtf8().constData(), fd);
}

DataOffer::operator wl_data_offer*()
{
    return d->dataOffer;
}

DataOffer::operator wl_data_offer*() const
{
    return d->dataOffer;
}

void DataOffer::dragAndDropFinished()
{
    Q_ASSERT(isValid());
    if (wl_proxy_get_version(d->dataOffer) < WL_DATA_OFFER_FINISH_SINCE_VERSION) {
        return;
    }
    wl_data_offer_finish(d->dataOffer);
}

DataDeviceManager::DnDActions DataOffer::sourceDragAndDropActions() const
{
    return d->sourceActions;
}

void DataOffer::setDragAndDropActions(DataDeviceManager::DnDActions supported,
                                      DataDeviceManager::DnDAction preferred)
{
    if (wl_proxy_get_version(d->dataOffer) < WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
        return;
    }
    auto toWayland = [](DataDeviceManager::DnDAction action) {
        switch (action) {
        case DataDeviceManager::DnDAction::Copy:
            return WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
        case DataDeviceManager::DnDAction::Move:
            return WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
        case DataDeviceManager::DnDAction::Ask:
            return WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
        case DataDeviceManager::DnDAction::None:
            return WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
        default:
            Q_UNREACHABLE();
        }
    };
    uint32_t wlSupported = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (supported.testFlag(DataDeviceManager::DnDAction::Copy)) {
        wlSupported |= toWayland(DataDeviceManager::DnDAction::Copy);
    }
    if (supported.testFlag(DataDeviceManager::DnDAction::Move)) {
        wlSupported |= toWayland(DataDeviceManager::DnDAction::Move);
    }
    if (supported.testFlag(DataDeviceManager::DnDAction::Ask)) {
        wlSupported |= toWayland(DataDeviceManager::DnDAction::Ask);
    }
    wl_data_offer_set_actions(d->dataOffer, wlSupported, toWayland(preferred));
}

DataDeviceManager::DnDAction DataOffer::selectedDragAndDropAction() const
{
    return d->selectedAction;
}

}
}
