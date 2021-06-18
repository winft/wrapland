/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct zwp_primary_selection_device_manager_v1;
struct zwp_primary_selection_device_v1;
struct zwp_primary_selection_offer_v1;
struct zwp_primary_selection_source_v1;
class QMimeType;

namespace Wrapland::Client
{

class EventQueue;
class PrimarySelectionSource;
class PrimarySelectionDevice;
class PrimarySelectionOffer;
class Seat;
class Surface;

/**
 * @short Wrapper for the zwp_primary_selection_device_manager_v1 interface.
 *
 * This class provides a convenient wrapper for the zwp_primary_selection_device_manager_v1
 *interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the PrimarySelectionDeviceManager interface:
 * @code
 * PrimarySelectionDeviceManager *m = registry->createPrimarySelectionDeviceManager(name, version);
 * @endcode
 *
 * This creates the PrimarySelectionDeviceManager and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * PrimarySelectionDeviceManager *m = new PrimarySelectionDeviceManager;
 * m->setup(registry->bindPrimarySelectionDeviceManager(name, version));
 * @endcode
 *
 * The PrimarySelectionDeviceManager can be used as a drop-in replacement for any
 *zwp_primary_selection_device_manager_v1 pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT PrimarySelectionDeviceManager : public QObject
{
    Q_OBJECT
public:
    using device_t = Wrapland::Client::PrimarySelectionDevice;
    using source_t = Wrapland::Client::PrimarySelectionSource;

    /**
     * Creates a new PrimarySelectionDeviceManager.
     * Note: after construction the object is not yet valid and one needs
     * to call setup. In order to get a ready to use object prefer using
     * @c Registry::createPrimarySelectionDeviceManager .
     **/
    explicit PrimarySelectionDeviceManager(QObject* parent = nullptr);
    ~PrimarySelectionDeviceManager() override;

    /**
     * @returns @c true if managing a zwp_primary_selection_device_manager_v1.
     **/
    bool isValid() const;
    /**
     * Setup this PrimarySelectionDeviceManager to manage the @p manager.
     * When using Registry::createPrimarySelectionDeviceManager there is no need to call this
     * method.
     **/
    void setup(zwp_primary_selection_device_manager_v1* manager);
    /**
     * Releases the zwp_primary_selection_device_manager_v1 interface.
     * After the interface has been released the PrimarySelectionDeviceManager instance is no
     * longer valid and can be setup with another zwp_primary_selection_device_manager_v1 interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating a PrimarySelectionSource.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a PrimarySelectionSource.
     **/
    EventQueue* eventQueue();

    source_t* createSource(QObject* parent = nullptr);

    device_t* getDevice(Seat* seat, QObject* parent = nullptr);

    operator zwp_primary_selection_device_manager_v1*();
    operator zwp_primary_selection_device_manager_v1*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the Compositor got created by
     * Registry::createPrimarySelectionDeviceManager
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * @short PrimarySelectionDevice allows clients to share data by copy-and-paste and drag-and-drop.
 *
 * This class is a convenient wrapper for the zwp_primary_selection_device_v1 interface.
 * To create a PrimarySelectionDevice call PrimarySelectionDeviceManager::getDevice.
 *
 * @see PrimarySelectionDeviceManager
 **/
class WRAPLANDCLIENT_EXPORT PrimarySelectionDevice : public QObject
{
    Q_OBJECT
public:
    using source_t = Wrapland::Client::PrimarySelectionSource;
    using offer_type = Wrapland::Client::PrimarySelectionOffer;

    explicit PrimarySelectionDevice(QObject* parent = nullptr);
    ~PrimarySelectionDevice() override;

    /**
     * Setup this PrimarySelectionDevice to manage the @p dataDevice.
     * When using PrimarySelectionDeviceManager::createPrimarySelectionDevice there is no need to
     *call this method.
     **/
    void setup(zwp_primary_selection_device_v1* dataDevice);
    /**
     * Releases the zwp_primary_selection_device_v1 interface.
     * After the interface has been released the PrimarySelectionDevice instance is no
     * longer valid and can be setup with another zwp_primary_selection_device_v1 interface.
     **/
    void release();

    /**
     * @returns @c true if managing a zwp_primary_selection_device_v1.
     **/
    bool isValid() const;

    void startDrag(quint32 serial, source_t* source, Surface* origin, Surface* icon = nullptr);
    void startDragInternally(quint32 serial, Surface* origin, Surface* icon = nullptr);

    void setSelection(quint32 serial, source_t* source = nullptr);
    void clearSelection(quint32 serial);

    PrimarySelectionOffer* offeredSelection() const;

    operator zwp_primary_selection_device_v1*();
    operator zwp_primary_selection_device_v1*() const;

Q_SIGNALS:
    void selectionOffered(Wrapland::Client::PrimarySelectionOffer*);
    void selectionCleared();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the zwp_primary_selection_offer_v1 interface.
 *
 * This class is a convenient wrapper for the zwp_primary_selection_offer_v1 interface.
 * The PrimarySelectionOffer gets created by PrimarySelectionDevice.
 *
 * @see PrimarySelectionOfferManager
 **/
class WRAPLANDCLIENT_EXPORT PrimarySelectionOffer : public QObject
{
    Q_OBJECT
public:
    explicit PrimarySelectionOffer(PrimarySelectionDevice* parent,
                                   zwp_primary_selection_offer_v1* dataOffer);
    ~PrimarySelectionOffer() override;

    /**
     * Releases the zwp_primary_selection_offer_v1 interface.
     * After the interface has been released the PrimarySelectionOffer instance is no
     * longer valid and can be setup with another zwp_primary_selection_offer_v1 interface.
     **/
    void release();

    /**
     * @returns @c true if managing a zwp_primary_selection_offer_v1.
     **/
    bool isValid() const;

    QList<QMimeType> offeredMimeTypes() const;

    void receive(const QMimeType& mimeType, qint32 fd);
    void receive(const QString& mimeType, qint32 fd);

    operator zwp_primary_selection_offer_v1*();
    operator zwp_primary_selection_offer_v1*() const;

Q_SIGNALS:
    void mimeTypeOffered(const QString&);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the zwp_primary_selection_source_v1 interface.
 *
 * This class is a convenient wrapper for the zwp_primary_selection_source_v1 interface.
 * To create a PrimarySelectionSource call
 *PrimarySelectionDeviceManager::createSource.
 *
 * @see PrimarySelectionDeviceManager
 **/
class WRAPLANDCLIENT_EXPORT PrimarySelectionSource : public QObject
{
    Q_OBJECT
public:
    explicit PrimarySelectionSource(QObject* parent = nullptr);
    ~PrimarySelectionSource() override;

    /**
     * Setup this PrimarySelectionSource to manage the @p dataSource.
     * When using PrimarySelectionDeviceManager::createSource there is no need to
     *call this method.
     **/
    void setup(zwp_primary_selection_source_v1* dataSource);
    /**
     * Releases the zwp_primary_selection_source_v1 interface.
     * After the interface has been released the PrimarySelectionSource instance is no
     * longer valid and can be setup with another zwp_primary_selection_source_v1 interface.
     **/
    void release();

    /**
     * @returns @c true if managing a zwp_primary_selection_source_v1.
     **/
    bool isValid() const;

    void offer(const QString& mimeType);
    void offer(const QMimeType& mimeType);

    operator zwp_primary_selection_source_v1*();
    operator zwp_primary_selection_source_v1*() const;

Q_SIGNALS:
    /**
     * Emitted when a target accepts pointer_focus or motion events. If
     * a target does not accept any of the offered types, @p mimeType is empty.
     **/
    void targetAccepts(const QString& mimeType);
    /**
     * Request for data from the client. Send the data as the
     * specified @p mimeType over the passed file descriptor @p fd, then close
     * it.
     **/
    void sendDataRequested(const QString& mimeType, qint32 fd);
    /**
     * This PrimarySelectionSource has been replaced by another PrimarySelectionSource.
     * The client should clean up and destroy this PrimarySelectionSource.
     **/
    void cancelled();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
