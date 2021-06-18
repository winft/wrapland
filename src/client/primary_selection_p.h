/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "event_queue.h"
#include "primary_selection.h"
#include "surface.h"
#include "wayland_pointer_p.h"

// Qt
#include <QPointer>
// Wayland
#include <wayland-primary-selection-unstable-v1-client-protocol.h>

#include <QObject>

struct zwp_primary_selection_device_manager_v1;
struct zwp_primary_selection_device_v1;
struct zwp_primary_selection_offer_v1;
struct zwp_primary_selection_source_v1;

namespace Wrapland::Client
{

class EventQueue;
class PrimarySelectionSource;
class PrimarySelectionDevice;
class PrimarySelectionOffer;
class Seat;

class Q_DECL_HIDDEN PrimarySelectionDeviceManager::Private
{
public:
    WaylandPointer<zwp_primary_selection_device_manager_v1,
                   zwp_primary_selection_device_manager_v1_destroy>
        manager;
    EventQueue* queue = nullptr;
};

class Q_DECL_HIDDEN PrimarySelectionDevice::Private
{
public:
    explicit Private(PrimarySelectionDevice*);
    void setup(zwp_primary_selection_device_v1* d);

    WaylandPointer<zwp_primary_selection_device_v1, zwp_primary_selection_device_v1_destroy> device;
    std::unique_ptr<PrimarySelectionOffer> selectionOffer;

    PrimarySelectionDevice* q;
    PrimarySelectionOffer* lastOffer = nullptr;

private:
    static const struct zwp_primary_selection_device_v1_listener s_listener;
};

class Q_DECL_HIDDEN PrimarySelectionOffer::Private
{
public:
    Private(zwp_primary_selection_offer_v1*, PrimarySelectionOffer*);
    WaylandPointer<zwp_primary_selection_offer_v1, zwp_primary_selection_offer_v1_destroy>
        dataOffer;
    QList<QMimeType> mimeTypes;

    PrimarySelectionOffer* q;

private:
    static const struct zwp_primary_selection_offer_v1_listener s_listener;
};

class Q_DECL_HIDDEN PrimarySelectionSource::Private
{
public:
    explicit Private(PrimarySelectionSource*);
    void setup(zwp_primary_selection_source_v1* s);

    WaylandPointer<zwp_primary_selection_source_v1, zwp_primary_selection_source_v1_destroy> source;
    PrimarySelectionSource* q;

private:
    static const struct zwp_primary_selection_source_v1_listener s_listener;
};

}
