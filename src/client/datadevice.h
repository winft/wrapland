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
#ifndef WAYLAND_DATADEVICE_H
#define WAYLAND_DATADEVICE_H

#include "dataoffer.h"

#include <QObject>
// STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct wl_data_device;

namespace Wrapland
{
namespace Client
{
class DataSource;
class Surface;

/**
 * @short DataDevice allows clients to share data by copy-and-paste and drag-and-drop.
 *
 * This class is a convenient wrapper for the wl_data_device interface.
 * To create a DataDevice call DataDeviceManager::getDevice.
 *
 * @see DataDeviceManager
 **/
class WRAPLANDCLIENT_EXPORT DataDevice : public QObject
{
    Q_OBJECT
public:
    using source_t = Wrapland::Client::DataSource;
    using offer_type = Wrapland::Client::DataOffer;

    explicit DataDevice(QObject* parent = nullptr);
    virtual ~DataDevice();

    /**
     * Setup this DataDevice to manage the @p dataDevice.
     * When using DataDeviceManager::createDataDevice there is no need to call this
     * method.
     **/
    void setup(wl_data_device* dataDevice);
    /**
     * Releases the wl_data_device interface.
     * After the interface has been released the DataDevice instance is no
     * longer valid and can be setup with another wl_data_device interface.
     **/
    void release();

    /**
     * @returns @c true if managing a wl_data_device.
     **/
    bool isValid() const;

    void startDrag(quint32 serial, DataSource* source, Surface* origin, Surface* icon = nullptr);
    void startDragInternally(quint32 serial, Surface* origin, Surface* icon = nullptr);

    void setSelection(quint32 serial, DataSource* source = nullptr);
    void clearSelection(quint32 serial);

    DataOffer* offeredSelection() const;

    /**
     * @returns the currently focused surface during drag'n'drop on this DataDevice.
     * @since 0.0.522
     **/
    QPointer<Surface> dragSurface() const;
    /**
     * @returns the DataOffer during a drag'n'drop operation.
     * @since 0.0.522
     **/
    DataOffer* dragOffer() const;

    operator wl_data_device*();
    operator wl_data_device*() const;

Q_SIGNALS:
    void selectionOffered(Wrapland::Client::DataOffer*);
    void selectionCleared();
    /**
     * Notification that a drag'n'drop operation entered a Surface on this DataDevice.
     *
     * @param serial The serial for this enter
     * @param relativeToSurface Coordinates relative to the upper-left corner of the Surface.
     * @see dragSurface
     * @see dragOffer
     * @see dragLeft
     * @see dragMotion
     * @since 0.0.522
     **/
    void dragEntered(quint32 serial, const QPointF& relativeToSurface);
    /**
     * Notification that the drag'n'drop operation left the Surface on this DataDevice.
     *
     * The leave notification is sent before the enter notification for the new focus.
     * @see dragEnter
     * @since 0.0.522
     **/
    void dragLeft();
    /**
     * Notification of drag motion events on the current drag surface.
     *
     * @param relativeToSurface  Coordinates relative to the upper-left corner of the entered
     * Surface.
     * @param time timestamp with millisecond granularity
     * @see dragEntered
     * @since 0.0.522
     **/
    void dragMotion(const QPointF& relativeToSurface, quint32 time);
    /**
     * Emitted when the implicit grab is removed and the drag'n'drop operation ended on this
     * DataDevice.
     *
     * The client can now start a data transfer on the DataOffer.
     * @see dragEntered
     * @see dragOffer
     * @since 0.0.522
     **/
    void dropped();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
