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

#include "seat.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-primary-selection-unstable-v1-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t PrimarySelectionDeviceManagerVersion = 1;
using PrimarySelectionDeviceManagerGlobal
    = Wayland::Global<PrimarySelectionDeviceManager, PrimarySelectionDeviceManagerVersion>;

class PrimarySelectionDeviceManager::Private : public PrimarySelectionDeviceManagerGlobal
{
public:
    Private(Display* display, PrimarySelectionDeviceManager* q);
    ~Private() override;

private:
    std::vector<PrimarySelectionSource*> primarysource;

    static void createSourceCallback(wl_client* client, wl_resource* resource, uint32_t id);
    static void
    getDeviceCallback(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* seat);
    static const struct zwp_primary_selection_device_manager_v1_interface s_interface;
};

class PrimarySelectionDevice::Private : public Wayland::Resource<PrimarySelectionDevice>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Seat* seat,
            PrimarySelectionDevice* qptr);
    ~Private() override;

    PrimarySelectionSource* selection = nullptr;
    Seat* m_seat;

    PrimarySelectionOffer* sendDataOffer(PrimarySelectionSource* source);
    void setSelection(wl_resource* wlSource);

private:
    static void setSelectionCallback(wl_client* client,
                                     wl_resource* resource,
                                     wl_resource* source,
                                     uint32_t serial);

    static const struct zwp_primary_selection_device_v1_interface s_interface;
};

class PrimarySelectionOffer::Private : public Wayland::Resource<PrimarySelectionOffer>
{
public:
    Private(Client* client,
            uint32_t version,
            PrimarySelectionSource* source,
            PrimarySelectionOffer* qptr);
    ~Private() override;

    PrimarySelectionSource* source;

private:
    static void
    receiveCallback(wl_client* client, wl_resource* resource, const char* mimeType, int32_t fd);
    static const struct zwp_primary_selection_offer_v1_interface s_interface;
};

class PrimarySelectionSource::Private : public Wayland::Resource<PrimarySelectionSource>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, PrimarySelectionSource* qptr);
    ~Private() override;

    std::vector<std::string> mimeTypes;

private:
    static void offerCallback(wl_client* client, wl_resource* resource, const char* mimeType);

    static const struct zwp_primary_selection_source_v1_interface s_interface;
};

}
