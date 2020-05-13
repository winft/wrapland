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
#include "output_configuration_v1_p.h"

#include "output_changeset_v1_p.h"
#include "output_device_v1_p.h"
#include "output_management_v1.h"

#include "logging.h"

#include <QSize>

namespace Wrapland::Server
{

const struct zkwinft_output_configuration_v1_interface OutputConfigurationV1::Private::s_interface
    = {
        destroyCallback,
        enableCallback,
        modeCallback,
        transformCallback,
        geometryCallback,
        applyCallback,
};

OutputConfigurationV1::Private::Private(Client* client,
                                        uint32_t version,
                                        uint32_t id,
                                        OutputManagementV1* manager,
                                        OutputConfigurationV1* q)
    : Wayland::Resource<OutputConfigurationV1>(client,
                                               version,
                                               id,
                                               &zkwinft_output_configuration_v1_interface,
                                               &s_interface,
                                               q)
    , manager{manager}
{
}

OutputConfigurationV1::Private::~Private() = default;

void OutputConfigurationV1::Private::enableCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource,
                                                    wl_resource* wlOutputDevice,
                                                    int32_t wlEnable)
{
    auto priv = handle(wlResource)->d_ptr;
    auto outputDevice = OutputDeviceV1Global::handle(wlOutputDevice);

    auto enable = (wlEnable == ZKWINFT_OUTPUT_DEVICE_V1_ENABLEMENT_ENABLED)
        ? OutputDeviceV1::Enablement::Enabled
        : OutputDeviceV1::Enablement::Disabled;

    priv->pendingChanges(outputDevice)->d_ptr->enabled = enable;
}

void OutputConfigurationV1::Private::modeCallback([[maybe_unused]] wl_client* wlClient,
                                                  wl_resource* wlResource,
                                                  wl_resource* wlOutputDevice,
                                                  int32_t mode_id)
{
    auto priv = handle(wlResource)->d_ptr;
    auto outputDevice = OutputDeviceV1Global::handle(wlOutputDevice);

    bool modeValid = false;
    for (const auto& m : outputDevice->modes()) {
        if (m.id == mode_id) {
            modeValid = true;
            break;
        }
    }

    if (!modeValid) {
        qCWarning(WRAPLAND_SERVER) << "Set invalid mode id:" << mode_id;
        return;
    }

    priv->pendingChanges(outputDevice)->d_ptr->modeId = mode_id;
}

void OutputConfigurationV1::Private::transformCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       wl_resource* wlOutputDevice,
                                                       int32_t wlTransform)
{
    auto priv = handle(wlResource)->d_ptr;
    auto outputDevice = OutputDeviceV1Global::handle(wlOutputDevice);

    auto toTransform = [](int32_t wlTransform) {
        switch (wlTransform) {
        case WL_OUTPUT_TRANSFORM_90:
            return OutputDeviceV1::Transform::Rotated90;
        case WL_OUTPUT_TRANSFORM_180:
            return OutputDeviceV1::Transform::Rotated180;
        case WL_OUTPUT_TRANSFORM_270:
            return OutputDeviceV1::Transform::Rotated270;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return OutputDeviceV1::Transform::Flipped;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return OutputDeviceV1::Transform::Flipped90;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return OutputDeviceV1::Transform::Flipped180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return OutputDeviceV1::Transform::Flipped270;
        case WL_OUTPUT_TRANSFORM_NORMAL:
        default:
            return OutputDeviceV1::Transform::Normal;
        }
    };

    priv->pendingChanges(outputDevice)->d_ptr->transform = toTransform(wlTransform);
}

void OutputConfigurationV1::Private::geometryCallback([[maybe_unused]] wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      wl_resource* wlOutputDevice,
                                                      wl_fixed_t x,
                                                      wl_fixed_t y,
                                                      wl_fixed_t width,
                                                      wl_fixed_t height)
{
    auto priv = handle(wlResource)->d_ptr;
    auto outputDevice = OutputDeviceV1Global::handle(wlOutputDevice);

    const QRectF geo(wl_fixed_to_double(x),
                     wl_fixed_to_double(y),
                     wl_fixed_to_double(width),
                     wl_fixed_to_double(height));

    priv->pendingChanges(outputDevice)->d_ptr->geometry = geo;
}

void OutputConfigurationV1::Private::applyCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;

    if (!priv->manager) {
        priv->sendFailed();
        return;
    }

    Q_EMIT priv->manager->configurationChangeRequested(priv->handle());
}

QHash<OutputDeviceV1*, OutputChangesetV1*> OutputConfigurationV1::changes() const
{
    return d_ptr->changes;
}

void OutputConfigurationV1::setApplied()
{
    d_ptr->clearPendingChanges();
    d_ptr->sendApplied();
}

void OutputConfigurationV1::Private::sendApplied()
{
    send<zkwinft_output_configuration_v1_send_applied>();
}

void OutputConfigurationV1::setFailed()
{
    d_ptr->clearPendingChanges();
    d_ptr->sendFailed();
}

void OutputConfigurationV1::Private::sendFailed()
{
    send<zkwinft_output_configuration_v1_send_failed>();
}

OutputChangesetV1* OutputConfigurationV1::Private::pendingChanges(OutputDeviceV1* outputdevice)
{
    if (!changes.keys().contains(outputdevice)) {
        changes[outputdevice] = new OutputChangesetV1(outputdevice, handle());
    }
    return changes[outputdevice];
}

bool OutputConfigurationV1::Private::hasPendingChanges(OutputDeviceV1* outputdevice) const
{
    if (!changes.keys().contains(outputdevice)) {
        return false;
    }

    auto change = changes[outputdevice];
    return change->enabledChanged() || change->modeChanged() || change->transformChanged()
        || change->geometryChanged();
}

void OutputConfigurationV1::Private::clearPendingChanges()
{
    qDeleteAll(changes.begin(), changes.end());
    changes.clear();
}

OutputConfigurationV1::OutputConfigurationV1(Client* client,
                                             uint32_t version,
                                             uint32_t id,
                                             OutputManagementV1* manager)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, manager, this))
{
}

OutputConfigurationV1::~OutputConfigurationV1()
{
    d_ptr->clearPendingChanges();
}

}
