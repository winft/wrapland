/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct zwlr_data_control_manager_v1;
struct zwlr_data_control_device_v1;
struct zwlr_data_control_offer_v1;
struct zwlr_data_control_source_v1;
class QMimeType;

namespace Wrapland::Client
{

class EventQueue;
class data_control_source_v1;
class data_control_device_v1;
class data_control_offer_v1;
class Seat;
class Surface;

/**
 * @short Wrapper for the zwlr_data_control_manager_v1 interface.
 *
 * This class provides a convenient wrapper for the zwlr_data_control_manager_v1
 *interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the data_control_manager interface:
 * @code
 * data_control_manager *m = registry->createPrimarySelectionDeviceManager(name, version);
 * @endcode
 *
 * This creates the data_control_manager and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * data_control_manager *m = new data_control_manager;
 * m->setup(registry->bindPrimarySelectionDeviceManager(name, version));
 * @endcode
 *
 * The data_control_manager can be used as a drop-in replacement for any
 * zwlr_data_control_manager_v1 pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT data_control_manager_v1 : public QObject
{
    Q_OBJECT
public:
    using device_t = Wrapland::Client::data_control_device_v1;
    using source_t = Wrapland::Client::data_control_source_v1;

    /**
     * Creates a new data_control_manager.
     * Note: after construction the object is not yet valid and one needs
     * to call setup. In order to get a ready to use object prefer using
     * @c Registry::createPrimarySelectionDeviceManager .
     **/
    explicit data_control_manager_v1(QObject* parent = nullptr);
    ~data_control_manager_v1() override;

    /**
     * @returns @c true if managing a zwlr_data_control_manager_v1.
     **/
    bool isValid() const;
    /**
     * Setup this data_control_manager to manage the @p manager.
     * When using Registry::createPrimarySelectionDeviceManager there is no need to call this
     * method.
     **/
    void setup(zwlr_data_control_manager_v1* manager);
    /**
     * Releases the zwlr_data_control_manager_v1 interface.
     * After the interface has been released the data_control_manager instance is no
     * longer valid and can be setup with another zwlr_data_control_manager_v1 interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating a data_control_source_v1.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a data_control_source_v1.
     **/
    EventQueue* eventQueue();

    source_t* create_source(QObject* parent = nullptr);

    device_t* get_device(Seat* seat, QObject* parent = nullptr);

    operator zwlr_data_control_manager_v1*();
    operator zwlr_data_control_manager_v1*() const;

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
 * @short data_control_device_v1 allows clients to share data by copy-and-paste and drag-and-drop.
 *
 * This class is a convenient wrapper for the zwlr_data_control_device_v1 interface.
 * To create a data_control_device_v1 call data_control_manager::get_device.
 *
 * @see data_control_manager
 **/
class WRAPLANDCLIENT_EXPORT data_control_device_v1 : public QObject
{
    Q_OBJECT
public:
    using source_t = Wrapland::Client::data_control_source_v1;
    using offer_type = Wrapland::Client::data_control_offer_v1;

    explicit data_control_device_v1(QObject* parent = nullptr);
    ~data_control_device_v1() override;

    /**
     * Setup this data_control_device_v1 to manage the @p dataDevice.
     * When using data_control_manager::createPrimarySelectionDevice there is no need to
     *call this method.
     **/
    void setup(zwlr_data_control_device_v1* dataDevice);
    /**
     * Releases the zwlr_data_control_device_v1 interface.
     * After the interface has been released the data_control_device_v1 instance is no
     * longer valid and can be setup with another zwlr_data_control_device_v1 interface.
     **/
    void release();

    /**
     * @returns @c true if managing a zwlr_data_control_device_v1.
     **/
    bool isValid() const;

    void set_selection(source_t* source);
    void set_primary_selection(source_t* source);

    data_control_offer_v1* offered_selection() const;
    data_control_offer_v1* offered_primary_selection() const;

    operator zwlr_data_control_device_v1*();
    operator zwlr_data_control_device_v1*() const;

Q_SIGNALS:
    void selectionOffered(Wrapland::Client::data_control_offer_v1*);
    void primary_selection_offered(Wrapland::Client::data_control_offer_v1*);
    void finished();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the zwlr_data_control_offer_v1 interface.
 *
 * This class is a convenient wrapper for the zwlr_data_control_offer_v1 interface.
 * The data_control_offer_v1 gets created by data_control_device_v1.
 *
 * @see data_control_offer_v1Manager
 **/
class WRAPLANDCLIENT_EXPORT data_control_offer_v1 : public QObject
{
    Q_OBJECT
public:
    explicit data_control_offer_v1(data_control_device_v1* parent,
                                   zwlr_data_control_offer_v1* dataOffer);
    ~data_control_offer_v1() override;

    /**
     * Releases the zwlr_data_control_offer_v1 interface.
     * After the interface has been released the data_control_offer_v1 instance is no
     * longer valid and can be setup with another zwlr_data_control_offer_v1 interface.
     **/
    void release();

    /**
     * @returns @c true if managing a zwlr_data_control_offer_v1.
     **/
    bool isValid() const;

    QList<QMimeType> offered_mime_types() const;

    void receive(QMimeType const& mimeType, int32_t fd);
    void receive(QString const& mimeType, int32_t fd);

    operator zwlr_data_control_offer_v1*();
    operator zwlr_data_control_offer_v1*() const;

Q_SIGNALS:
    void mimeTypeOffered(QString const&);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the zwlr_data_control_source_v1 interface.
 *
 * This class is a convenient wrapper for the zwlr_data_control_source_v1 interface.
 * To create a data_control_source_v1 call
 *data_control_manager::createSource.
 *
 * @see data_control_manager
 **/
class WRAPLANDCLIENT_EXPORT data_control_source_v1 : public QObject
{
    Q_OBJECT
public:
    explicit data_control_source_v1(QObject* parent = nullptr);
    ~data_control_source_v1() override;

    /**
     * Setup this data_control_source_v1 to manage the @p dataSource.
     * When using data_control_manager::createSource there is no need to
     *call this method.
     **/
    void setup(zwlr_data_control_source_v1* dataSource);
    /**
     * Releases the zwlr_data_control_source_v1 interface.
     * After the interface has been released the data_control_source_v1 instance is no
     * longer valid and can be setup with another zwlr_data_control_source_v1 interface.
     **/
    void release();

    /**
     * @returns @c true if managing a zwlr_data_control_source_v1.
     **/
    bool isValid() const;

    void offer(QString const& mimeType);
    void offer(QMimeType const& mimeType);

    operator zwlr_data_control_source_v1*();
    operator zwlr_data_control_source_v1*() const;

Q_SIGNALS:
    /**
     * Emitted when a target accepts pointer_focus or motion events. If
     * a target does not accept any of the offered types, @p mimeType is empty.
     **/
    void targetAccepts(QString const& mimeType);
    /**
     * Request for data from the client. Send the data as the
     * specified @p mimeType over the passed file descriptor @p fd, then close
     * it.
     **/
    void sendDataRequested(QString const& mimeType, int32_t fd);
    /**
     * This data_control_source_v1 has been replaced by another data_control_source_v1.
     * The client should clean up and destroy this data_control_source_v1.
     **/
    void cancelled();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
