/********************************************************************
Copyright © 2015  Sebastian Kügler <sebas@kde.org>
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
#include "output_configuration_v1.h"

#include "event_queue.h"
#include "output_device_v1.h"
#include "output_management_v1.h"
#include "wayland_pointer_p.h"

#include "wayland-output-management-v1-client-protocol.h"
#include "wayland-output-device-v1-client-protocol.h"

#include <QRectF>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN OutputConfigurationV1::Private
{
public:
    Private() = default;

    void setup(zkwinft_output_configuration_v1 *outputConfiguration);

    WaylandPointer<zkwinft_output_configuration_v1,
                        zkwinft_output_configuration_v1_destroy> outputConfiguration;
    static struct zkwinft_output_configuration_v1_listener s_outputConfigurationListener;
    EventQueue *queue = nullptr;

    OutputConfigurationV1 *q;

private:
    static void appliedCallback(void *data, zkwinft_output_configuration_v1 *config);
    static void failedCallback(void *data, zkwinft_output_configuration_v1 *config);
};

OutputConfigurationV1::OutputConfigurationV1(QObject *parent)
: QObject(parent)
, d(new Private)
{
    d->q = this;
}

OutputConfigurationV1::~OutputConfigurationV1()
{
    release();
}

void OutputConfigurationV1::setup(zkwinft_output_configuration_v1 *outputConfiguration)
{
    Q_ASSERT(outputConfiguration);
    Q_ASSERT(!d->outputConfiguration);
    d->outputConfiguration.setup(outputConfiguration);
    d->setup(outputConfiguration);
}

void OutputConfigurationV1::Private::setup(zkwinft_output_configuration_v1* outputConfiguration)
{
    zkwinft_output_configuration_v1_add_listener(outputConfiguration,
                                                 &s_outputConfigurationListener, this);
}


void OutputConfigurationV1::release()
{
    d->outputConfiguration.release();
}

void OutputConfigurationV1::destroy()
{
    d->outputConfiguration.destroy();
}

void OutputConfigurationV1::setEventQueue(EventQueue *queue)
{
    d->queue = queue;
}

EventQueue *OutputConfigurationV1::eventQueue()
{
    return d->queue;
}

OutputConfigurationV1::operator zkwinft_output_configuration_v1*() {
    return d->outputConfiguration;
}

OutputConfigurationV1::operator zkwinft_output_configuration_v1*() const {
    return d->outputConfiguration;
}

bool OutputConfigurationV1::isValid() const
{
    return d->outputConfiguration.isValid();
}

// Requests

void OutputConfigurationV1::setEnabled(OutputDeviceV1 *outputDevice,
                                       OutputDeviceV1::Enablement enable)
{
    qint32 _enable = ZKWINFT_OUTPUT_DEVICE_V1_ENABLEMENT_DISABLED;
    if (enable == OutputDeviceV1::Enablement::Enabled) {
        _enable = ZKWINFT_OUTPUT_DEVICE_V1_ENABLEMENT_ENABLED;
    }
    zkwinft_output_device_v1 *od = outputDevice->output();
    zkwinft_output_configuration_v1_enable(d->outputConfiguration, od, _enable);
}

void OutputConfigurationV1::setMode(OutputDeviceV1 *outputDevice, const int modeId)
{
    zkwinft_output_device_v1 *od = outputDevice->output();
    zkwinft_output_configuration_v1_mode(d->outputConfiguration, od, modeId);
}

void OutputConfigurationV1::setTransform(OutputDeviceV1 *outputDevice,
                                         OutputDeviceV1::Transform transform)
{
    auto toTransform = [transform]() {
        switch (transform) {
            case OutputDeviceV1::Transform::Normal:
                return WL_OUTPUT_TRANSFORM_NORMAL;
            case OutputDeviceV1::Transform::Rotated90:
                return WL_OUTPUT_TRANSFORM_90;
            case OutputDeviceV1::Transform::Rotated180:
                return WL_OUTPUT_TRANSFORM_180;
            case OutputDeviceV1::Transform::Rotated270:
                return WL_OUTPUT_TRANSFORM_270;
            case OutputDeviceV1::Transform::Flipped:
                return WL_OUTPUT_TRANSFORM_FLIPPED;
            case OutputDeviceV1::Transform::Flipped90:
                return WL_OUTPUT_TRANSFORM_FLIPPED_90;
            case OutputDeviceV1::Transform::Flipped180:
                return WL_OUTPUT_TRANSFORM_FLIPPED_180;
            case OutputDeviceV1::Transform::Flipped270:
                return WL_OUTPUT_TRANSFORM_FLIPPED_270;
        }
        abort();
    };
    zkwinft_output_device_v1 *od = outputDevice->output();
    zkwinft_output_configuration_v1_transform(d->outputConfiguration, od, toTransform());
}

void OutputConfigurationV1::setGeometry(OutputDeviceV1 *outputDevice, const QRectF &geo)
{
    zkwinft_output_device_v1 *od = outputDevice->output();
    zkwinft_output_configuration_v1_geometry(d->outputConfiguration, od,
                                             wl_fixed_from_double(geo.x()),
                                             wl_fixed_from_double(geo.y()),
                                             wl_fixed_from_double(geo.width()),
                                             wl_fixed_from_double(geo.height()));
}

void OutputConfigurationV1::apply()
{
    zkwinft_output_configuration_v1_apply(d->outputConfiguration);
}

// Callbacks
zkwinft_output_configuration_v1_listener OutputConfigurationV1::Private::
                                                                s_outputConfigurationListener = {
    appliedCallback,
    failedCallback
};

void OutputConfigurationV1::Private::appliedCallback(void* data,
                                                     zkwinft_output_configuration_v1 *config)
{
    Q_UNUSED(config);
    auto o = reinterpret_cast<OutputConfigurationV1::Private*>(data);
    emit o->q->applied();
}

void OutputConfigurationV1::Private::failedCallback(void* data,
                                                    zkwinft_output_configuration_v1 *config)
{
    Q_UNUSED(config);
    auto o = reinterpret_cast<OutputConfigurationV1::Private*>(data);
    emit o->q->failed();
}

}
}

