/********************************************************************
Copyright © 2013        Martin Gräßlin <mgraesslin@kde.org>
Copyright © 2018-2020   Roman Gilg <subdiff@gmail.com>

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
#include "output_device_v1.h"

#include "logging.h"
#include "wayland_pointer_p.h"

#include <QRectF>
#include <QSize>

#include "wayland-output-device-v1-client-protocol.h"
#include <wayland-client-protocol.h>

namespace Wrapland
{

namespace Client
{

typedef QList<OutputDeviceV1::Mode> Modes;

class Q_DECL_HIDDEN OutputDeviceV1::Private
{
public:
    Private(OutputDeviceV1 *q);
    void setup(zkwinft_output_device_v1 *o);

    WaylandPointer<zkwinft_output_device_v1,
                        zkwinft_output_device_v1_destroy> output;
    EventQueue *queue = nullptr;
    QSize physicalSize;
    QRectF geometry;
    QString manufacturer;
    QString model;
    QString serialNumber;
    QString eisaId;
    Transform transform = Transform::Normal;
    Modes modes;
    Modes::iterator currentMode = modes.end();

    QByteArray edid;
    OutputDeviceV1::Enablement enabled = OutputDeviceV1::Enablement::Enabled;
    QByteArray uuid;

    bool done = false;

private:
    static void infoCallback(void *data, zkwinft_output_device_v1 *output,
                             const char *uuid, const char *eisa_id, const char *serial_number,
                             const char *edid, const char *make, const char *model,
                             int32_t physical_width, int32_t physical_height);

    static void enabledCallback(void *data, zkwinft_output_device_v1 *output,
                                int32_t enabled);

    static void modeCallback(void *data, zkwinft_output_device_v1 *output, uint32_t flags,
                             int32_t width, int32_t height, int32_t refresh, int32_t mode_id);

    static void transformCallback(void *data, zkwinft_output_device_v1 *output,
                                  int32_t transform);

    static void geometryCallback(void *data, zkwinft_output_device_v1 *output,
                                 wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height);

    static void doneCallback(void *data, zkwinft_output_device_v1 *output);

    void addMode(uint32_t flags, int32_t width, int32_t height, int32_t refresh, int32_t mode_id);

    OutputDeviceV1 *q;
    static struct zkwinft_output_device_v1_listener s_outputListener;
};

OutputDeviceV1::Private::Private(OutputDeviceV1 *q)
    : q(q)
{
}

void OutputDeviceV1::Private::setup(zkwinft_output_device_v1 *o)
{
    Q_ASSERT(o);
    Q_ASSERT(!output);
    output.setup(o);
    zkwinft_output_device_v1_add_listener(output, &s_outputListener, this);
}

bool OutputDeviceV1::Mode::operator==(const OutputDeviceV1::Mode &m) const
{
    return size == m.size
           && refreshRate == m.refreshRate
           && flags == m.flags
           && output == m.output;
}

zkwinft_output_device_v1_listener OutputDeviceV1::Private::s_outputListener = {
    infoCallback,
    enabledCallback,
    modeCallback,
    transformCallback,
    geometryCallback,
    doneCallback,
};

void OutputDeviceV1::Private::infoCallback(void *data, zkwinft_output_device_v1 *output,
                                           const char *uuid, const char *eisa_id,
                                           const char *serial_number, const char *edid,
                                           const char *make, const char *model,
                                           int32_t physical_width, int32_t physical_height)
{
    auto out = reinterpret_cast<OutputDeviceV1::Private*>(data);
    Q_ASSERT(out->output == output);

    out->uuid = uuid;
    out->eisaId = eisa_id;
    out->serialNumber = serial_number;
    out->edid = edid;
    out->manufacturer = make;
    out->model = model;
    out->physicalSize = QSize(physical_width, physical_height);
}

void OutputDeviceV1::Private::enabledCallback(void* data, zkwinft_output_device_v1* output,
                                              int32_t enabled)
{
    auto out = reinterpret_cast<OutputDeviceV1::Private*>(data);
    Q_ASSERT(out->output == output);

    OutputDeviceV1::Enablement _enabled = OutputDeviceV1::Enablement::Disabled;
    if (enabled == ZKWINFT_OUTPUT_DEVICE_V1_ENABLEMENT_ENABLED) {
        _enabled = OutputDeviceV1::Enablement::Enabled;
    }
    if (out->enabled != _enabled) {
        out->enabled = _enabled;
        emit out->q->enabledChanged(out->enabled);
        if (out->done) {
            emit out->q->changed();
        }
    }
}

void OutputDeviceV1::Private::modeCallback(void *data, zkwinft_output_device_v1 *output,
                                           uint32_t flags, int32_t width, int32_t height,
                                           int32_t refresh, int32_t mode_id)
{
    auto out = reinterpret_cast<OutputDeviceV1::Private*>(data);
    Q_ASSERT(out->output == output);
    out->addMode(flags, width, height, refresh, mode_id);
}

void OutputDeviceV1::Private::addMode(uint32_t flags, int32_t width, int32_t height,
                                      int32_t refresh, int32_t mode_id)
{
    Mode mode;
    mode.output = QPointer<OutputDeviceV1>(q);
    mode.refreshRate = refresh;
    mode.size = QSize(width, height);
    mode.id = mode_id;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        mode.flags |= Mode::Flag::Current;
    }
    if (flags & WL_OUTPUT_MODE_PREFERRED) {
        mode.flags |= Mode::Flag::Preferred;
    }
    auto currentIt = modes.insert(modes.end(), mode);
    bool existing = false;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        auto it = modes.begin();
        while (it != currentIt) {
            auto &m = (*it);
            if (m.flags.testFlag(Mode::Flag::Current)) {
                m.flags &= ~Mode::Flags(Mode::Flag::Current);
                emit q->modeChanged(m);
            }
            if (m.refreshRate == mode.refreshRate && m.size == mode.size) {
                it = modes.erase(it);
                existing = true;
            } else {
                it++;
            }
        }
        currentMode = currentIt;
    }
    if (existing) {
        emit q->modeChanged(mode);
    } else {
        emit q->modeAdded(mode);
    }
}

Wrapland::Client::OutputDeviceV1::Mode OutputDeviceV1::currentMode() const
{
    for (const auto &mode: modes()) {
        if (mode.flags.testFlag(Wrapland::Client::OutputDeviceV1::Mode::Flag::Current)) {
            return mode;
        }
    }
    qCWarning(WRAPLAND_CLIENT) << "current mode not found";
    return Mode();
}

void OutputDeviceV1::Private::geometryCallback(void *data, zkwinft_output_device_v1 *output,
                                               wl_fixed_t x, wl_fixed_t y,
                                               wl_fixed_t width, wl_fixed_t height)
{
    auto out = reinterpret_cast<OutputDeviceV1::Private*>(data);
    Q_ASSERT(out->output == output);

    out->geometry = QRectF(wl_fixed_to_double(x), wl_fixed_to_double(y),
                           wl_fixed_to_double(width), wl_fixed_to_double(height));
}

void OutputDeviceV1::Private::transformCallback(void *data,
                                                zkwinft_output_device_v1 *output,
                                                int32_t transform)
{
//    Q_UNUSED(transform)
    auto out = reinterpret_cast<OutputDeviceV1::Private*>(data);
    Q_ASSERT(out->output == output);

    auto toTransform = [transform]() {
        switch (transform) {
        case WL_OUTPUT_TRANSFORM_90:
            return Transform::Rotated90;
        case WL_OUTPUT_TRANSFORM_180:
            return Transform::Rotated180;
        case WL_OUTPUT_TRANSFORM_270:
            return Transform::Rotated270;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return Transform::Flipped;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return Transform::Flipped90;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return Transform::Flipped180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return Transform::Flipped270;
        case WL_OUTPUT_TRANSFORM_NORMAL:
        default:
            return Transform::Normal;
        }
    };
    out->transform = toTransform();
}

void OutputDeviceV1::Private::doneCallback(void *data, zkwinft_output_device_v1 *output)
{
    auto out = reinterpret_cast<OutputDeviceV1::Private*>(data);
    Q_ASSERT(out->output == output);
    out->done = true;
    emit out->q->changed();
    emit out->q->done();
}

void OutputDeviceV1::setup(zkwinft_output_device_v1 *output)
{
    d->setup(output);
}

EventQueue *OutputDeviceV1::eventQueue() const
{
    return d->queue;
}

void OutputDeviceV1::setEventQueue(EventQueue *queue)
{
    d->queue = queue;
}

OutputDeviceV1::OutputDeviceV1(QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
}

OutputDeviceV1::~OutputDeviceV1()
{
    d->output.release();
}

QRectF OutputDeviceV1::geometry() const
{
    return d->geometry;
}

QString OutputDeviceV1::manufacturer() const
{
    return d->manufacturer;
}

QString OutputDeviceV1::model() const
{
    return d->model;
}

QString OutputDeviceV1::serialNumber() const
{
    return d->serialNumber;
}

QString OutputDeviceV1::eisaId() const
{
    return d->eisaId;
}

zkwinft_output_device_v1* OutputDeviceV1::output()
{
    return d->output;
}

QSize OutputDeviceV1::physicalSize() const
{
    return d->physicalSize;
}

QSize OutputDeviceV1::pixelSize() const
{
    if (d->currentMode == d->modes.end()) {
        return QSize();
    }
    return (*d->currentMode).size;
}

int OutputDeviceV1::refreshRate() const
{
    if (d->currentMode == d->modes.end()) {
        return 0;
    }
    return (*d->currentMode).refreshRate;
}

bool OutputDeviceV1::isValid() const
{
    return d->output.isValid();
}

OutputDeviceV1::Transform OutputDeviceV1::transform() const
{
    return d->transform;
}

QList< OutputDeviceV1::Mode > OutputDeviceV1::modes() const
{
    return d->modes;
}

OutputDeviceV1::operator zkwinft_output_device_v1* () {
    return d->output;
}

OutputDeviceV1::operator zkwinft_output_device_v1* () const {
    return d->output;
}

QByteArray OutputDeviceV1::edid() const
{
    return d->edid;
}

OutputDeviceV1::Enablement OutputDeviceV1::enabled() const
{
    return d->enabled;
}

QByteArray OutputDeviceV1::uuid() const
{
    return d->uuid;
}

void OutputDeviceV1::destroy()
{
    d->output.destroy();
}

}
}
