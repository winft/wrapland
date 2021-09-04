/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "primary_selection_p.h"

#include "client.h"
#include "display.h"
#include "seat_p.h"
#include "selection_p.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <unistd.h>

namespace Wrapland::Server
{

const struct zwp_primary_selection_device_manager_v1_interface
    PrimarySelectionDeviceManager::Private::s_interface
    = {
        cb<create_source>,
        cb<get_device>,
        resourceDestroyCallback,
};

PrimarySelectionDeviceManager::Private::Private(Display* display, PrimarySelectionDeviceManager* q)
    : device_manager<PrimarySelectionDeviceManagerGlobal>(
        q,
        display,
        &zwp_primary_selection_device_manager_v1_interface,
        &s_interface)
{
    create();
}

PrimarySelectionDeviceManager::Private::~Private() = default;

PrimarySelectionDeviceManager::PrimarySelectionDeviceManager(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

PrimarySelectionDeviceManager::~PrimarySelectionDeviceManager() = default;

void PrimarySelectionDeviceManager::create_source(Client* client, uint32_t version, uint32_t id)
{
    auto source = new source_t(client, version, id);
    if (!source) {
        return;
    }

    Q_EMIT sourceCreated(source);
}

void PrimarySelectionDeviceManager::get_device(Client* client,
                                               uint32_t version,
                                               uint32_t id,
                                               Seat* seat)
{
    auto device = new PrimarySelectionDevice(client, version, id, seat);
    if (!device) {
        return;
    }

    seat->d_ptr->primary_selection_devices.register_device(device);
    Q_EMIT deviceCreated(device);
}

}
