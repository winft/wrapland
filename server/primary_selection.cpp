/****************************************************************************
Copyright 2020  Adrien Faveraux <af@brain-networks.fr>

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
#include "client.h"
#include "display.h"
#include "primary_selection_p.h"
#include "seat_p.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <unistd.h>

namespace Wrapland::Server
{

const struct zwp_primary_selection_device_manager_v1_interface
    PrimarySelectionDeviceManager::Private::s_interface
    = {
        createSourceCallback,
        getDeviceCallback,
        resourceDestroyCallback,
};

PrimarySelectionDeviceManager::Private::Private(Display* display, PrimarySelectionDeviceManager* q)
    : PrimarySelectionDeviceManagerGlobal(q,
                                          display,
                                          &zwp_primary_selection_device_manager_v1_interface,
                                          &s_interface)
{
    create();
}

PrimarySelectionDeviceManager::Private::~Private() = default;

void PrimarySelectionDeviceManager::Private::createSourceCallback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    auto dataSource = new PrimarySelectionSource(bind->client()->handle(), bind->version(), id);
    if (!dataSource) {
        return;
    }

    Q_EMIT priv->handle()->primarySelectionSourceCreated(dataSource);
}

void PrimarySelectionDeviceManager::Private::getDeviceCallback([[maybe_unused]] wl_client* wlClient,
                                                               wl_resource* wlResource,
                                                               uint32_t id,
                                                               wl_resource* wlSeat)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);
    auto seat = SeatGlobal::handle(wlSeat);

    auto dataDevice
        = new PrimarySelectionDevice(bind->client()->handle(), bind->version(), id, seat);
    if (!dataDevice) {
        return;
    }

    seat->d_ptr->registerPrimarySelectionDevice(dataDevice);
    Q_EMIT priv->handle()->primarySelectionDeviceCreated(dataDevice);
}

PrimarySelectionDeviceManager::PrimarySelectionDeviceManager(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

PrimarySelectionDeviceManager::~PrimarySelectionDeviceManager() = default;

const struct zwp_primary_selection_device_v1_interface PrimarySelectionDevice::Private::s_interface
    = {
        setSelectionCallback,
        destroyCallback,
};

PrimarySelectionDevice::Private::Private(Client* client,
                                         uint32_t version,
                                         uint32_t id,
                                         Seat* seat,
                                         PrimarySelectionDevice* qptr)
    : Wayland::Resource<PrimarySelectionDevice>(client,
                                                version,
                                                id,
                                                &zwp_primary_selection_device_v1_interface,
                                                &s_interface,
                                                qptr)
    , m_seat(seat)
{
}

PrimarySelectionDevice::Private::~Private() = default;

void PrimarySelectionDevice::Private::setSelectionCallback([[maybe_unused]] wl_client* wlClient,
                                                           wl_resource* wlResource,
                                                           wl_resource* wlSource,
                                                           [[maybe_unused]] uint32_t serial)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->setSelection(wlSource);
}

void PrimarySelectionDevice::Private::setSelection(wl_resource* wlSource)
{
    auto dataSource = wlSource ? Resource<PrimarySelectionSource>::handle(wlSource) : nullptr;

    if (selection == dataSource) {
        return;
    }

    if (selection) {
        selection->cancel();
    }

    selection = dataSource;

    if (selection) {
        auto clearSelection = [this] { setSelection(nullptr); };
        connect(selection, &PrimarySelectionSource::resourceDestroyed, handle(), clearSelection);
        Q_EMIT handle()->selectionChanged(selection);
    } else {
        Q_EMIT handle()->selectionCleared();
    }
}

void PrimarySelectionDevice::sendSelection(PrimarySelectionDevice* device)
{
    auto deviceSelection = device->selection();
    if (!deviceSelection) {
        sendClearSelection();
        return;
    }

    auto offer = d_ptr->sendDataOffer(deviceSelection);
    if (!offer) {
        return;
    }

    d_ptr->send<zwp_primary_selection_device_v1_send_selection>(offer->d_ptr->resource());
}

void PrimarySelectionDevice::sendClearSelection()
{
    d_ptr->send<zwp_primary_selection_device_v1_send_selection>(nullptr);
}

PrimarySelectionOffer*
PrimarySelectionDevice::Private::sendDataOffer(PrimarySelectionSource* source)
{
    if (!source) {
        // A data offer can only exist together with a source.
        return nullptr;
    }

    auto offer = new PrimarySelectionOffer(client()->handle(), version(), source);

    if (!offer->d_ptr->resource()) {
        delete offer;
        return nullptr;
    }

    send<zwp_primary_selection_device_v1_send_data_offer>(offer->d_ptr->resource());
    offer->sendOffer();
    return offer;
}

PrimarySelectionDevice::PrimarySelectionDevice(Client* client,
                                               uint32_t version,
                                               uint32_t id,
                                               Seat* seat)
    : d_ptr(new Private(client, version, id, seat, this))
{
}

PrimarySelectionDevice::~PrimarySelectionDevice() = default;

PrimarySelectionSource* PrimarySelectionDevice::selection()
{
    return d_ptr->selection;
}

Client* PrimarySelectionDevice::client() const
{
    return d_ptr->client()->handle();
}

Seat* PrimarySelectionDevice::seat() const
{
    return d_ptr->m_seat;
}

const struct zwp_primary_selection_offer_v1_interface PrimarySelectionOffer::Private::s_interface
    = {
        receiveCallback,
        destroyCallback,
};

PrimarySelectionOffer::Private::Private(Client* client,
                                        uint32_t version,
                                        PrimarySelectionSource* source,
                                        PrimarySelectionOffer* qptr)
    : Wayland::Resource<PrimarySelectionOffer>(client,
                                               version,
                                               0,
                                               &zwp_primary_selection_offer_v1_interface,
                                               &s_interface,
                                               qptr)
    , source(source)
{
}

PrimarySelectionOffer::Private::~Private() = default;

void PrimarySelectionOffer::Private::receiveCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource,
                                                     const char* mimeType,
                                                     int32_t fd)
{
    auto priv = handle(wlResource)->d_ptr;
    if (!priv->source) {
        close(fd);
        return;
    }
    priv->source->requestData(mimeType, fd);
}

PrimarySelectionOffer::PrimarySelectionOffer(Client* client,
                                             uint32_t version,
                                             PrimarySelectionSource* source)
    : d_ptr(new Private(client, version, source, this))
{
    assert(source);
    connect(source,
            &PrimarySelectionSource::mimeTypeOffered,
            this,
            [this](std::string const& mimeType) {
                d_ptr->send<zwp_primary_selection_offer_v1_send_offer>(mimeType.c_str());
            });
    QObject::connect(source, &PrimarySelectionSource::resourceDestroyed, this, [this] {
        d_ptr->source = nullptr;
    });
}

PrimarySelectionOffer::~PrimarySelectionOffer() = default;

void PrimarySelectionOffer::sendOffer()
{
    for (auto const& mimeType : d_ptr->source->mimeTypes()) {
        d_ptr->send<zwp_primary_selection_offer_v1_send_offer>(mimeType.c_str());
    }
}

const struct zwp_primary_selection_source_v1_interface PrimarySelectionSource::Private::s_interface
    = {
        offerCallback,
        destroyCallback,
};

PrimarySelectionSource::Private::Private(Client* client,
                                         uint32_t version,
                                         uint32_t id,
                                         PrimarySelectionSource* qptr)
    : Wayland::Resource<PrimarySelectionSource>(client,
                                                version,
                                                id,
                                                &zwp_primary_selection_source_v1_interface,
                                                &s_interface,
                                                qptr)
{
}

PrimarySelectionSource::Private::~Private() = default;

void PrimarySelectionSource::Private::offerCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource,
                                                    const char* mimeType)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->mimeTypes.emplace_back(mimeType);
    Q_EMIT handle(wlResource)->mimeTypeOffered(mimeType);
}

PrimarySelectionSource::PrimarySelectionSource(Client* client, uint32_t version, uint32_t id)
    : d_ptr(new Private(client, version, id, this))
{
}

PrimarySelectionSource::~PrimarySelectionSource() = default;

std::vector<std::string> PrimarySelectionSource::mimeTypes()
{
    return d_ptr->mimeTypes;
}

void PrimarySelectionSource::cancel()
{
    d_ptr->send<zwp_primary_selection_source_v1_send_cancelled>();
    d_ptr->client()->flush();
}
void PrimarySelectionSource::requestData(std::string const& mimeType, qint32 fd)
{
    d_ptr->send<zwp_primary_selection_source_v1_send_send>(mimeType.c_str(), fd);
    close(fd);
}
Client* PrimarySelectionSource::client() const
{
    return d_ptr->client()->handle();
}

}
