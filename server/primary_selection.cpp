/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "primary_selection_p.h"

#include "client.h"
#include "display.h"
#include "seat_p.h"
#include "selection_p.h"

namespace Wrapland::Server
{

const struct zwp_primary_selection_device_manager_v1_interface
    PrimarySelectionDeviceManager::Private::s_interface
    = {
        cb<create_source>,
        cb<get_device>,
        resourceDestroyCallback,
};

PrimarySelectionDeviceManager::Private::Private(Display* display, PrimarySelectionDeviceManager* q)
    : device_manager<PrimarySelectionDeviceManagerGlobal>(
        q,
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

void PrimarySelectionDeviceManager::create_source(Client* client, uint32_t version, uint32_t id)
{
    auto source = new source_t(client, version, id);
    if (!source) {
        return;
    }

    Q_EMIT sourceCreated(source);
}

void PrimarySelectionDeviceManager::get_device(Client* client,
                                               uint32_t version,
                                               uint32_t id,
                                               Seat* seat)
{
    auto device = new PrimarySelectionDevice(client, version, id, seat);
    if (!device) {
        return;
    }

    seat->d_ptr->primary_selection_devices.register_device(device);
    Q_EMIT deviceCreated(device);
}

const struct zwp_primary_selection_device_v1_interface PrimarySelectionDevice::Private::s_interface
    = {
        set_selection_callback,
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

void PrimarySelectionDevice::Private::set_selection_callback(wl_client* /*wlClient*/,
                                                             wl_resource* wlResource,
                                                             wl_resource* wlSource,
                                                             uint32_t /*id*/)
{
    // TODO(unknown author): verify serial
    auto handle = Resource::handle(wlResource);
    set_selection(handle, handle->d_ptr, wlSource);
}

void PrimarySelectionDevice::sendSelection(Wrapland::Server::PrimarySelectionSource* source)
{
    if (!source) {
        sendClearSelection();
        return;
    }

    auto offer = d_ptr->sendDataOffer(source);
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
        receive_callback,
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

void PrimarySelectionOffer::Private::receive_callback(wl_client* /*wlClient*/,
                                                      wl_resource* wlResource,
                                                      char const* mimeType,
                                                      int32_t fd)
{
    auto handle = Resource::handle(wlResource);
    receive_mime_type_offer(handle->d_ptr->source, mimeType, fd);
}

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
        offer_callback,
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

void PrimarySelectionSource::Private::offer_callback(wl_client* /*wlClient*/,
                                                     wl_resource* wlResource,
                                                     char const* mimeType)
{
    auto handle = Resource::handle(wlResource);
    offer_mime_type(handle, handle->d_ptr, mimeType);
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
