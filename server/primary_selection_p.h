/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "primary_selection.h"

#include "seat.h"
#include "selection_device_manager_p.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-primary-selection-unstable-v1-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t PrimarySelectionDeviceManagerVersion = 1;
using PrimarySelectionDeviceManagerGlobal
    = Wayland::Global<PrimarySelectionDeviceManager, PrimarySelectionDeviceManagerVersion>;

class PrimarySelectionDeviceManager::Private
    : public device_manager<PrimarySelectionDeviceManagerGlobal>
{
public:
    Private(Display* display, PrimarySelectionDeviceManager* q);
    ~Private() override;

private:
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

    Seat* m_seat;

    PrimarySelectionSource* selection = nullptr;
    QMetaObject::Connection selectionDestroyedConnection;

    PrimarySelectionOffer* sendDataOffer(PrimarySelectionSource* source);

private:
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
    static const struct zwp_primary_selection_offer_v1_interface s_interface;
};

class PrimarySelectionSource::Private : public Wayland::Resource<PrimarySelectionSource>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, PrimarySelectionSource* qptr);
    ~Private() override;

    std::vector<std::string> mimeTypes;

private:
    static const struct zwp_primary_selection_source_v1_interface s_interface;
};

}
