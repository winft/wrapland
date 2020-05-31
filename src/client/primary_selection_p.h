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
#pragma once

#include "primary_selection.h"
#include "event_queue.h"
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
    explicit Private(PrimarySelectionDevice* q);
    void setup(zwp_primary_selection_device_v1* d);

    WaylandPointer<zwp_primary_selection_device_v1, zwp_primary_selection_device_v1_destroy> device;
    std::unique_ptr<PrimarySelectionOffer> selectionOffer;

private:
    void dataOffer(zwp_primary_selection_offer_v1* id);
    void selection(zwp_primary_selection_offer_v1* id);
    static void dataOfferCallback(void* data,
                                  zwp_primary_selection_device_v1* dataDevice,
                                  zwp_primary_selection_offer_v1* id);
    static void selectionCallback(void* data,
                                  zwp_primary_selection_device_v1* dataDevice,
                                  zwp_primary_selection_offer_v1* id);

    static const struct zwp_primary_selection_device_v1_listener s_listener;

    PrimarySelectionDevice* q_ptr;
    PrimarySelectionOffer* lastOffer = nullptr;
};

class Q_DECL_HIDDEN PrimarySelectionOffer::Private
{
public:
    Private(zwp_primary_selection_offer_v1* offer, PrimarySelectionOffer* q);
    WaylandPointer<zwp_primary_selection_offer_v1, zwp_primary_selection_offer_v1_destroy>
        dataOffer;
    QList<QMimeType> mimeTypes;

private:
    void offer(const QString& mimeType);
    static void
    offerCallback(void* data, zwp_primary_selection_offer_v1* dataOffer, const char* mimeType);
    PrimarySelectionOffer* q_ptr;

    static const struct zwp_primary_selection_offer_v1_listener s_listener;
};

class Q_DECL_HIDDEN PrimarySelectionSource::Private
{
public:
    explicit Private(PrimarySelectionSource* q);
    void setup(zwp_primary_selection_source_v1* s);

    WaylandPointer<zwp_primary_selection_source_v1, zwp_primary_selection_source_v1_destroy> source;

private:
    static void sendCallback(void* data,
                             zwp_primary_selection_source_v1* dataSource,
                             const char* mimeType,
                             int32_t fd);
    static void cancelledCallback(void* data, zwp_primary_selection_source_v1* dataSource);

    static const struct zwp_primary_selection_source_v1_listener s_listener;

    PrimarySelectionSource* q_ptr;
};

}
