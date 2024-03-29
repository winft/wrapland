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
#ifndef WAYLAND_DATA_DEVICE_MANAGER_H
#define WAYLAND_DATA_DEVICE_MANAGER_H

#include <QObject>
// STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct wl_data_device_manager;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class DataDevice;
class DataSource;
class Seat;

/**
 * @short Wrapper for the wl_data_device_manager interface.
 *
 * This class provides a convenient wrapper for the wl_data_device_manager interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the DataDeviceManager interface:
 * @code
 * DataDeviceManager *m = registry->createDataDeviceManager(name, version);
 * @endcode
 *
 * This creates the DataDeviceManager and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * DataDeviceManager *m = new DataDeviceManager;
 * m->setup(registry->bindDataDeviceManager(name, version));
 * @endcode
 *
 * The DataDeviceManager can be used as a drop-in replacement for any wl_data_device_manager
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT DataDeviceManager : public QObject
{
    Q_OBJECT
public:
    using device_t = Wrapland::Client::DataDevice;
    using source_t = Wrapland::Client::DataSource;

    /**
     * Drag and Drop actions supported by DataSource and DataOffer.
     * @since 0.0.542
     **/
    enum class DnDAction {
        None = 0,
        Copy = 1 << 0,
        Move = 1 << 1,
        Ask = 1 << 2,
    };
    Q_DECLARE_FLAGS(DnDActions, DnDAction)

    /**
     * Creates a new Compositor.
     * Note: after constructing the Compositor it is not yet valid and one needs
     * to call setup. In order to get a ready to use Compositor prefer using
     * Registry::createCompositor.
     **/
    explicit DataDeviceManager(QObject* parent = nullptr);
    virtual ~DataDeviceManager();

    /**
     * @returns @c true if managing a wl_data_device_manager.
     **/
    bool isValid() const;
    /**
     * Setup this DataDeviceManager to manage the @p manager.
     * When using Registry::createDataDeviceManager there is no need to call this
     * method.
     **/
    void setup(wl_data_device_manager* manager);
    /**
     * Releases the wl_data_device_manager interface.
     * After the interface has been released the DataDeviceManager instance is no
     * longer valid and can be setup with another wl_data_device_manager interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating a DataSource.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a DataSource.
     **/
    EventQueue* eventQueue();

    DataSource* createSource(QObject* parent = nullptr);

    DataDevice* getDevice(Seat* seat, QObject* parent = nullptr);

    operator wl_data_device_manager*();
    operator wl_data_device_manager*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the Compositor got created by
     * Registry::createDataDeviceManager
     *
     * @since 5.5
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::DataDeviceManager::DnDActions)

#endif
