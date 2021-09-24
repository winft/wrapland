/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>
#include <vector>

struct wp_drm_lease_device_v1;
struct wp_drm_lease_connector_v1;
struct wp_drm_lease_v1;

namespace Wrapland::Client
{
class drm_lease_connector_v1;
class drm_lease_v1;
class EventQueue;

/**
 * @short Wrapper for the wp_drm_lease_device_v1 interface.
 *
 * This class provides a convenient wrapper for the wp_drm_lease_device_v1 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the drm_lease_device_v1 interface:
 * @code
 * auto c = registry->createDrmLeaseDeviceV1(name, version);
 * @endcode
 *
 * This creates the drm_lease_device_v1 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * auto c = new drm_lease_device_v1;
 * c->setup(registry->bindDrmLeaseDeviceV1(name, version));
 * @endcode
 *
 * The drm_lease_device_v1 can be used as a drop-in replacement for any
 * wp_drm_lease_device_v1 pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT drm_lease_device_v1 : public QObject
{
    Q_OBJECT
public:
    explicit drm_lease_device_v1(QObject* parent = nullptr);
    ~drm_lease_device_v1() override;

    /**
     * Setup this drm_lease_device_v1 to manage the @p device.
     * When using Registry::createDrmLeaseDeviceV1 there is no need to call this
     * method.
     **/
    void setup(wp_drm_lease_device_v1* device);
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;
    /**
     * Releases the interface.
     * After the interface has been released the drm_lease_device_v1 instance is no
     * longer valid. It can't be used anymore, in particular not setup with another interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this drm_lease_device_v1.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this drm_lease_device_v1.
     **/
    EventQueue* event_queue();

    int drm_fd();

    /**
     * Request a new lease.
     **/
    drm_lease_v1* create_lease(std::vector<drm_lease_connector_v1*> const& connectors);

    /**
     * @returns @c null if not for a wp_drm_lease_device_v1
     **/
    operator wp_drm_lease_device_v1*();
    /**
     * @returns @c null if not for a wp_drm_lease_device_v1
     **/
    operator wp_drm_lease_device_v1*() const;

Q_SIGNALS:
    void connector(drm_lease_connector_v1* connector);
    void done();
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the drm_lease_device_v1 got created by
     * Registry::createDrmLeaseDeviceV1
     **/
    void removed();

private:
    friend class Registry;
    class Private;
    Private* d_ptr;
};

struct drm_lease_connector_v1_data {
    std::string name;
    std::string description;
    uint32_t id{0};
    bool enabled{false};
};

/**
 * @short Wrapper for the wp_drm_lease_connector_v1 interface.
 *
 * This class is a convenient wrapper for the wp_drm_lease_connector_v1 interface.
 * drm_lease_connector_v1 objects are advertised by the drm_lease_device_v1::connector signal.
 *
 * @see drm_lease_device_v1
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT drm_lease_connector_v1 : public QObject
{
    Q_OBJECT
public:
    ~drm_lease_connector_v1() override;

    /**
     * Setup this drm_lease_connector_v1 to manage the @p connector.
     * When the drm_lease_connector_v1 was received through drm_lease_device_v1::connector there is
     * no need to call this method.
     **/
    void setup(wp_drm_lease_connector_v1* connector);
    /**
     * Releases the wp_drm_lease_connector_v1 interface.
     * After the interface has been released the drm_lease_connector_v1 instance is no
     * longer valid and can be setup with another wp_drm_lease_connector_v1 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;

    operator wp_drm_lease_connector_v1*();
    operator wp_drm_lease_connector_v1*() const;

    void setEventQueue(EventQueue* queue);
    EventQueue* event_queue() const;

    drm_lease_connector_v1_data const& data() const;

Q_SIGNALS:
    void done();

private:
    drm_lease_connector_v1();

    friend class drm_lease_device_v1;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the wp_drm_lease_v1 interface.
 *
 * This class is a convenient wrapper for the wp_drm_lease_v1 interface.
 * To create a drm_lease_v1 call drm_lease_device_v1::create_lease.
 *
 * @see drm_lease_device_v1
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT drm_lease_v1 : public QObject
{
    Q_OBJECT
public:
    ~drm_lease_v1() override;

    /**
     * Setup this drm_lease_v1 to manage the @p lease.
     * When using drm_lease_device_v1::create_lease there is no need to call
     * this method.
     **/
    void setup(wp_drm_lease_v1* lease);
    /**
     * Releases the wp_drm_lease_v1 interface.
     * After the interface has been released the drm_lease_v1 instance is no
     * longer valid and can be setup with another wp_drm_lease_v1 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;

    operator wp_drm_lease_v1*();
    operator wp_drm_lease_v1*() const;

    void setEventQueue(EventQueue* queue);
    EventQueue* event_queue() const;

Q_SIGNALS:
    void leased_fd(int fd);
    void finished();

private:
    drm_lease_v1();

    friend class drm_lease_device_v1;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
