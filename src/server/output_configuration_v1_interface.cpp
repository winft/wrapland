/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2015  Sebastian Kügler <sebas@kde.org>

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
#include "output_configuration_v1_interface.h"

#include "display.h"
#include "logging.h"
#include "output_changeset_v1_p.h"
#include "output_device_v1_interface.h"
#include "resource_p.h"

#include <wayland-server.h>
#include "wayland-output-management-v1-server-protocol.h"
#include "wayland-output-device-v1-server-protocol.h"

#include <QSize>

namespace Wrapland
{
namespace Server
{

class OutputConfigurationV1Interface::Private : public Resource::Private
{
public:
    Private(OutputConfigurationV1Interface *q, OutputManagementV1Interface *c,
            wl_resource *parentResource);
    ~Private() override;

    void sendApplied();
    void sendFailed();
    void emitConfigurationChangeRequested() const;
    void clearPendingChanges();

    bool hasPendingChanges(OutputDeviceV1Interface *outputdevice) const;
    OutputChangesetV1* pendingChanges(OutputDeviceV1Interface *outputdevice);

    OutputManagementV1Interface *outputManagement;
    QHash<OutputDeviceV1Interface*, OutputChangesetV1*> changes;

    static const quint32 s_version = 1;

private:
    static void enableCallback(wl_client *client, wl_resource *resource,
                               wl_resource * outputdevice, int32_t enable);
    static void modeCallback(wl_client *client, wl_resource *resource,
                             wl_resource * outputdevice, int32_t mode_id);
    static void transformCallback(wl_client *client, wl_resource *resource,
                                  wl_resource * outputdevice, int32_t transform);
    static void geometryCallback(wl_client *client, wl_resource *resource,
                                 wl_resource * outputdevice, wl_fixed_t x, wl_fixed_t y,
                                 wl_fixed_t width, wl_fixed_t height);
    static void applyCallback(wl_client *client, wl_resource *resource);

    OutputConfigurationV1Interface *q_func() {
        return reinterpret_cast<OutputConfigurationV1Interface *>(q);
    }

    static const struct zkwinft_output_configuration_v1_interface s_interface;
};

const struct zkwinft_output_configuration_v1_interface OutputConfigurationV1Interface::Private::
                                                                                    s_interface = {
    enableCallback,
    modeCallback,
    transformCallback,
    geometryCallback,
    applyCallback
};

OutputConfigurationV1Interface::OutputConfigurationV1Interface(OutputManagementV1Interface* parent,
                                                               wl_resource* parentResource)
    : Resource(new Private(this, parent, parentResource))
{
    Q_D();
    d->outputManagement = parent;
}

OutputConfigurationV1Interface::~OutputConfigurationV1Interface()
{
    Q_D();
    d->clearPendingChanges();
}

void OutputConfigurationV1Interface::Private::enableCallback(wl_client *client,
                                                             wl_resource *resource,
                                                             wl_resource * outputdevice,
                                                             int32_t enable)
{
    Q_UNUSED(client);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    auto _enable = (enable == ZKWINFT_OUTPUT_DEVICE_V1_ENABLEMENT_ENABLED) ?
                                    OutputDeviceV1Interface::Enablement::Enabled :
                                    OutputDeviceV1Interface::Enablement::Disabled;
    OutputDeviceV1Interface *o = OutputDeviceV1Interface::get(outputdevice);
    s->pendingChanges(o)->d_func()->enabled = _enable;
}

void OutputConfigurationV1Interface::Private::modeCallback(wl_client *client, wl_resource *resource,
                                                           wl_resource * outputdevice,
                                                           int32_t mode_id)
{
    Q_UNUSED(client);
    OutputDeviceV1Interface *o = OutputDeviceV1Interface::get(outputdevice);

    bool modeValid = false;
    for (const auto &m: o->modes()) {
        if (m.id == mode_id) {
            modeValid = true;
            break;
        }
    }
    if (!modeValid) {
        qCWarning(WRAPLAND_SERVER) << "Set invalid mode id:" << mode_id;
        return;
    }
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->pendingChanges(o)->d_func()->modeId = mode_id;
}

void OutputConfigurationV1Interface::Private::transformCallback(wl_client *client,
                                                                wl_resource *resource,
                                                                wl_resource * outputdevice,
                                                                int32_t transform)
{
    Q_UNUSED(client);
    auto toTransform = [transform]() {
        switch (transform) {
            case WL_OUTPUT_TRANSFORM_90:
                return OutputDeviceV1Interface::Transform::Rotated90;
            case WL_OUTPUT_TRANSFORM_180:
                return OutputDeviceV1Interface::Transform::Rotated180;
            case WL_OUTPUT_TRANSFORM_270:
                return OutputDeviceV1Interface::Transform::Rotated270;
            case WL_OUTPUT_TRANSFORM_FLIPPED:
                return OutputDeviceV1Interface::Transform::Flipped;
            case WL_OUTPUT_TRANSFORM_FLIPPED_90:
                return OutputDeviceV1Interface::Transform::Flipped90;
            case WL_OUTPUT_TRANSFORM_FLIPPED_180:
                return OutputDeviceV1Interface::Transform::Flipped180;
            case WL_OUTPUT_TRANSFORM_FLIPPED_270:
                return OutputDeviceV1Interface::Transform::Flipped270;
            case WL_OUTPUT_TRANSFORM_NORMAL:
            default:
                return OutputDeviceV1Interface::Transform::Normal;
        }
    };
    auto _transform = toTransform();
    OutputDeviceV1Interface *o = OutputDeviceV1Interface::get(outputdevice);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->pendingChanges(o)->d_func()->transform = _transform;
}

void OutputConfigurationV1Interface::Private::geometryCallback(wl_client *client,
                                                               wl_resource *resource,
                                                               wl_resource * outputdevice,
                                                               wl_fixed_t x, wl_fixed_t y,
                                                               wl_fixed_t width, wl_fixed_t height)
{
    Q_UNUSED(client);
    const QRectF geo(wl_fixed_to_double(x),
                     wl_fixed_to_double(y),
                     wl_fixed_to_double(width),
                     wl_fixed_to_double(height));
    OutputDeviceV1Interface *o = OutputDeviceV1Interface::get(outputdevice);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->pendingChanges(o)->d_func()->geometry = geo;
}

void OutputConfigurationV1Interface::Private::applyCallback(wl_client *client,
                                                            wl_resource *resource)
{
    Q_UNUSED(client);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->emitConfigurationChangeRequested();
}

void OutputConfigurationV1Interface::Private::emitConfigurationChangeRequested() const
{
    auto configinterface = reinterpret_cast<OutputConfigurationV1Interface *>(q);
    emit outputManagement->configurationChangeRequested(configinterface);
}


OutputConfigurationV1Interface::Private::Private(OutputConfigurationV1Interface *q,
                                                 OutputManagementV1Interface *c,
                                                 wl_resource *parentResource)
: Resource::Private(q, c, parentResource, &zkwinft_output_configuration_v1_interface, &s_interface)
{
}

OutputConfigurationV1Interface::Private::~Private() = default;

OutputConfigurationV1Interface::Private *OutputConfigurationV1Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

QHash<OutputDeviceV1Interface*, OutputChangesetV1*> OutputConfigurationV1Interface::changes() const
{
    Q_D();
    return d->changes;
}

void OutputConfigurationV1Interface::setApplied()
{
    Q_D();
    d->clearPendingChanges();
    d->sendApplied();
}

void OutputConfigurationV1Interface::Private::sendApplied()
{
    if (!resource) {
        return;
    }
    zkwinft_output_configuration_v1_send_applied(resource);
}

void OutputConfigurationV1Interface::setFailed()
{
    Q_D();
    d->clearPendingChanges();
    d->sendFailed();
}

void OutputConfigurationV1Interface::Private::sendFailed()
{
    if (!resource) {
        return;
    }
    zkwinft_output_configuration_v1_send_failed(resource);
}

OutputChangesetV1* OutputConfigurationV1Interface::Private::pendingChanges(
                                                            OutputDeviceV1Interface *outputdevice)
{
    if (!changes.keys().contains(outputdevice)) {
        changes[outputdevice] = new OutputChangesetV1(outputdevice, q);
    }
    return changes[outputdevice];
}

bool OutputConfigurationV1Interface::Private::hasPendingChanges(
                                                        OutputDeviceV1Interface *outputdevice) const
{
    if (!changes.keys().contains(outputdevice)) {
        return false;
    }
    auto c = changes[outputdevice];
    return c->enabledChanged() ||
    c->modeChanged() ||
    c->transformChanged() ||
    c->geometryChanged();
}

void OutputConfigurationV1Interface::Private::clearPendingChanges()
{
    qDeleteAll(changes.begin(), changes.end());
    changes.clear();
}

}
}
