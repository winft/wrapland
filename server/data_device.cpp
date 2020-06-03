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
#include "data_device.h"

#include "data_device_manager.h"
#include "data_offer_p.h"
#include "data_source.h"
#include "data_source_p.h"

#include "seat.h"
#include "surface.h"
#include "surface_p.h"

#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

class DataDevice::Private : public Wayland::Resource<DataDevice>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Seat* seat, DataDevice* q);
    ~Private() override;

    DataOffer* createDataOffer(AbstractDataSource* source);

    Seat* seat;
    DataSource* source = nullptr;
    Surface* surface = nullptr;
    Surface* icon = nullptr;

    DataSource* selection = nullptr;
    QMetaObject::Connection selectionDestroyedConnection;

    struct Drag {
        Surface* surface = nullptr;
        QMetaObject::Connection destroyConnection;
        QMetaObject::Connection posConnection;
        QMetaObject::Connection sourceActionConnection;
        QMetaObject::Connection targetActionConnection;
        quint32 serial = 0;
    };
    Drag drag;

    QPointer<Surface> proxyRemoteSurface;

private:
    static void startDragCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  wl_resource* wlSource,
                                  wl_resource* wlOrigin,
                                  wl_resource* wlIcon,
                                  uint32_t serial);
    static void setSelectionCallback(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     wl_resource* wlSource,
                                     uint32_t serial);

    void startDrag(DataSource* dataSource, Surface* origin, Surface* icon, quint32 serial);
    void setSelection(wl_resource* sourceResource);

    static const struct wl_data_device_interface s_interface;

    DataDevice* q_ptr;
};

const struct wl_data_device_interface DataDevice::Private::s_interface = {
    startDragCallback,
    setSelectionCallback,
    destroyCallback,
};

DataDevice::Private::Private(Client* client,
                             uint32_t version,
                             uint32_t id,
                             Seat* seat,
                             DataDevice* q)
    : Wayland::Resource<DataDevice>(client, version, id, &wl_data_device_interface, &s_interface, q)
    , seat(seat)
    , q_ptr{q}
{
}

DataDevice::Private::~Private() = default;

void DataDevice::Private::startDragCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            wl_resource* wlSource,
                                            wl_resource* wlOrigin,
                                            wl_resource* wlIcon,
                                            uint32_t serial)
{
    auto priv = handle(wlResource)->d_ptr;
    auto source = wlSource ? Resource<DataSource>::handle(wlSource) : nullptr;
    auto origin = Resource<Surface>::handle(wlOrigin);
    auto icon = wlIcon ? Resource<Surface>::handle(wlIcon) : nullptr;

    priv->startDrag(source, origin, icon, serial);
}

void DataDevice::Private::startDrag(DataSource* dataSource,
                                    Surface* origin,
                                    Surface* _icon,
                                    quint32 serial)
{
    // TODO(unknown author): verify serial

    auto focusSurface = origin;

    if (proxyRemoteSurface) {
        // origin is a proxy surface
        focusSurface = proxyRemoteSurface.data();
    }

    const bool pointerGrab
        = seat->hasImplicitPointerGrab(serial) && seat->focusedPointerSurface() == focusSurface;

    if (!pointerGrab) {
        // Client doesn't have pointer grab.
        if (!seat->hasImplicitTouchGrab(serial) || seat->focusedTouchSurface() != focusSurface) {
            // Client neither has pointer nor touch grab. No drag start allowed.
            return;
        }
    }

    // Source is allowed to be null, handled client internally.
    source = dataSource;
    if (dataSource) {
        QObject::connect(
            dataSource, &DataSource::resourceDestroyed, q_ptr, [this] { source = nullptr; });
    }

    surface = origin;
    icon = _icon;
    drag.serial = serial;
    Q_EMIT q_ptr->dragStarted();
}

void DataDevice::Private::setSelectionCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               wl_resource* wlSource,
                                               [[maybe_unused]] uint32_t serial)
{
    // TODO(unknown author): verify serial
    auto priv = handle(wlResource)->d_ptr;
    priv->setSelection(wlSource);
}

void DataDevice::Private::setSelection(wl_resource* sourceResource)
{
    // TODO(unknown author): verify serial

    auto dataSource = sourceResource ? Resource<DataSource>::handle(sourceResource) : nullptr;

    // TODO(romangg): move errors into Wayland namespace.
    if (dataSource && dataSource->supportedDragAndDropActions()
        && wl_resource_get_version(sourceResource) >= WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        wl_resource_post_error(sourceResource,
                               WL_DATA_SOURCE_ERROR_INVALID_SOURCE,
                               "Data source is for drag and drop");
        return;
    }

    if (selection == dataSource) {
        return;
    }

    QObject::disconnect(selectionDestroyedConnection);

    if (selection) {
        selection->cancel();
    }

    selection = dataSource;

    if (selection) {
        auto clearSelection = [this] { setSelection(nullptr); };
        selectionDestroyedConnection
            = QObject::connect(selection, &DataSource::resourceDestroyed, q_ptr, clearSelection);
        Q_EMIT q_ptr->selectionChanged(selection);
    } else {
        selectionDestroyedConnection = QMetaObject::Connection();
        Q_EMIT q_ptr->selectionCleared();
    }
}

DataOffer* DataDevice::Private::createDataOffer(AbstractDataSource* source)
{
    if (!source) {
        // A data offer can only exist together with a source.
        return nullptr;
    }

    auto offer = new DataOffer(client()->handle(), version(), source);

    if (!offer->d_ptr->resource()) {
        // TODO(unknown author): send error?
        delete offer;
        return nullptr;
    }

    send<wl_data_device_send_data_offer>(offer->d_ptr->resource());
    offer->sendAllOffers();
    return offer;
}

DataDevice::DataDevice(Client* client, uint32_t version, uint32_t id, Seat* seat)
    : d_ptr(new Private(client, version, id, seat, this))
{
}

Seat* DataDevice::seat() const
{
    return d_ptr->seat;
}

DataSource* DataDevice::dragSource() const
{

    return d_ptr->source;
}

Surface* DataDevice::icon() const
{
    return d_ptr->icon;
}

Surface* DataDevice::origin() const
{
    return d_ptr->proxyRemoteSurface ? d_ptr->proxyRemoteSurface.data() : d_ptr->surface;
}

DataSource* DataDevice::selection() const
{
    return d_ptr->selection;
}

void DataDevice::sendSelection(AbstractDataSource* other)
{
    auto offer = d_ptr->createDataOffer(other);
    if (!offer) {
        return;
    }

    d_ptr->send<wl_data_device_send_selection>(offer->d_ptr->resource());
}

void DataDevice::sendClearSelection()
{
    d_ptr->send<wl_data_device_send_selection>(nullptr);
}

void DataDevice::drop()
{
    d_ptr->send<wl_data_device_send_drop>();

    if (d_ptr->drag.posConnection) {
        disconnect(d_ptr->drag.posConnection);
        d_ptr->drag.posConnection = QMetaObject::Connection();
    }

    disconnect(d_ptr->drag.destroyConnection);
    d_ptr->drag.destroyConnection = QMetaObject::Connection();
    d_ptr->drag.surface = nullptr;

    // TODO(romangg): do we need to flush the client here?
}

void DataDevice::updateDragTarget(Surface* surface, quint32 serial)
{
    if (d_ptr->drag.surface) {
        if (d_ptr->resource() && d_ptr->drag.surface->d_ptr->resource()) {
            d_ptr->send<wl_data_device_send_leave>();
        }
        if (d_ptr->drag.posConnection) {
            disconnect(d_ptr->drag.posConnection);
            d_ptr->drag.posConnection = QMetaObject::Connection();
        }
        disconnect(d_ptr->drag.destroyConnection);
        d_ptr->drag.destroyConnection = QMetaObject::Connection();
        d_ptr->drag.surface = nullptr;
        if (d_ptr->drag.sourceActionConnection) {
            disconnect(d_ptr->drag.sourceActionConnection);
            d_ptr->drag.sourceActionConnection = QMetaObject::Connection();
        }
        if (d_ptr->drag.targetActionConnection) {
            disconnect(d_ptr->drag.targetActionConnection);
            d_ptr->drag.targetActionConnection = QMetaObject::Connection();
        }
        // don't update serial, we need it
    }
    if (!surface) {
        if (auto s = d_ptr->seat->dragSource()->dragSource()) {
            s->dndAction(DataDeviceManager::DnDAction::None);
        }
        return;
    }
    if (d_ptr->proxyRemoteSurface && d_ptr->proxyRemoteSurface == surface) {
        // A proxy can not have the remote surface as target. All other surfaces even of itself
        // are fine. Such surfaces get data offers from themselves while a drag is ongoing.
        return;
    }
    auto* source = d_ptr->seat->dragSource()->dragSource();
    DataOffer* offer = d_ptr->createDataOffer(source);
    d_ptr->drag.surface = surface;
    if (d_ptr->seat->isDragPointer()) {
        d_ptr->drag.posConnection = connect(d_ptr->seat, &Seat::pointerPosChanged, this, [this] {
            const QPointF pos
                = d_ptr->seat->dragSurfaceTransformation().map(d_ptr->seat->pointerPos());
            d_ptr->send<wl_data_device_send_motion>(d_ptr->seat->timestamp(),
                                                    wl_fixed_from_double(pos.x()),
                                                    wl_fixed_from_double(pos.y()));
            d_ptr->client()->flush();
        });
    } else if (d_ptr->seat->isDragTouch()) {
        d_ptr->drag.posConnection
            = connect(d_ptr->seat,
                      &Seat::touchMoved,
                      this,
                      [this](qint32 id, quint32 serial, const QPointF& globalPosition) {
                          Q_UNUSED(id);
                          if (serial != d_ptr->drag.serial) {
                              // different touch down has been moved
                              return;
                          }
                          const QPointF pos
                              = d_ptr->seat->dragSurfaceTransformation().map(globalPosition);
                          d_ptr->send<wl_data_device_send_motion>(d_ptr->seat->timestamp(),
                                                                  wl_fixed_from_double(pos.x()),
                                                                  wl_fixed_from_double(pos.y()));
                          d_ptr->client()->flush();
                      });
    }
    d_ptr->drag.destroyConnection
        = connect(d_ptr->drag.surface, &Surface::resourceDestroyed, this, [this] {
              if (d_ptr->resource()) {
                  d_ptr->send<wl_data_device_send_leave>();
              }
              if (d_ptr->drag.posConnection) {
                  disconnect(d_ptr->drag.posConnection);
              }
              d_ptr->drag = Private::Drag();
          });

    // TODO(unknown author): handle touch position
    const QPointF pos = d_ptr->seat->dragSurfaceTransformation().map(d_ptr->seat->pointerPos());
    d_ptr->send<wl_data_device_send_enter>(serial,
                                           surface->d_ptr->resource(),
                                           wl_fixed_from_double(pos.x()),
                                           wl_fixed_from_double(pos.y()),
                                           offer ? offer->d_ptr->resource() : nullptr);
    if (offer) {
        offer->d_ptr->sendSourceActions();

        auto matchOffers = [source, offer] {
            DataDeviceManager::DnDAction action{DataDeviceManager::DnDAction::None};

            if (source->supportedDragAndDropActions().testFlag(
                    offer->preferredDragAndDropAction())) {
                action = offer->preferredDragAndDropAction();

            } else {

                if (source->supportedDragAndDropActions().testFlag(
                        DataDeviceManager::DnDAction::Copy)
                    && offer->supportedDragAndDropActions().testFlag(
                        DataDeviceManager::DnDAction::Copy)) {
                    action = DataDeviceManager::DnDAction::Copy;
                }

                else if (source->supportedDragAndDropActions().testFlag(
                             DataDeviceManager::DnDAction::Move)
                         && offer->supportedDragAndDropActions().testFlag(
                             DataDeviceManager::DnDAction::Move)) {
                    action = DataDeviceManager::DnDAction::Move;
                }

                else if (source->supportedDragAndDropActions().testFlag(
                             DataDeviceManager::DnDAction::Ask)
                         && offer->supportedDragAndDropActions().testFlag(
                             DataDeviceManager::DnDAction::Ask)) {
                    action = DataDeviceManager::DnDAction::Ask;
                }
            }

            offer->dndAction(action);
            source->dndAction(action);
        };
        d_ptr->drag.targetActionConnection
            = connect(offer, &DataOffer::dragAndDropActionsChanged, offer, matchOffers);
        d_ptr->drag.sourceActionConnection
            = connect(source, &DataSource::supportedDragAndDropActionsChanged, source, matchOffers);
    }
    d_ptr->client()->flush();
}

quint32 DataDevice::dragImplicitGrabSerial() const
{
    return d_ptr->drag.serial;
}

void DataDevice::updateProxy(Surface* remote)
{
    // TODO(romangg): connect destroy signal?
    d_ptr->proxyRemoteSurface = remote;
}

Client* DataDevice::client() const
{
    return d_ptr->client()->handle();
}

}
