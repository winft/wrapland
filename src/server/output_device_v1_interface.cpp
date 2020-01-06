/********************************************************************
Copyright © 2014 Martin Gräßlin <mgraesslin@kde.org>
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
#include "output_device_v1_interface.h"

#include "display.h"
#include "global_p.h"
#include "logging.h"

#include <wayland-server.h>
#include "wayland-output-device-v1-server-protocol.h"

#include <QRectF>

namespace Wrapland
{
namespace Server
{

class OutputDeviceV1Interface::Private : public Global::Private
{
public:
    struct ResourceData {
        wl_resource *resource;
        uint32_t version;
    };
    Private(OutputDeviceV1Interface *q, Display *d);
    ~Private() override;
    void create() override;

    void addMode(Mode &mode);
    void setMode(int id);

    void sendInfo(const ResourceData &data);
    void sendEnabled(const ResourceData &data);

    void sendMode(const ResourceData &data, const Mode &mode);
    void sendTransform(const ResourceData &data);
    void sendGeometry(const ResourceData &data);

    void sendDone(const ResourceData &data);

    void broadcast();

    struct State {
        struct Info {
            bool operator==(struct Info &i) {
            return uuid == i.uuid && eisaId == i.eisaId && serialNumber == i.serialNumber
                && edid == i.edid && manufacturer == i.manufacturer && model == i.model
                && physicalSize == i.physicalSize;
            }
            QByteArray uuid;
            QString eisaId;
            QString serialNumber;
            QByteArray edid;
            QString manufacturer = QStringLiteral("kwinft");
            QString model = QStringLiteral("none");
            QSize physicalSize;
        } info;

        Enablement enabled = Enablement::Enabled;

        Mode mode;
        Transform transform = OutputDeviceV1Interface::Transform::Normal;
        QRectF geometry;
    } pending, published;

    QList<Mode> modes;

    QList<ResourceData> resources;

    static OutputDeviceV1Interface *get(wl_resource *native);

private:
    bool broadcastInfo();
    bool broadcastEnabled();
    bool broadcastMode();
    bool broadcastTransform();
    bool broadcastGeometry();

    static Private *cast(wl_resource *native);
    static void unbind(wl_resource *resource);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static const quint32 s_version;
    OutputDeviceV1Interface *q;
    static QVector<Private*> s_privates;
};

const quint32 OutputDeviceV1Interface::Private::s_version = 1;

QVector<OutputDeviceV1Interface::Private*> OutputDeviceV1Interface::Private::s_privates;

OutputDeviceV1Interface::Private::Private(OutputDeviceV1Interface *q, Display *d)
    : Global::Private(d, &zkwinft_output_device_v1_interface, s_version)
    , q(q)
{
    s_privates << this;
}

OutputDeviceV1Interface::Private::~Private()
{
    s_privates.removeAll(this);
}

OutputDeviceV1Interface *OutputDeviceV1Interface::Private::get(wl_resource *native)
{
    if (Private *p = cast(native)) {
        return p->q;
    }
    return nullptr;
}

OutputDeviceV1Interface::Private *OutputDeviceV1Interface::Private::cast(wl_resource *native)
{
    for (auto it = s_privates.constBegin(); it != s_privates.constEnd(); ++it) {
        const auto &resources = (*it)->resources;
        auto rit = std::find_if(resources.begin(), resources.end(),
                                [native] (const ResourceData &data) {
                                    return data.resource == native;
                                }
        );
        if (rit != resources.end()) {
            return (*it);
        }
    }
    return nullptr;
}

OutputDeviceV1Interface::OutputDeviceV1Interface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
//    Q_D();
//    connect(this, &OutputDeviceV1Interface::currentModeChanged, this,
//        [d] {
//            Q_ASSERT(d->currentMode.id >= 0);
//            for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
//                d->sendMode((*it).resource, d->currentMode);
//                d->sendDone(*it);
//            }
//            wl_display_flush_clients(*(d->display));
//        }
//    );
}

OutputDeviceV1Interface::~OutputDeviceV1Interface() = default;

QSize OutputDeviceV1Interface::modeSize() const
{
    Q_D();

    if (d->pending.mode.id == -1) {
        return QSize();
    }
    return d->pending.mode.size;
}

OutputDeviceV1Interface *OutputDeviceV1Interface::get(wl_resource* native)
{
    return Private::get(native);
}

int OutputDeviceV1Interface::refreshRate() const
{
    Q_D();

    if (d->pending.mode.id == -1) {
        return 60000;
    }
    return d->pending.mode.refreshRate;
}

void OutputDeviceV1Interface::Private::addMode(Mode &mode)
{
    bool currentChanged = false;
    bool preferredChanged = false;

    auto removeFlag = [this](Mode &mode, ModeFlag flag) {
        mode.flags &= ~uint(flag);
        if (published.mode.id ==  mode.id) {
            published.mode.flags = mode.flags;
            published.mode.flags = mode.flags;
        }
    };

    if (mode.flags.testFlag(ModeFlag::Current)) {
        Q_ASSERT(published.mode.id < 0);
        if (published.mode.id >= 0 && published.mode.id != mode.id) {
            qCWarning(WRAPLAND_SERVER) << "Duplicate current Mode id" << published.mode.id
                                       << "and" << mode.id <<  mode.size << mode.refreshRate
                                       << ": setting current mode to:"
                                       << mode.size << mode.refreshRate;

        }
        mode.flags &= ~uint(ModeFlag::Current);
        published.mode = mode;
        pending.mode = mode;
        currentChanged = true;
    }

    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        // Check that there is not yet a Preferred mode.
        auto preferredIt = std::find_if(modes.begin(), modes.end(),
            [](const Mode &mode) {
                return mode.flags.testFlag(ModeFlag::Preferred);
            }
        );
        if (preferredIt != modes.end()) {
            qCWarning(WRAPLAND_SERVER) << "Duplicate preferred Mode id" << (*preferredIt).id
                                       << "and" << mode.id <<  mode.size << mode.refreshRate
                                       << ": removing flag from previous mode:"
                                       << (*preferredIt).size << (*preferredIt).refreshRate;
            removeFlag(*preferredIt, ModeFlag::Preferred);
        }
        preferredChanged = true;
    }

    auto existingModeIt = std::find_if(modes.begin(), modes.end(),
        [mode](const Mode &mode_it) {
            return mode.size == mode_it.size &&
                   mode.refreshRate == mode_it.refreshRate &&
                   mode.id == mode_it.id;
        }
    );
    auto emitCurrentChange = [this, &mode, currentChanged] {
        if (currentChanged) {
            Q_EMIT q->refreshRateChanged(mode.refreshRate);
            Q_EMIT q->modeSizeChanged(mode.size);
            Q_EMIT q->currentModeChanged();
        }
    };

    if (existingModeIt != modes.end()) {
        // Mode already exists in modes list. Just update its flags and emit signals if necessary.
        (*existingModeIt).flags = mode.flags;
        emitCurrentChange();
        if (currentChanged || preferredChanged) {
            Q_EMIT q->modesChanged();
        }
        return;
    }

    auto idIt = std::find_if(modes.constBegin(), modes.constEnd(),
                             [mode](const Mode &mode_it) { return mode.id == mode_it.id; } );
    if (idIt != modes.constEnd()) {
        qCWarning(WRAPLAND_SERVER) << "Duplicate Mode id" << mode.id << ": not adding mode"
                                   << mode.size << mode.refreshRate;
        return;
    }

    modes << mode;
    emitCurrentChange();
    Q_EMIT q->modesChanged();
}

void OutputDeviceV1Interface::addMode(Mode &mode)
{
    Q_ASSERT(!isValid());
    Q_ASSERT(mode.id >= 0);
    Q_ASSERT(mode.size.isValid());

    Q_D();
    d->addMode(mode);
}

void OutputDeviceV1Interface::Private::setMode(int id)
{
    auto existingModeIt = std::find_if(modes.begin(), modes.end(),
        [id](const Mode &mode) {
            return mode.id == id;
        }
    );

    Q_ASSERT(existingModeIt != modes.end());
    pending.mode = *existingModeIt;

    emit q->modesChanged();
    emit q->refreshRateChanged(pending.mode.refreshRate);
    emit q->modeSizeChanged(pending.mode.size);
    emit q->currentModeChanged();
}

void OutputDeviceV1Interface::setMode(int id)
{
    Q_D();
    d->setMode(id);
}

void OutputDeviceV1Interface::Private::create()
{
    // This is just a quick way to copy all pending to published data. There are no clients
    // connected yet.
    Q_ASSERT(resources.isEmpty());
    q->broadcast();
    Global::Private::create();
}

void OutputDeviceV1Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&zkwinft_output_device_v1_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_user_data(resource, this);
    wl_resource_set_destructor(resource, unbind);
    ResourceData r;
    r.resource = resource;
    r.version = version;
    resources << r;

    sendInfo(r);

    auto currentModeIt = modes.constEnd();
    for (auto it = modes.constBegin(); it != modes.constEnd(); ++it) {
        const Mode &mode = *it;
        if (mode.id == published.mode.id) {
            // Needs to be sent as last mode.
            currentModeIt = it;
            continue;
        }
        sendMode(r, mode);
    }

    if (currentModeIt != modes.constEnd()) {
        sendMode(r, *currentModeIt);
    }

    sendEnabled(r);
    sendGeometry(r);

    sendDone(r);
    c->flush();
}

void OutputDeviceV1Interface::Private::unbind(wl_resource *resource)
{
    Private *o = cast(resource);
    if (!o) {
        return;
    }
    auto it = std::find_if(o->resources.begin(), o->resources.end(), [resource](const ResourceData &r) { return r.resource == resource; });
    if (it != o->resources.end()) {
        o->resources.erase(it);
    }
}

void OutputDeviceV1Interface::Private::sendInfo(const ResourceData &data)
{
    const auto &info = published.info;
    zkwinft_output_device_v1_send_info(data.resource,
                                       info.uuid.constData(),
                                       qPrintable(info.eisaId),
                                       qPrintable(info.serialNumber),
                                       info.edid.toBase64().constData(),
                                       qPrintable(info.manufacturer),
                                       qPrintable(info.model),
                                       info.physicalSize.width(),
                                       info.physicalSize.height());
}

void OutputDeviceV1Interface::Private::sendEnabled(const ResourceData &data)
{
    const int enabled = published.enabled == OutputDeviceV1Interface::Enablement::Enabled;
    zkwinft_output_device_v1_send_enabled(data.resource, enabled);
}

void OutputDeviceV1Interface::Private::sendMode(const ResourceData &data, const Mode &mode)
{
    int32_t flags = 0;
    if (mode.id == published.mode.id) {
        flags |= WL_OUTPUT_MODE_CURRENT;
    }
    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        flags |= WL_OUTPUT_MODE_PREFERRED;
    }
    zkwinft_output_device_v1_send_mode(data.resource, flags,
                                       mode.size.width(), mode.size.height(),
                                       mode.refreshRate, mode.id);
}

void OutputDeviceV1Interface::Private::sendGeometry(const ResourceData &data)
{
    const QRectF &geo = published.geometry;
    zkwinft_output_device_v1_send_geometry(data.resource,
                                           wl_fixed_from_double(geo.x()),
                                           wl_fixed_from_double(geo.y()),
                                           wl_fixed_from_double(geo.width()),
                                           wl_fixed_from_double(geo.height()));
}

int32_t toTransform(OutputDeviceV1Interface::Transform transform)
{
    switch (transform) {
    case OutputDeviceV1Interface::Transform::Normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case OutputDeviceV1Interface::Transform::Rotated90:
        return WL_OUTPUT_TRANSFORM_90;
    case OutputDeviceV1Interface::Transform::Rotated180:
        return WL_OUTPUT_TRANSFORM_180;
    case OutputDeviceV1Interface::Transform::Rotated270:
        return WL_OUTPUT_TRANSFORM_270;
    case OutputDeviceV1Interface::Transform::Flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case OutputDeviceV1Interface::Transform::Flipped90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case OutputDeviceV1Interface::Transform::Flipped180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case OutputDeviceV1Interface::Transform::Flipped270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    }
    abort();
}

void OutputDeviceV1Interface::Private::sendTransform(const ResourceData &data)
{
    zkwinft_output_device_v1_send_transform(data.resource, toTransform(published.transform));
}

void OutputDeviceV1Interface::Private::sendDone(const ResourceData &data)
{
    zkwinft_output_device_v1_send_done(data.resource);
}

#define SETTER(setterName, type, argumentName, variableName) \
    void OutputDeviceV1Interface::setterName(type arg) \
    { \
        Q_D(); \
        if (d->pending.argumentName == arg) { \
            return; \
        } \
        d->pending.argumentName = arg; \
        emit variableName##Changed(d->pending.argumentName); \
    }

#define SETTER2(setterName, type, argumentName) SETTER(setterName, type, argumentName, argumentName)

SETTER(setSerialNumber, const QString&, info.serialNumber, serialNumber)
SETTER(setEisaId, const QString&, info.eisaId, eisaId)
SETTER(setUuid, const QByteArray&, info.uuid, uuid)
SETTER(setEdid, const QByteArray&, info.edid, edid)
SETTER(setManufacturer, const QString&, info.manufacturer, manufacturer)
SETTER(setModel, const QString&, info.model, model)
SETTER(setPhysicalSize, const QSize&, info.physicalSize, physicalSize)
SETTER2(setEnabled, Enablement, enabled)
SETTER2(setTransform, Transform, transform)
SETTER2(setGeometry, const QRectF&, geometry)

#undef SETTER
#undef SETTER2

QSize OutputDeviceV1Interface::physicalSize() const
{
    Q_D();
    return d->pending.info.physicalSize;
}

QRectF OutputDeviceV1Interface::geometry() const
{
    Q_D();
    return d->pending.geometry;
}

QString OutputDeviceV1Interface::serialNumber() const
{
    Q_D();
    return d->pending.info.serialNumber;
}

QString OutputDeviceV1Interface::eisaId() const
{
    Q_D();
    return d->pending.info.eisaId;
}

QString OutputDeviceV1Interface::manufacturer() const
{
    Q_D();
    return d->pending.info.manufacturer;
}

QString OutputDeviceV1Interface::model() const
{
    Q_D();
    return d->pending.info.model;
}

QList< OutputDeviceV1Interface::Mode > OutputDeviceV1Interface::modes() const
{
    Q_D();
    return d->modes;
}

int OutputDeviceV1Interface::modeId() const
{
    Q_D();
    for (const Mode &m: d->modes) {
        if (m.flags.testFlag(OutputDeviceV1Interface::ModeFlag::Current)) {
            return m.id;
        }
    }
    return -1;
}

OutputDeviceV1Interface::Transform OutputDeviceV1Interface::transform() const
{
    Q_D();
    return d->pending.transform;
}

OutputDeviceV1Interface::Private *OutputDeviceV1Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

QByteArray OutputDeviceV1Interface::uuid() const
{
    Q_D();
    return d->pending.info.uuid;
}

QByteArray OutputDeviceV1Interface::edid() const
{
    Q_D();
    return d->pending.info.edid;
}

OutputDeviceV1Interface::Enablement OutputDeviceV1Interface::enabled() const
{
    Q_D();
    return d->pending.enabled;
}

bool OutputDeviceV1Interface::Private::broadcastMode()
{
    if (pending.mode.id == published.mode.id) {
        return false;
    }
    published.mode = pending.mode;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendMode(*it, published.mode);
    }
    return true;
}

#define BROADCASTER(capitalName, lowercaseName) \
    bool OutputDeviceV1Interface::Private::broadcast##capitalName() \
    { \
        if (pending.lowercaseName == published.lowercaseName) { \
            return false; \
        } \
        published.lowercaseName = pending.lowercaseName; \
        for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) { \
            send##capitalName(*it); \
        } \
        return true; \
    }

BROADCASTER(Info, info)
BROADCASTER(Enabled, enabled)
BROADCASTER(Transform, transform)
BROADCASTER(Geometry, geometry)

#undef BROADCASTER

void OutputDeviceV1Interface::Private::broadcast()
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

    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendDone(*it);
    }
}

void OutputDeviceV1Interface::broadcast()
{
    Q_D();
    d->broadcast();
}

}
}
