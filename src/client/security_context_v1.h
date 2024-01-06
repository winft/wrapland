/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct wp_security_context_manager_v1;
struct wp_security_context_v1;

namespace Wrapland::Client
{

class EventQueue;
class security_context_v1;

/**
 * @short Wrapper for the wp_security_context_manager_v1 interface.
 *
 * With the help of security_context_manager_v1 it is possible to get notified when a Seat is not
 *being used. E.g. a chat application which wants to set the user automatically to away if the user
 *did not interact with the Seat for 5 minutes can create an security_context_v1 to get notified
 *when the Seat has been idle for the given amount of time.
 *
 * This class provides a convenient wrapper for the wp_security_context_manager_v1 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the security_context_manager_v1 interface:
 * @code
 * security_context_manager_v1* m = registry->createIdle(name, version);
 * @endcode
 *
 * This creates the security_context_manager_v1 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * auto m = new security_context_manager_v1;
 * m->setup(registry->bindIdle(name, version));
 * @endcode
 *
 * The security_context_manager_v1 can be used as a drop-in replacement for any
 *wp_security_context_manager_v1 pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT security_context_manager_v1 : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new security_context_manager_v1.
     * Note: after constructing the security_context_manager_v1 it is not yet valid and one needs
     * to call setup. In order to get a ready to use security_context_manager_v1 prefer using
     * Registry::createIdleNotifierV1.
     **/
    explicit security_context_manager_v1(QObject* parent = nullptr);
    virtual ~security_context_manager_v1();

    /**
     * @returns @c true if managing a wp_security_context_manager_v1.
     **/
    bool isValid() const;
    /**
     * Setup this security_context_manager_v1 to manage the @p manager.
     * When using Registry::createIdleNotifierV1 there is no need to call this
     * method.
     **/
    void setup(wp_security_context_manager_v1* manager);
    /**
     * Releases the wp_security_context_manager_v1 interface.
     * After the interface has been released the security_context_manager_v1 instance is no
     * longer valid and can be setup with another wp_security_context_manager_v1 interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating a security_context_v1.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a security_context_v1.
     **/
    EventQueue* eventQueue();

    /**
     * Creates a new security_context_v1 for the @p listen_fd and @p close_fd.
     **/
    security_context_v1*
    create_listener(int32_t listen_fd, int32_t close_fd, QObject* parent = nullptr);

    operator wp_security_context_manager_v1*();
    operator wp_security_context_manager_v1*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the Compositor got created by
     * Registry::createIdle
     *
     * @since 5.5
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @short Wrapper for the wp_security_context_v1 interface.
 *
 * This class is a convenient wrapper for the wp_security_context_v1 interface.
 * To create a security_context_v1 call security_context_manager_v1::get_notification.
 *
 * @see security_context_manager_v1
 **/
class WRAPLANDCLIENT_EXPORT security_context_v1 : public QObject
{
    Q_OBJECT
public:
    /**
     * To create an security_context_v1 prefer using {@link
     *security_context_manager_v1::get_notification} which sets up the security_context_v1 to be
     *fully functional.
     **/
    explicit security_context_v1(QObject* parent = nullptr);
    virtual ~security_context_v1();

    /**
     * Setup this security_context_v1 to manage the @p timeout.
     * When using security_context_manager_v1::create_notification there is no need to call this
     * method.
     **/
    void setup(wp_security_context_v1* timeout);
    /**
     * Releases the wp_security_context_v1 interface.
     * After the interface has been released the security_context_v1 instance is no
     * longer valid and can be setup with another wp_security_context_v1 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a wp_security_context_v1.
     **/
    bool isValid() const;

    void set_sandbox_engine(std::string const& engine) const;
    void set_app_id(std::string const& engine) const;
    void set_instance_id(std::string const& engine) const;
    void commit() const;

    operator wp_security_context_v1*();
    operator wp_security_context_v1*() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
