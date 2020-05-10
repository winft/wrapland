/********************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

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
#pragma once

#include "output_configuration_v1.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include "wayland-output-device-v1-server-protocol.h"
#include "wayland-output-management-v1-server-protocol.h"

namespace Wrapland::Server
{

class OutputConfigurationV1::Private : public Wayland::Resource<OutputConfigurationV1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            OutputManagementV1* manager,
            OutputConfigurationV1* q);
    ~Private() override;

    void sendApplied();
    void sendFailed();
    void clearPendingChanges();

    bool hasPendingChanges(OutputDeviceV1* outputdevice) const;
    OutputChangesetV1* pendingChanges(OutputDeviceV1* outputdevice);

    OutputManagementV1* manager;
    QHash<OutputDeviceV1*, OutputChangesetV1*> changes;

private:
    static void enableCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               wl_resource* wlOutputDevice,
                               int32_t wlEnable);
    static void modeCallback(wl_client* wlClient,
                             wl_resource* wlResource,
                             wl_resource* wlOutputDevice,
                             int32_t mode_id);
    static void transformCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  wl_resource* wlOutputDevice,
                                  int32_t wlTransform);
    static void geometryCallback(wl_client* wlClient,
                                 wl_resource* wlResource,
                                 wl_resource* wlOutputDevice,
                                 wl_fixed_t x,
                                 wl_fixed_t y,
                                 wl_fixed_t width,
                                 wl_fixed_t height);
    static void applyCallback(wl_client* wlClient, wl_resource* wlResource);

    static const struct zkwinft_output_configuration_v1_interface s_interface;
};

}
