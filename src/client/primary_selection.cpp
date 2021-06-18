/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "primary_selection_p.h"
#include "seat.h"
#include "selection_device_p.h"
#include "selection_offer_p.h"
#include "selection_source_p.h"

namespace Wrapland::Client
{

const zwp_primary_selection_device_v1_listener PrimarySelectionDevice::Private::s_listener = {
    data_offer_callback<Private>,
    selection_callback<Private>,
};

PrimarySelectionDevice::Private::Private(PrimarySelectionDevice* ptr)
    : q(ptr)
{
}

void PrimarySelectionDevice::Private::setup(zwp_primary_selection_device_v1* d)
{
    Q_ASSERT(d);
    Q_ASSERT(!device.isValid());
    device.setup(d);
    zwp_primary_selection_device_v1_add_listener(device, &s_listener, this);
}

PrimarySelectionDevice::PrimarySelectionDevice(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

PrimarySelectionDevice::~PrimarySelectionDevice()
{
    release();
}

void PrimarySelectionDevice::release()
{
    d_ptr->device.release();
}

bool PrimarySelectionDevice::isValid() const
{
    return d_ptr->device.isValid();
}

void PrimarySelectionDevice::setup(zwp_primary_selection_device_v1* dataDevice)
{
    d_ptr->setup(dataDevice);
}

namespace
{
zwp_primary_selection_source_v1* dataSource(const PrimarySelectionSource* source)
{
    if (!source) {
        return nullptr;
    }
    return *source;
}
}

void PrimarySelectionDevice::setSelection(quint32 serial, PrimarySelectionSource* source)
{
    zwp_primary_selection_device_v1_set_selection(d_ptr->device, dataSource(source), serial);
}

void PrimarySelectionDevice::clearSelection(quint32 serial)
{
    setSelection(serial);
}

PrimarySelectionOffer* PrimarySelectionDevice::offeredSelection() const
{
    return d_ptr->selectionOffer.get();
}

PrimarySelectionDevice::operator zwp_primary_selection_device_v1*()
{
    return d_ptr->device;
}

PrimarySelectionDevice::operator zwp_primary_selection_device_v1*() const
{
    return d_ptr->device;
}

PrimarySelectionDeviceManager::PrimarySelectionDeviceManager(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private)
{
}

PrimarySelectionDeviceManager::~PrimarySelectionDeviceManager()
{
    release();
}

void PrimarySelectionDeviceManager::release()
{
    d_ptr->manager.release();
}

bool PrimarySelectionDeviceManager::isValid() const
{
    return d_ptr->manager.isValid();
}

void PrimarySelectionDeviceManager::setup(zwp_primary_selection_device_manager_v1* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!d_ptr->manager.isValid());
    d_ptr->manager.setup(manager);
}

EventQueue* PrimarySelectionDeviceManager::eventQueue()
{
    return d_ptr->queue;
}

void PrimarySelectionDeviceManager::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

PrimarySelectionSource* PrimarySelectionDeviceManager::createSource(QObject* parent)
{
    Q_ASSERT(isValid());
    auto* s = new PrimarySelectionSource(parent);
    auto w = zwp_primary_selection_device_manager_v1_create_source(d_ptr->manager);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(w);
    }
    s->setup(w);
    return s;
}

PrimarySelectionDevice* PrimarySelectionDeviceManager::getDevice(Seat* seat, QObject* parent)
{
    Q_ASSERT(isValid());
    Q_ASSERT(seat);
    auto* device = new PrimarySelectionDevice(parent);
    auto w = zwp_primary_selection_device_manager_v1_get_device(d_ptr->manager, *seat);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(w);
    }
    device->setup(w);
    return device;
}

PrimarySelectionDeviceManager::operator zwp_primary_selection_device_manager_v1*() const
{
    return d_ptr->manager;
}

PrimarySelectionDeviceManager::operator zwp_primary_selection_device_manager_v1*()
{
    return d_ptr->manager;
}

const struct zwp_primary_selection_offer_v1_listener PrimarySelectionOffer::Private::s_listener = {
    offer_callback<Private>,
};

PrimarySelectionOffer::Private::Private(zwp_primary_selection_offer_v1* offer,
                                        PrimarySelectionOffer* q)
    : q(q)
{
    dataOffer.setup(offer);
    zwp_primary_selection_offer_v1_add_listener(offer, &s_listener, this);
}

PrimarySelectionOffer::PrimarySelectionOffer(PrimarySelectionDevice* parent,
                                             zwp_primary_selection_offer_v1* dataOffer)
    : QObject(parent)
    , d_ptr(new Private(dataOffer, this))
{
}

PrimarySelectionOffer::~PrimarySelectionOffer()
{
    release();
}

void PrimarySelectionOffer::release()
{
    d_ptr->dataOffer.release();
}

bool PrimarySelectionOffer::isValid() const
{
    return d_ptr->dataOffer.isValid();
}

QList<QMimeType> PrimarySelectionOffer::offeredMimeTypes() const
{
    return d_ptr->mimeTypes;
}

void PrimarySelectionOffer::receive(const QMimeType& mimeType, qint32 fd)
{
    receive(mimeType.name(), fd);
}

void PrimarySelectionOffer::receive(const QString& mimeType, qint32 fd)
{
    Q_ASSERT(isValid());
    zwp_primary_selection_offer_v1_receive(d_ptr->dataOffer, mimeType.toUtf8().constData(), fd);
}

PrimarySelectionOffer::operator zwp_primary_selection_offer_v1*()
{
    return d_ptr->dataOffer;
}

PrimarySelectionOffer::operator zwp_primary_selection_offer_v1*() const
{
    return d_ptr->dataOffer;
}

const zwp_primary_selection_source_v1_listener PrimarySelectionSource::Private::s_listener = {
    send_callback<Private>,
    cancelled_callback<Private>,
};

PrimarySelectionSource::Private::Private(PrimarySelectionSource* ptr)
    : q(ptr)
{
}

void PrimarySelectionSource::Private::setup(zwp_primary_selection_source_v1* s)
{
    Q_ASSERT(!source.isValid());
    Q_ASSERT(s);
    source.setup(s);
    zwp_primary_selection_source_v1_add_listener(s, &s_listener, this);
}

PrimarySelectionSource::PrimarySelectionSource(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

PrimarySelectionSource::~PrimarySelectionSource()
{
    release();
}

void PrimarySelectionSource::release()
{
    d_ptr->source.release();
}

bool PrimarySelectionSource::isValid() const
{
    return d_ptr->source.isValid();
}

void PrimarySelectionSource::setup(zwp_primary_selection_source_v1* dataSource)
{
    d_ptr->setup(dataSource);
}

void PrimarySelectionSource::offer(const QString& mimeType)
{
    zwp_primary_selection_source_v1_offer(d_ptr->source, mimeType.toUtf8().constData());
}

void PrimarySelectionSource::offer(const QMimeType& mimeType)
{
    if (!mimeType.isValid()) {
        return;
    }
    offer(mimeType.name());
}

PrimarySelectionSource::operator zwp_primary_selection_source_v1*() const
{
    return d_ptr->source;
}

PrimarySelectionSource::operator zwp_primary_selection_source_v1*()
{
    return d_ptr->source;
}

}
