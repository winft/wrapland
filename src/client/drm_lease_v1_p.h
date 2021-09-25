/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "drm_lease_v1.h"

#include "wayland_pointer_p.h"

#include <QObject>

#include <wayland-drm-lease-v1-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN drm_lease_device_v1::Private : public QObject
{
    Q_OBJECT
public:
    Private(drm_lease_device_v1* q);
    ~Private();

    void setup(wp_drm_lease_device_v1* deviced);

    void release();
    bool isValid();
    drm_lease_v1* create_lease(std::vector<drm_lease_connector_v1*> const& connectors);

    operator wp_drm_lease_device_v1*()
    {
        return device_ptr;
    }
    operator wp_drm_lease_device_v1*() const
    {
        return device_ptr;
    }

    int drm_fd{0};

    WaylandPointer<wp_drm_lease_device_v1, wp_drm_lease_device_v1_destroy> device_ptr;
    EventQueue* queue{nullptr};
    drm_lease_device_v1* q_ptr;

private:
    static void drm_fd_callback(void* data, wp_drm_lease_device_v1* wp_drm_lease_device_v1, int fd);
    static void connector_callback(void* data,
                                   wp_drm_lease_device_v1* wp_drm_lease_device_v1,
                                   wp_drm_lease_connector_v1* wp_drm_lease_connector_v1);
    static void done_callback(void* data, wp_drm_lease_device_v1* wp_drm_lease_device_v1);
    static void released_callback(void* data, wp_drm_lease_device_v1* wp_drm_lease_device_v1);

    static const wp_drm_lease_device_v1_listener s_listener;
};

class Q_DECL_HIDDEN drm_lease_connector_v1::Private
{
public:
    Private(drm_lease_connector_v1* q);
    ~Private() = default;

    void setup(wp_drm_lease_connector_v1* connector);

    bool isValid() const;

    drm_lease_connector_v1_data data;

    WaylandPointer<wp_drm_lease_connector_v1, wp_drm_lease_connector_v1_destroy> connector_ptr;

    EventQueue* queue{nullptr};

private:
    static void name_callback(void* data,
                              wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
                              char const* text);
    static void description_callback(void* data,
                                     wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
                                     char const* text);
    static void connector_id_callback(void* data,
                                      wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
                                      uint32_t id);
    static void done_callback(void* data, wp_drm_lease_connector_v1* wp_drm_lease_connector_v1);
    static void withdrawn_callback(void* data,
                                   wp_drm_lease_connector_v1* wp_drm_lease_connector_v1);

    drm_lease_connector_v1* q_ptr;

    static const wp_drm_lease_connector_v1_listener s_listener;
};

class Q_DECL_HIDDEN drm_lease_v1::Private
{
public:
    Private(drm_lease_v1* q);
    ~Private() = default;

    void setup(wp_drm_lease_v1* lease);

    bool isValid() const;

    void enable();
    void disable();
    void reset();

    WaylandPointer<wp_drm_lease_v1, wp_drm_lease_v1_destroy> lease_ptr;

    EventQueue* queue{nullptr};

private:
    static void lease_fd_callback(void* data, wp_drm_lease_v1* wp_drm_lease_v1, int leased_fd);
    static void finished_callback(void* data, wp_drm_lease_v1* wp_drm_lease_v1);

    drm_lease_v1* q_ptr;

    static const wp_drm_lease_v1_listener s_listener;
};

}
