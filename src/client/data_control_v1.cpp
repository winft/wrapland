/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "data_control_v1_p.h"

#include "seat.h"
#include "selection_device_p.h"
#include "selection_offer_p.h"
#include "selection_source_p.h"

namespace Wrapland::Client
{

data_control_manager_v1::data_control_manager_v1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private)
{
}

data_control_manager_v1::~data_control_manager_v1()
{
    release();
}

void data_control_manager_v1::release()
{
    d_ptr->manager.release();
}

bool data_control_manager_v1::isValid() const
{
    return d_ptr->manager.isValid();
}

void data_control_manager_v1::setup(zwlr_data_control_manager_v1* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!d_ptr->manager.isValid());
    d_ptr->manager.setup(manager);
}

EventQueue* data_control_manager_v1::eventQueue()
{
    return d_ptr->queue;
}

void data_control_manager_v1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

data_control_source_v1* data_control_manager_v1::create_source(QObject* parent)
{
    Q_ASSERT(isValid());
    auto* s = new data_control_source_v1(parent);
    auto w = zwlr_data_control_manager_v1_create_data_source(d_ptr->manager);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(w);
    }
    s->setup(w);
    return s;
}

data_control_device_v1* data_control_manager_v1::get_device(Seat* seat, QObject* parent)
{
    Q_ASSERT(isValid());
    Q_ASSERT(seat);
    auto* device = new data_control_device_v1(parent);
    auto w = zwlr_data_control_manager_v1_get_data_device(d_ptr->manager, *seat);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(w);
    }
    device->setup(w);
    return device;
}

data_control_manager_v1::operator zwlr_data_control_manager_v1*() const
{
    return d_ptr->manager;
}

data_control_manager_v1::operator zwlr_data_control_manager_v1*()
{
    return d_ptr->manager;
}

const zwlr_data_control_device_v1_listener data_control_device_v1::Private::s_listener = {
    data_offer_callback<Private>,
    selection_callback<Private>,
    finished_callback,
    primary_selection_callback,
};

data_control_device_v1::Private::Private(data_control_device_v1* ptr)
    : q(ptr)
{
}

void data_control_device_v1::Private::setup(zwlr_data_control_device_v1* d)
{
    Q_ASSERT(d);
    Q_ASSERT(!device.isValid());
    device.setup(d);
    zwlr_data_control_device_v1_add_listener(device, &s_listener, this);
}

data_control_device_v1::data_control_device_v1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

data_control_device_v1::~data_control_device_v1()
{
    release();
}

void data_control_device_v1::release()
{
    d_ptr->device.release();
}

bool data_control_device_v1::isValid() const
{
    return d_ptr->device.isValid();
}

void data_control_device_v1::setup(zwlr_data_control_device_v1* dataDevice)
{
    d_ptr->setup(dataDevice);
}

void data_control_device_v1::Private::finished_callback(void* data,
                                                        zwlr_data_control_device_v1* device)
{
    auto priv = reinterpret_cast<data_control_device_v1::Private*>(data);
    assert(priv->device == device);

    Q_EMIT priv->q->finished();
}

void data_control_device_v1::Private::primary_selection_callback(
    void* data,
    zwlr_data_control_device_v1* device,
    zwlr_data_control_offer_v1* id)
{
    auto priv = reinterpret_cast<data_control_device_v1::Private*>(data);
    assert(priv->device == device);

    if (!id) {
        priv->primary_selection_offer.reset();
        Q_EMIT priv->q->primary_selection_offered(nullptr);
        return;
    }

    assert(*priv->lastOffer == id);

    priv->primary_selection_offer.reset(priv->lastOffer);
    priv->lastOffer = nullptr;

    Q_EMIT priv->q->primary_selection_offered(priv->primary_selection_offer.get());
}

void data_control_device_v1::set_selection(data_control_source_v1* source)
{
    zwlr_data_control_source_v1* wlr_src = nullptr;
    if (source) {
        wlr_src = *source;
    }
    zwlr_data_control_device_v1_set_selection(d_ptr->device, wlr_src);
}

void data_control_device_v1::set_primary_selection(data_control_source_v1* source)
{
    zwlr_data_control_source_v1* wlr_src = nullptr;
    if (source) {
        wlr_src = *source;
    }
    zwlr_data_control_device_v1_set_primary_selection(d_ptr->device, wlr_src);
}

data_control_offer_v1* data_control_device_v1::offered_selection() const
{
    return d_ptr->selectionOffer.get();
}

data_control_offer_v1* data_control_device_v1::offered_primary_selection() const
{
    return d_ptr->primary_selection_offer.get();
}

data_control_device_v1::operator zwlr_data_control_device_v1*()
{
    return d_ptr->device;
}

data_control_device_v1::operator zwlr_data_control_device_v1*() const
{
    return d_ptr->device;
}

const struct zwlr_data_control_offer_v1_listener data_control_offer_v1::Private::s_listener = {
    offer_callback<Private>,
};

data_control_offer_v1::Private::Private(zwlr_data_control_offer_v1* offer, data_control_offer_v1* q)
    : q(q)
{
    dataOffer.setup(offer);
    zwlr_data_control_offer_v1_add_listener(offer, &s_listener, this);
}

data_control_offer_v1::data_control_offer_v1(data_control_device_v1* parent,
                                             zwlr_data_control_offer_v1* dataOffer)
    : QObject(parent)
    , d_ptr(new Private(dataOffer, this))
{
}

data_control_offer_v1::~data_control_offer_v1()
{
    release();
}

void data_control_offer_v1::release()
{
    d_ptr->dataOffer.release();
}

bool data_control_offer_v1::isValid() const
{
    return d_ptr->dataOffer.isValid();
}

QList<QMimeType> data_control_offer_v1::offered_mime_types() const
{
    return d_ptr->mimeTypes;
}

void data_control_offer_v1::receive(QMimeType const& mimeType, int32_t fd)
{
    receive(mimeType.name(), fd);
}

void data_control_offer_v1::receive(QString const& mimeType, int32_t fd)
{
    Q_ASSERT(isValid());
    zwlr_data_control_offer_v1_receive(d_ptr->dataOffer, mimeType.toUtf8().constData(), fd);
}

data_control_offer_v1::operator zwlr_data_control_offer_v1*()
{
    return d_ptr->dataOffer;
}

data_control_offer_v1::operator zwlr_data_control_offer_v1*() const
{
    return d_ptr->dataOffer;
}

const zwlr_data_control_source_v1_listener data_control_source_v1::Private::s_listener = {
    send_callback<Private>,
    cancelled_callback<Private>,
};

data_control_source_v1::Private::Private(data_control_source_v1* ptr)
    : q(ptr)
{
}

void data_control_source_v1::Private::setup(zwlr_data_control_source_v1* s)
{
    Q_ASSERT(!source.isValid());
    Q_ASSERT(s);
    source.setup(s);
    zwlr_data_control_source_v1_add_listener(s, &s_listener, this);
}

data_control_source_v1::data_control_source_v1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

data_control_source_v1::~data_control_source_v1()
{
    release();
}

void data_control_source_v1::release()
{
    d_ptr->source.release();
}

bool data_control_source_v1::isValid() const
{
    return d_ptr->source.isValid();
}

void data_control_source_v1::setup(zwlr_data_control_source_v1* dataSource)
{
    d_ptr->setup(dataSource);
}

void data_control_source_v1::offer(QString const& mimeType)
{
    zwlr_data_control_source_v1_offer(d_ptr->source, mimeType.toUtf8().constData());
}

void data_control_source_v1::offer(QMimeType const& mimeType)
{
    if (!mimeType.isValid()) {
        return;
    }
    offer(mimeType.name());
}

data_control_source_v1::operator zwlr_data_control_source_v1*() const
{
    return d_ptr->source;
}

data_control_source_v1::operator zwlr_data_control_source_v1*()
{
    return d_ptr->source;
}

}
