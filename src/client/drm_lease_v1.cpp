/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "drm_lease_v1_p.h"

#include "event_queue.h"
#include "surface.h"

namespace Wrapland::Client
{

wp_drm_lease_device_v1_listener const drm_lease_device_v1::Private::s_listener = {
    drm_fd_callback,
    connector_callback,
    done_callback,
    released_callback,
};

void drm_lease_device_v1::Private::drm_fd_callback(
    void* data,
    [[maybe_unused]] wp_drm_lease_device_v1* wp_drm_lease_device_v1,
    int fd)
{
    auto priv = reinterpret_cast<drm_lease_device_v1::Private*>(data);
    assert(priv->device_ptr == wp_drm_lease_device_v1);
    priv->drm_fd = fd;
}

void drm_lease_device_v1::Private::connector_callback(
    void* data,
    [[maybe_unused]] wp_drm_lease_device_v1* wp_drm_lease_device_v1,
    wp_drm_lease_connector_v1* wp_drm_lease_connector_v1)
{
    auto priv = reinterpret_cast<drm_lease_device_v1::Private*>(data);
    assert(priv->device_ptr == wp_drm_lease_device_v1);

    if (!priv->q_ptr) {
        return;
    }

    auto connector = new drm_lease_connector_v1();
    connector->d_ptr->setup(wp_drm_lease_connector_v1);
    Q_EMIT priv->q_ptr->connector(connector);
}

void drm_lease_device_v1::Private::done_callback(
    void* data,
    [[maybe_unused]] wp_drm_lease_device_v1* wp_drm_lease_device_v1)
{
    auto priv = reinterpret_cast<drm_lease_device_v1::Private*>(data);
    assert(priv->device_ptr == wp_drm_lease_device_v1);

    if (!priv->q_ptr) {
        return;
    }

    Q_EMIT priv->q_ptr->done();
}

void drm_lease_device_v1::Private::released_callback(
    void* data,
    [[maybe_unused]] wp_drm_lease_device_v1* wp_drm_lease_device_v1)
{
    auto priv = reinterpret_cast<drm_lease_device_v1::Private*>(data);
    assert(priv->device_ptr == wp_drm_lease_device_v1);

    assert(!priv->q_ptr);
    assert(priv->isValid());
    delete priv;
}

drm_lease_device_v1::Private::Private(drm_lease_device_v1* q)
    : q_ptr{q}
{
}

drm_lease_device_v1::Private::~Private()
{
    release();
}

void drm_lease_device_v1::Private::setup(wp_drm_lease_device_v1* device)
{
    assert(device);
    assert(!device_ptr);
    device_ptr.setup(device);
    wp_drm_lease_device_v1_add_listener(device, &s_listener, this);
}

void drm_lease_device_v1::Private::release()
{
    if (isValid() && q_ptr) {
        // Need to wait for released callback.
        assert(q_ptr);
        wp_drm_lease_device_v1_release(device_ptr);
    } else {
        device_ptr.release();
    }

    q_ptr = nullptr;
}

bool drm_lease_device_v1::Private::isValid()
{
    return device_ptr.isValid();
}

drm_lease_v1*
drm_lease_device_v1::Private::create_lease(std::vector<drm_lease_connector_v1*> const& connectors)
{
    assert(isValid());

    auto request = wp_drm_lease_device_v1_create_lease_request(device_ptr);

    for (auto& con : connectors) {
        wp_drm_lease_request_v1_request_connector(request, *con);
    }

    auto lease = new drm_lease_v1();
    auto wllease = wp_drm_lease_request_v1_submit(request);

    if (queue) {
        queue->addProxy(wllease);
    }
    lease->setup(wllease);

    return lease;
}

drm_lease_device_v1::drm_lease_device_v1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

drm_lease_device_v1::~drm_lease_device_v1()
{
    if (d_ptr) {
        d_ptr->release();
    }
}

void drm_lease_device_v1::setup(wp_drm_lease_device_v1* device)
{
    d_ptr->setup(device);
}

void drm_lease_device_v1::release()
{
    d_ptr->release();
    d_ptr = nullptr;
}

void drm_lease_device_v1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* drm_lease_device_v1::event_queue()
{
    return d_ptr->queue;
}

drm_lease_device_v1::operator wp_drm_lease_device_v1*()
{
    return *d_ptr;
}

drm_lease_device_v1::operator wp_drm_lease_device_v1*() const
{
    return *d_ptr;
}

bool drm_lease_device_v1::isValid() const
{
    return d_ptr->isValid();
}

int drm_lease_device_v1::drm_fd()
{
    return d_ptr->drm_fd;
}

drm_lease_v1*
drm_lease_device_v1::create_lease(std::vector<drm_lease_connector_v1*> const& connectors)
{
    return d_ptr->create_lease(connectors);
}

/**
 * drm_lease_connector_v1
 */

drm_lease_connector_v1::Private::Private(drm_lease_connector_v1* q)
    : q_ptr{q}
{
}

wp_drm_lease_connector_v1_listener const drm_lease_connector_v1::Private::s_listener = {
    name_callback,
    description_callback,
    connector_id_callback,
    done_callback,
    withdrawn_callback,
};

void drm_lease_connector_v1::Private::name_callback(
    void* data,
    wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
    char const* text)
{
    auto priv = reinterpret_cast<drm_lease_connector_v1::Private*>(data);
    assert(priv->connector_ptr == wp_drm_lease_connector_v1);
    priv->data.name = text;
}

void drm_lease_connector_v1::Private::description_callback(
    void* data,
    wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
    char const* text)
{
    auto priv = reinterpret_cast<drm_lease_connector_v1::Private*>(data);
    assert(priv->connector_ptr == wp_drm_lease_connector_v1);
    priv->data.description = text;
}

void drm_lease_connector_v1::Private::connector_id_callback(
    void* data,
    wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
    uint32_t id)
{
    auto priv = reinterpret_cast<drm_lease_connector_v1::Private*>(data);
    assert(priv->connector_ptr == wp_drm_lease_connector_v1);
    priv->data.id = id;
}

void drm_lease_connector_v1::Private::done_callback(
    void* data,
    wp_drm_lease_connector_v1* wp_drm_lease_connector_v1)
{
    auto priv = reinterpret_cast<drm_lease_connector_v1::Private*>(data);
    assert(priv->connector_ptr == wp_drm_lease_connector_v1);
    priv->data.enabled = true;
    Q_EMIT priv->q_ptr->done();
}

void drm_lease_connector_v1::Private::withdrawn_callback(
    void* data,
    wp_drm_lease_connector_v1* wp_drm_lease_connector_v1)
{
    auto priv = reinterpret_cast<drm_lease_connector_v1::Private*>(data);
    assert(priv->connector_ptr == wp_drm_lease_connector_v1);
    priv->data.enabled = false;
    Q_EMIT priv->q_ptr->done();
}

void drm_lease_connector_v1::Private::setup(wp_drm_lease_connector_v1* connector)
{
    assert(connector);
    assert(!connector_ptr);
    connector_ptr.setup(connector);
    wp_drm_lease_connector_v1_add_listener(connector, &s_listener, this);
}

bool drm_lease_connector_v1::Private::isValid() const
{
    return connector_ptr.isValid();
}

drm_lease_connector_v1::drm_lease_connector_v1()
    : QObject()
    , d_ptr(new Private(this))
{
}

drm_lease_connector_v1::~drm_lease_connector_v1()
{
    release();
}

void drm_lease_connector_v1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* drm_lease_connector_v1::event_queue() const
{
    return d_ptr->queue;
}

bool drm_lease_connector_v1::isValid() const
{
    return d_ptr->isValid();
}

drm_lease_connector_v1_data const& drm_lease_connector_v1::data() const
{
    return d_ptr->data;
}

void drm_lease_connector_v1::setup(wp_drm_lease_connector_v1* connector)
{
    d_ptr->setup(connector);
}

void drm_lease_connector_v1::release()
{
    d_ptr->connector_ptr.release();
}

drm_lease_connector_v1::operator wp_drm_lease_connector_v1*()
{
    return d_ptr->connector_ptr;
}

drm_lease_connector_v1::operator wp_drm_lease_connector_v1*() const
{
    return d_ptr->connector_ptr;
}

/**
 * drm_lease_v1
 */

drm_lease_v1::Private::Private(drm_lease_v1* q)
    : q_ptr{q}
{
}

wp_drm_lease_v1_listener const drm_lease_v1::Private::s_listener = {
    lease_fd_callback,
    finished_callback,
};

void drm_lease_v1::Private::lease_fd_callback(void* data,
                                              wp_drm_lease_v1* wp_drm_lease_v1,
                                              int leased_fd)
{
    auto priv = reinterpret_cast<drm_lease_v1::Private*>(data);
    assert(priv->lease_ptr == wp_drm_lease_v1);
    Q_EMIT priv->q_ptr->leased_fd(leased_fd);
}

void drm_lease_v1::Private::finished_callback(void* data, wp_drm_lease_v1* wp_drm_lease_v1)
{
    auto priv = reinterpret_cast<drm_lease_v1::Private*>(data);
    assert(priv->lease_ptr == wp_drm_lease_v1);
    Q_EMIT priv->q_ptr->finished();
}

void drm_lease_v1::Private::setup(wp_drm_lease_v1* lease)
{
    assert(lease);
    assert(!lease_ptr);
    lease_ptr.setup(lease);
    wp_drm_lease_v1_add_listener(lease, &s_listener, this);
}

bool drm_lease_v1::Private::isValid() const
{
    return lease_ptr.isValid();
}

drm_lease_v1::drm_lease_v1()
    : QObject()
    , d_ptr(new Private(this))
{
}

drm_lease_v1::~drm_lease_v1()
{
    release();
}

void drm_lease_v1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* drm_lease_v1::event_queue() const
{
    return d_ptr->queue;
}

bool drm_lease_v1::isValid() const
{
    return d_ptr->isValid();
}

void drm_lease_v1::setup(wp_drm_lease_v1* connector)
{
    d_ptr->setup(connector);
}

void drm_lease_v1::release()
{
    d_ptr->lease_ptr.release();
}

drm_lease_v1::operator wp_drm_lease_v1*()
{
    return d_ptr->lease_ptr;
}

drm_lease_v1::operator wp_drm_lease_v1*() const
{
    return d_ptr->lease_ptr;
}

}
