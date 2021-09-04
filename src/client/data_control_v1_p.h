/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "data_control_v1.h"

#include "event_queue.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <QObject>
#include <wayland-wlr-data-control-v1-client-protocol.h>

struct zwlr_data_control_manager_v1;
struct zwlr_data_control_device_v1;
struct zwlr_data_control_offer_v1;
struct zwlr_data_control_source_v1;

namespace Wrapland::Client
{

class EventQueue;
class data_control_source_v1;
class data_control_device_v1;
class data_control_offer_v1;
class Seat;

class Q_DECL_HIDDEN data_control_manager_v1::Private
{
public:
    WaylandPointer<zwlr_data_control_manager_v1, zwlr_data_control_manager_v1_destroy> manager;
    EventQueue* queue = nullptr;
};

class Q_DECL_HIDDEN data_control_device_v1::Private
{
public:
    explicit Private(data_control_device_v1*);
    void setup(zwlr_data_control_device_v1* d);

    WaylandPointer<zwlr_data_control_device_v1, zwlr_data_control_device_v1_destroy> device;
    std::unique_ptr<data_control_offer_v1> selectionOffer;
    std::unique_ptr<data_control_offer_v1> primary_selection_offer;

    data_control_device_v1* q;
    data_control_offer_v1* lastOffer{nullptr};

private:
    static void finished_callback(void* data, zwlr_data_control_device_v1* device);
    static void primary_selection_callback(void* data,
                                           zwlr_data_control_device_v1* device,
                                           zwlr_data_control_offer_v1* id);

    static const struct zwlr_data_control_device_v1_listener s_listener;
};

class Q_DECL_HIDDEN data_control_offer_v1::Private
{
public:
    Private(zwlr_data_control_offer_v1*, data_control_offer_v1*);
    WaylandPointer<zwlr_data_control_offer_v1, zwlr_data_control_offer_v1_destroy> dataOffer;
    QList<QMimeType> mimeTypes;

    data_control_offer_v1* q;

private:
    static const struct zwlr_data_control_offer_v1_listener s_listener;
};

class Q_DECL_HIDDEN data_control_source_v1::Private
{
public:
    explicit Private(data_control_source_v1*);
    void setup(zwlr_data_control_source_v1* s);

    WaylandPointer<zwlr_data_control_source_v1, zwlr_data_control_source_v1_destroy> source;
    data_control_source_v1* q;

private:
    static const struct zwlr_data_control_source_v1_listener s_listener;
};

}
