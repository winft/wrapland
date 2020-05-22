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
#include "output_device_v1_p.h"

#include "display.h"
#include "logging.h"

#include "wayland-output-device-v1-server-protocol.h"

namespace Wrapland::Server
{

OutputDeviceV1::Private::Private(OutputDeviceV1* q, Display* display)
    : OutputDeviceV1Global(q, display, &zkwinft_output_device_v1_interface, nullptr)
    , displayHandle{display}
{
    create();
}

QSize OutputDeviceV1::modeSize() const
{
    if (d_ptr->pending.mode.id == -1) {
        return QSize();
    }
    return d_ptr->pending.mode.size;
}

int OutputDeviceV1::refreshRate() const
{
    if (d_ptr->pending.mode.id == -1) {
        return Mode::defaultRefreshRate;
    }
    return d_ptr->pending.mode.refreshRate;
}

void OutputDeviceV1::Private::addMode(Mode& mode)
{
    bool currentChanged = false;
    bool preferredChanged = false;

    auto removeFlag = [this](Mode& mode, ModeFlag flag) {
        mode.flags &= ~uint(flag);
        if (published.mode.id == mode.id) {
            published.mode.flags = mode.flags;
            published.mode.flags = mode.flags;
        }
    };

    if (mode.flags.testFlag(ModeFlag::Current)) {
        Q_ASSERT(published.mode.id < 0);
        if (published.mode.id >= 0 && published.mode.id != mode.id) {
            // TODO(romangg): Make the clang-tidy check pass here and below too.
            // NOLINTNEXTLINE(clang-diagnostic-gnu-zero-variadic-macro-arguments)
            qCWarning(WRAPLAND_SERVER)
                << "Duplicate current Mode id" << published.mode.id << "and" << mode.id << mode.size
                << mode.refreshRate << ": setting current mode to:" << mode.size
                << mode.refreshRate;
        }
        mode.flags &= ~uint(ModeFlag::Current);
        published.mode = mode;
        pending.mode = mode;
        currentChanged = true;
    }

    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        // Check that there is not yet a Preferred mode.
        auto preferredIt = std::find_if(modes.begin(), modes.end(), [](const Mode& mode) {
            return mode.flags.testFlag(ModeFlag::Preferred);
        });
        if (preferredIt != modes.end()) {
            // NOLINTNEXTLINE(clang-diagnostic-gnu-zero-variadic-macro-arguments)
            qCWarning(WRAPLAND_SERVER)
                << "Duplicate preferred Mode id" << (*preferredIt).id << "and" << mode.id
                << mode.size << mode.refreshRate
                << ": removing flag from previous mode:" << (*preferredIt).size
                << (*preferredIt).refreshRate;
            removeFlag(*preferredIt, ModeFlag::Preferred);
        }
        preferredChanged = true;
    }

    auto existingModeIt = std::find_if(modes.begin(), modes.end(), [mode](const Mode& mode_it) {
        return mode.size == mode_it.size && mode.refreshRate == mode_it.refreshRate
            && mode.id == mode_it.id;
    });
    auto emitCurrentChange = [this, &mode, currentChanged] {
        if (currentChanged) {
            Q_EMIT handle()->refreshRateChanged(mode.refreshRate);
            Q_EMIT handle()->modeSizeChanged(mode.size);
            Q_EMIT handle()->currentModeChanged();
        }
    };

    if (existingModeIt != modes.end()) {
        // Mode already exists in modes list. Just update its flags and emit signals if necessary.
        (*existingModeIt).flags = mode.flags;
        emitCurrentChange();
        if (currentChanged || preferredChanged) {
            Q_EMIT handle()->modesChanged();
        }
        return;
    }

    auto idIt = std::find_if(modes.constBegin(), modes.constEnd(), [mode](const Mode& mode_it) {
        return mode.id == mode_it.id;
    });
    if (idIt != modes.constEnd()) {
        // NOLINTNEXTLINE(clang-diagnostic-gnu-zero-variadic-macro-arguments)
        qCWarning(WRAPLAND_SERVER) << "Duplicate Mode id" << mode.id << ": not adding mode"
                                   << mode.size << mode.refreshRate;
        return;
    }

    modes << mode;
    emitCurrentChange();
    Q_EMIT handle()->modesChanged();
}

void OutputDeviceV1::addMode(Mode& mode)
{
    Q_ASSERT(mode.id >= 0);
    Q_ASSERT(mode.size.isValid());

    d_ptr->addMode(mode);
}

void OutputDeviceV1::Private::setMode(int id)
{
    auto existingModeIt = std::find_if(
        modes.begin(), modes.end(), [id](const Mode& mode) { return mode.id == id; });

    Q_ASSERT(existingModeIt != modes.end());
    pending.mode = *existingModeIt;

    Q_EMIT handle()->modesChanged();
    Q_EMIT handle()->refreshRateChanged(pending.mode.refreshRate);
    Q_EMIT handle()->modeSizeChanged(pending.mode.size);
    Q_EMIT handle()->currentModeChanged();
}

void OutputDeviceV1::setMode(int id)
{
    d_ptr->setMode(id);
}

void OutputDeviceV1::Private::bindInit(OutputDeviceV1Bind* bind)
{
    sendInfo(bind);

    auto currentModeIt = modes.constEnd();
    for (auto it = modes.constBegin(); it != modes.constEnd(); ++it) {
        const Mode& mode = *it;
        if (mode.id == published.mode.id) {
            // Needs to be sent as last mode.
            currentModeIt = it;
            continue;
        }
        sendMode(bind, mode);
    }

    if (currentModeIt != modes.constEnd()) {
        sendMode(bind, *currentModeIt);
    }

    sendEnabled(bind);
    sendGeometry(bind);
    sendTransform(bind);

    sendDone(bind);
    bind->client()->flush();
}

void OutputDeviceV1::Private::sendInfo(OutputDeviceV1Bind* bind)
{
    const auto& info = published.info;
    send<zkwinft_output_device_v1_send_info>(bind,
                                             info.uuid.constData(),
                                             qPrintable(info.eisaId),
                                             qPrintable(info.serialNumber),
                                             info.edid.toBase64().constData(),
                                             qPrintable(info.manufacturer),
                                             qPrintable(info.model),
                                             info.physicalSize.width(),
                                             info.physicalSize.height());
}

void OutputDeviceV1::Private::sendEnabled(OutputDeviceV1Bind* bind)
{
    const int enabled = published.enabled == OutputDeviceV1::Enablement::Enabled;
    send<zkwinft_output_device_v1_send_enabled>(bind, enabled);
}

void OutputDeviceV1::Private::sendMode(OutputDeviceV1Bind* bind, const Mode& mode)
{
    int32_t flags = 0;
    if (mode.id == published.mode.id) {
        flags |= WL_OUTPUT_MODE_CURRENT;
    }
    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        flags |= WL_OUTPUT_MODE_PREFERRED;
    }
    send<zkwinft_output_device_v1_send_mode>(
        bind, flags, mode.size.width(), mode.size.height(), mode.refreshRate, mode.id);
}

void OutputDeviceV1::Private::sendGeometry(OutputDeviceV1Bind* bind)
{
    const QRectF& geo = published.geometry;
    send<zkwinft_output_device_v1_send_geometry>(bind,
                                                 wl_fixed_from_double(geo.x()),
                                                 wl_fixed_from_double(geo.y()),
                                                 wl_fixed_from_double(geo.width()),
                                                 wl_fixed_from_double(geo.height()));
}

int32_t toTransform(OutputDeviceV1::Transform transform)
{
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
}

void OutputDeviceV1::Private::sendTransform(OutputDeviceV1Bind* bind)
{
    send<zkwinft_output_device_v1_send_transform>(bind, toTransform(published.transform));
}

void OutputDeviceV1::Private::sendDone(OutputDeviceV1Bind* bind)
{
    send<zkwinft_output_device_v1_send_done>(bind);
}

#define SETTER(setterName, type, argumentName, variableName)                                       \
    void OutputDeviceV1::setterName(type arg)                                                      \
    {                                                                                              \
        if (d_ptr->pending.argumentName == arg) {                                                  \
            return;                                                                                \
        }                                                                                          \
        d_ptr->pending.argumentName = arg;                                                         \
        Q_EMIT variableName##Changed(d_ptr->pending.argumentName);                                 \
    }

#define SETTER2(setterName, type, argumentName) SETTER(setterName, type, argumentName, argumentName)

SETTER(setSerialNumber, const QString&, info.serialNumber, serialNumber) // NOLINT
SETTER(setEisaId, const QString&, info.eisaId, eisaId)                   // NOLINT
SETTER(setUuid, const QByteArray&, info.uuid, uuid)                      // NOLINT
SETTER(setEdid, const QByteArray&, info.edid, edid)                      // NOLINT
SETTER(setManufacturer, const QString&, info.manufacturer, manufacturer) // NOLINT
SETTER(setModel, const QString&, info.model, model)                      // NOLINT
SETTER(setPhysicalSize, const QSize&, info.physicalSize, physicalSize)   // NOLINT
SETTER2(setEnabled, Enablement, enabled)                                 // NOLINT
SETTER2(setTransform, Transform, transform)                              // NOLINT
SETTER2(setGeometry, const QRectF&, geometry)                            // NOLINT

#undef SETTER
#undef SETTER2

QSize OutputDeviceV1::physicalSize() const
{
    return d_ptr->pending.info.physicalSize;
}

QRectF OutputDeviceV1::geometry() const
{
    return d_ptr->pending.geometry;
}

QString OutputDeviceV1::serialNumber() const
{
    return d_ptr->pending.info.serialNumber;
}

QString OutputDeviceV1::eisaId() const
{
    return d_ptr->pending.info.eisaId;
}

QString OutputDeviceV1::manufacturer() const
{
    return d_ptr->pending.info.manufacturer;
}

QString OutputDeviceV1::model() const
{
    return d_ptr->pending.info.model;
}

QList<OutputDeviceV1::Mode> OutputDeviceV1::modes() const
{
    return d_ptr->modes;
}

int OutputDeviceV1::modeId() const
{
    for (const Mode& m : d_ptr->modes) {
        if (m.flags.testFlag(OutputDeviceV1::ModeFlag::Current)) {
            return m.id;
        }
    }
    return -1;
}

OutputDeviceV1::Transform OutputDeviceV1::transform() const
{
    return d_ptr->pending.transform;
}

QByteArray OutputDeviceV1::uuid() const
{
    return d_ptr->pending.info.uuid;
}

QByteArray OutputDeviceV1::edid() const
{
    return d_ptr->pending.info.edid;
}

OutputDeviceV1::Enablement OutputDeviceV1::enabled() const
{
    return d_ptr->pending.enabled;
}

bool OutputDeviceV1::Private::broadcastMode()
{
    if (pending.mode.id == published.mode.id) {
        return false;
    }
    published.mode = pending.mode;
    for (auto bind : getBinds()) {
        sendMode(bind, published.mode);
    }
    return true;
}

#define BROADCASTER(capitalName, lowercaseName)                                                    \
    bool OutputDeviceV1::Private::broadcast##capitalName()                                         \
    {                                                                                              \
        if (pending.lowercaseName == published.lowercaseName) {                                    \
            return false;                                                                          \
        }                                                                                          \
        published.lowercaseName = pending.lowercaseName;                                           \
        auto const binds = getBinds();                                                             \
        for (auto bind : binds) {                                                                  \
            send##capitalName(bind);                                                               \
        }                                                                                          \
        return true;                                                                               \
    }

BROADCASTER(Info, info)
BROADCASTER(Enabled, enabled)
BROADCASTER(Transform, transform)
BROADCASTER(Geometry, geometry)

#undef BROADCASTER

void OutputDeviceV1::Private::broadcast()
{
    bool changed = false;

    changed |= broadcastInfo();
    changed |= broadcastEnabled();
    changed |= broadcastMode();
    changed |= broadcastTransform();
    changed |= broadcastGeometry();

    if (!changed) {
        return;
    }

    for (auto bind : getBinds()) {
        sendDone(bind);
    }
}

OutputDeviceV1::OutputDeviceV1(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
}

OutputDeviceV1::~OutputDeviceV1()
{
    if (d_ptr->displayHandle) {
        d_ptr->displayHandle->removeOutputDevice(this);
    }
}

void OutputDeviceV1::done()
{
    d_ptr->broadcast();
}

}
