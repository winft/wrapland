/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct ext_idle_notifier_v1;
struct ext_idle_notification_v1;

namespace Wrapland::Client
{

class EventQueue;
class idle_notification_v1;
class Seat;

/**
 * @short Wrapper for the ext_idle_notifier_v1 interface.
 *
 * With the help of idle_notifier_v1 it is possible to get notified when a Seat is not being
 * used. E.g. a chat application which wants to set the user automatically to away
 * if the user did not interact with the Seat for 5 minutes can create an idle_notification_v1
 * to get notified when the Seat has been idle for the given amount of time.
 *
 * This class provides a convenient wrapper for the ext_idle_notifier_v1 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the idle_notifier_v1 interface:
 * @code
 * idle_notifier_v1* m = registry->createIdle(name, version);
 * @endcode
 *
 * This creates the idle_notifier_v1 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * auto m = new idle_notifier_v1;
 * m->setup(registry->bindIdle(name, version));
 * @endcode
 *
 * The idle_notifier_v1 can be used as a drop-in replacement for any ext_idle_notifier_v1
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT idle_notifier_v1 : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new idle_notifier_v1.
     * Note: after constructing the idle_notifier_v1 it is not yet valid and one needs
     * to call setup. In order to get a ready to use idle_notifier_v1 prefer using
     * Registry::createIdleNotifierV1.
     **/
    explicit idle_notifier_v1(QObject* parent = nullptr);
    virtual ~idle_notifier_v1();

    /**
     * @returns @c true if managing a ext_idle_notifier_v1.
     **/
    bool isValid() const;
    /**
     * Setup this idle_notifier_v1 to manage the @p manager.
     * When using Registry::createIdleNotifierV1 there is no need to call this
     * method.
     **/
    void setup(ext_idle_notifier_v1* manager);
    /**
     * Releases the ext_idle_notifier_v1 interface.
     * After the interface has been released the idle_notifier_v1 instance is no
     * longer valid and can be setup with another ext_idle_notifier_v1 interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating a idle_notification_v1.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a idle_notification_v1.
     **/
    EventQueue* eventQueue();

    /**
     * Creates a new idle_notification_v1 for the @p seat. If the @p seat has been idle,
     * that is none of the connected input devices got used for @p msec, the
     * idle_notification_v1 will emit the {@link idle_notification_v1::idled} signal.
     *
     * It is not guaranteed that the signal will be emitted exactly at the given
     * timeout. A Wayland server might for example have a minimum timeout which is
     * larger than @p msec.
     *
     * @param msec The duration in milliseconds after which an idle timeout should fire
     * @param seat The Seat on which the user activity should be monitored.
     **/
    idle_notification_v1* get_notification(quint32 msecs, Seat* seat, QObject* parent = nullptr);

    operator ext_idle_notifier_v1*();
    operator ext_idle_notifier_v1*() const;

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
 * @short Wrapper for the ext_idle_notification_v1 interface.
 *
 * This class is a convenient wrapper for the ext_idle_notification_v1 interface.
 * To create a idle_notification_v1 call idle_notifier_v1::get_notification.
 *
 * @see idle_notifier_v1
 **/
class WRAPLANDCLIENT_EXPORT idle_notification_v1 : public QObject
{
    Q_OBJECT
public:
    /**
     * To create an idle_notification_v1 prefer using {@link idle_notifier_v1::get_notification}
     * which sets up the idle_notification_v1 to be fully functional.
     **/
    explicit idle_notification_v1(QObject* parent = nullptr);
    virtual ~idle_notification_v1();

    /**
     * Setup this idle_notification_v1 to manage the @p timeout.
     * When using idle_notifier_v1::create_notification there is no need to call this
     * method.
     **/
    void setup(ext_idle_notification_v1* timeout);
    /**
     * Releases the ext_idle_notification_v1 interface.
     * After the interface has been released the idle_notification_v1 instance is no
     * longer valid and can be setup with another ext_idle_notification_v1 interface.
     **/
    void release();
    /**
     * Destroys the data held by this idle_notification_v1.
     * This method is supposed to be used when the connection to the Wayland
     * server goes away. If the connection is not valid anymore, it's not
     * possible to call release anymore as that calls into the Wayland
     * connection and the call would fail. This method cleans up the data, so
     * that the instance can be deleted or set up to a new ext_idle_notification_v1 interface
     * once there is a new connection available.
     *
     * It is suggested to connect this method to ConnectionThread::connectionDied:
     * @code
     * connect(connection, &ConnectionThread::connectionDied, source,
     * &idle_notification_v1::destroy);
     * @endcode
     *
     * @see release
     **/
    void destroy();
    /**
     * @returns @c true if managing a ext_idle_notification_v1.
     **/
    bool isValid() const;

    operator ext_idle_notification_v1*();
    operator ext_idle_notification_v1*() const;

Q_SIGNALS:
    /**
     * Emitted when this idle_notification_v1 triggered. This means the system has been idle for
     * the duration specified when creating the idle_notification_v1.
     * @see idle_notifier_v1::get_notification.
     * @see resumed
     **/
    void idled();
    /**
     * Emitted when the system shows activity again after the idle state was reached.
     * @see idled
     **/
    void resumed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
