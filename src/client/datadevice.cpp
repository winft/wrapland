/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "datadevice.h"
#include "dataoffer.h"
#include "datasource.h"
#include "selection_device_p.h"
#include "surface.h"
#include "wayland_pointer_p.h"
// Qt
#include <QPointer>
// Wayland
#include <wayland-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN DataDevice::Private
{
public:
    explicit Private(DataDevice* q);
    void setup(wl_data_device* d);

    WaylandPointer<wl_data_device, wl_data_device_release> device;
    std::unique_ptr<DataOffer> selectionOffer;
    struct Drag {
        QPointer<DataOffer> offer;
        QPointer<Surface> surface;
    };
    Drag drag;

    void dragEnter(quint32 serial,
                   QPointer<Surface> const& surface,
                   QPointF const& relativeToSurface,
                   wl_data_offer* dataOffer);
    void dragLeft();
    static void enterCallback(void* data,
                              wl_data_device* dataDevice,
                              uint32_t serial,
                              wl_surface* surface,
                              wl_fixed_t x,
                              wl_fixed_t y,
                              wl_data_offer* id);
    static void leaveCallback(void* data, wl_data_device* dataDevice);
    static void motionCallback(void* data,
                               wl_data_device* dataDevice,
                               uint32_t time,
                               wl_fixed_t x,
                               wl_fixed_t y);
    static void dropCallback(void* data, wl_data_device* dataDevice);

    static const struct wl_data_device_listener s_listener;

    DataDevice* q;
    DataOffer* lastOffer = nullptr;
};

wl_data_device_listener const DataDevice::Private::s_listener = {
    data_offer_callback<Private>,
    enterCallback,
    leaveCallback,
    motionCallback,
    dropCallback,
    selection_callback<Private>,
};

void DataDevice::Private::enterCallback(void* data,
                                        wl_data_device* dataDevice,
                                        uint32_t serial,
                                        wl_surface* surface,
                                        wl_fixed_t x,
                                        wl_fixed_t y,
                                        wl_data_offer* id)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->device == dataDevice);
    d->dragEnter(serial,
                 QPointer<Surface>(Surface::get(surface)),
                 QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)),
                 id);
}

void DataDevice::Private::dragEnter(quint32 serial,
                                    QPointer<Surface> const& surface,
                                    QPointF const& relativeToSurface,
                                    wl_data_offer* dataOffer)
{
    drag.surface = surface;
    Q_ASSERT(*lastOffer == dataOffer);
    drag.offer = lastOffer;
    lastOffer = nullptr;
    Q_EMIT q->dragEntered(serial, relativeToSurface);
}

void DataDevice::Private::leaveCallback(void* data, wl_data_device* dataDevice)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->device == dataDevice);
    d->dragLeft();
}

void DataDevice::Private::dragLeft()
{
    if (drag.offer) {
        delete drag.offer;
    }
    drag = Drag();
    Q_EMIT q->dragLeft();
}

void DataDevice::Private::motionCallback(void* data,
                                         wl_data_device* dataDevice,
                                         uint32_t time,
                                         wl_fixed_t x,
                                         wl_fixed_t y)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->device == dataDevice);
    Q_EMIT d->q->dragMotion(QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), time);
}

void DataDevice::Private::dropCallback(void* data, wl_data_device* dataDevice)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->device == dataDevice);
    Q_EMIT d->q->dropped();
}

DataDevice::Private::Private(DataDevice* q)
    : q(q)
{
}

void DataDevice::Private::setup(wl_data_device* d)
{
    Q_ASSERT(d);
    Q_ASSERT(!device.isValid());
    device.setup(d);
    wl_data_device_add_listener(device, &s_listener, this);
}

DataDevice::DataDevice(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

DataDevice::~DataDevice()
{
    if (d->drag.offer) {
        delete d->drag.offer;
    }
    release();
}

void DataDevice::release()
{
    d->device.release();
}

bool DataDevice::isValid() const
{
    return d->device.isValid();
}

void DataDevice::setup(wl_data_device* dataDevice)
{
    d->setup(dataDevice);
}

void DataDevice::startDragInternally(quint32 serial, Surface* origin, Surface* icon)
{
    startDrag(serial, nullptr, origin, icon);
}

namespace
{
static wl_data_source* dataSource(DataSource const* source)
{
    if (!source) {
        return nullptr;
    }
    return *source;
}
}

void DataDevice::startDrag(quint32 serial, DataSource* source, Surface* origin, Surface* icon)
{
    wl_data_device_start_drag(
        d->device, dataSource(source), *origin, icon ? (wl_surface*)*icon : nullptr, serial);
}

void DataDevice::setSelection(quint32 serial, DataSource* source)
{
    wl_data_device_set_selection(d->device, dataSource(source), serial);
}

DataOffer* DataDevice::offeredSelection() const
{
    return d->selectionOffer.get();
}

QPointer<Surface> DataDevice::dragSurface() const
{
    return d->drag.surface;
}

DataOffer* DataDevice::dragOffer() const
{
    return d->drag.offer;
}

DataDevice::operator wl_data_device*()
{
    return d->device;
}

DataDevice::operator wl_data_device*() const
{
    return d->device;
}

}
}
