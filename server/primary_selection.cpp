/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "client.h"
#include "display.h"
#include "primary_selection_p.h"
#include "seat_p.h"
#include "selection_device_manager_p.h"
#include "selection_device_p.h"
#include "selection_offer_p.h"
#include "selection_source_p.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <unistd.h>

namespace Wrapland::Server
{

const struct zwp_primary_selection_device_manager_v1_interface
    PrimarySelectionDeviceManager::Private::s_interface
    = {
        create_selection_source<PrimarySelectionDeviceManagerGlobal>,
        get_selection_device<PrimarySelectionDeviceManagerGlobal>,
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

PrimarySelectionDeviceManager::PrimarySelectionDeviceManager(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

PrimarySelectionDeviceManager::~PrimarySelectionDeviceManager() = default;

const struct zwp_primary_selection_device_v1_interface PrimarySelectionDevice::Private::s_interface
    = {
        set_selection_callback<Wayland::Resource<PrimarySelectionDevice>>,
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
        receive_selection_offer<Wayland::Resource<PrimarySelectionOffer>>,
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

PrimarySelectionOffer::PrimarySelectionOffer(Client* client,
                                             uint32_t version,
                                             PrimarySelectionSource* source)
    : d_ptr(new Private(client, version, source, this))
{
    assert(source);
    QObject::connect(source,
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
        add_offered_mime_type<Wayland::Resource<PrimarySelectionSource>>,
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
