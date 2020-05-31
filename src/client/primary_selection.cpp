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
#include "primary_selection_p.h"
#include "seat.h"

#include <QMimeDatabase>

namespace Wrapland::Client
{

const zwp_primary_selection_device_v1_listener PrimarySelectionDevice::Private::s_listener = {
    dataOfferCallback,
    selectionCallback,
};

void PrimarySelectionDevice::Private::dataOfferCallback(void* data,
                                                        zwp_primary_selection_device_v1* dataDevice,
                                                        zwp_primary_selection_offer_v1* id)
{
    auto priv = reinterpret_cast<Private*>(data);
    Q_ASSERT(priv->device == dataDevice);
    priv->dataOffer(id);
}

void PrimarySelectionDevice::Private::dataOffer(zwp_primary_selection_offer_v1* id)
{
    Q_ASSERT(!lastOffer);
    lastOffer = new PrimarySelectionOffer(q_ptr, id);
    Q_ASSERT(lastOffer->isValid());
}

void PrimarySelectionDevice::Private::selectionCallback(void* data,
                                                        zwp_primary_selection_device_v1* dataDevice,
                                                        zwp_primary_selection_offer_v1* id)
{
    auto priv = reinterpret_cast<Private*>(data);
    Q_ASSERT(priv->device == dataDevice);
    priv->selection(id);
}

void PrimarySelectionDevice::Private::selection(zwp_primary_selection_offer_v1* id)
{
    if (!id) {
        selectionOffer.reset();
        Q_EMIT q_ptr->selectionCleared();
        return;
    }
    Q_ASSERT(*lastOffer == id);
    selectionOffer.reset(lastOffer);
    lastOffer = nullptr;
    Q_EMIT q_ptr->selectionOffered(selectionOffer.get());
}

PrimarySelectionDevice::Private::Private(PrimarySelectionDevice* q)
    : q_ptr(q)
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
static zwp_primary_selection_source_v1* dataSource(const PrimarySelectionSource* source)
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

PrimarySelectionSource* PrimarySelectionDeviceManager::createPrimarySelectionSource(QObject* parent)
{
    Q_ASSERT(isValid());
    PrimarySelectionSource* s = new PrimarySelectionSource(parent);
    auto w = zwp_primary_selection_device_manager_v1_create_source(d_ptr->manager);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(w);
    }
    s->setup(w);
    return s;
}

PrimarySelectionDevice* PrimarySelectionDeviceManager::getPrimarySelectionDevice(Seat* seat,
                                                                                 QObject* parent)
{
    Q_ASSERT(isValid());
    Q_ASSERT(seat);
    PrimarySelectionDevice* device = new PrimarySelectionDevice(parent);
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
    offerCallback,
};

PrimarySelectionOffer::Private::Private(zwp_primary_selection_offer_v1* offer,
                                        PrimarySelectionOffer* q)
    : q_ptr(q)
{
    dataOffer.setup(offer);
    zwp_primary_selection_offer_v1_add_listener(offer, &s_listener, this);
}

void PrimarySelectionOffer::Private::offerCallback(void* data,
                                                   zwp_primary_selection_offer_v1* dataOffer,
                                                   const char* mimeType)
{
    auto priv = reinterpret_cast<Private*>(data);
    Q_ASSERT(priv->dataOffer == dataOffer);
    priv->offer(QString::fromUtf8(mimeType));
}

void PrimarySelectionOffer::Private::offer(const QString& mimeType)
{
    QMimeDatabase db;
    const auto& m = db.mimeTypeForName(mimeType);
    if (m.isValid()) {
        mimeTypes << m;
        Q_EMIT q_ptr->mimeTypeOffered(m.name());
    }
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
    sendCallback,
    cancelledCallback,
};

PrimarySelectionSource::Private::Private(PrimarySelectionSource* q)
    : q_ptr(q)
{
}

void PrimarySelectionSource::Private::sendCallback(void* data,
                                                   zwp_primary_selection_source_v1* dataSource,
                                                   const char* mimeType,
                                                   int32_t fd)
{
    auto priv = reinterpret_cast<PrimarySelectionSource::Private*>(data);
    Q_ASSERT(priv->source == dataSource);
    Q_EMIT priv->q_ptr->sendDataRequested(QString::fromUtf8(mimeType), fd);
}

void PrimarySelectionSource::Private::cancelledCallback(void* data,
                                                        zwp_primary_selection_source_v1* dataSource)
{
    auto priv = reinterpret_cast<PrimarySelectionSource::Private*>(data);
    Q_ASSERT(priv->source == dataSource);
    Q_EMIT priv->q_ptr->cancelled();
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
