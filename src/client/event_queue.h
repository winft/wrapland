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
#ifndef WAYLAND_EVENT_QUEUE_H
#define WAYLAND_EVENT_QUEUE_H

#include <QObject>
// STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct wl_display;
struct wl_proxy;
struct wl_event_queue;

namespace Wrapland
{
namespace Client
{

class ConnectionThread;

/**
 * @short Wrapper class for wl_event_queue interface.
 *
 * The EventQueue is needed if a different thread is used for the connection.
 * If the interface wrappers are held in a different thread than the connection thread
 * an EventQueue is needed for the thread which holds the interface wrappers. A common
 * example is a dedicated connection thread while the interface wrappers are created
 * in the main thread.
 *
 * All interface wrappers are set up to support the EventQueue in the most convenient
 * way. The EventQueue needs only to be passed to the Registry. The EventQueue will then
 * be passed to all created wrappers through the tree.
 *
 * @code
 * ConnectionThread connection;
 * EventQueue queue;
 * Registry registry;
 *
 * connect(&connection, &ConnectionThread::connected, this, [&] {
 *     queue.setup(&connection);
 *     registry.setEventQueue(&queue);
 *     registry.setup(&connection);
 *     registry.create();
 * });
 *
 * connection.initConnection();
 * @endcode
 *
 * The EventQueue can be used as a drop-in replacement for any wl_event_queue
 * pointer as it provides matching cast operators.
 **/
class WRAPLANDCLIENT_EXPORT EventQueue : public QObject
{
    Q_OBJECT
public:
    explicit EventQueue(QObject* parent = nullptr);
    virtual ~EventQueue();

    /**
     * Creates the event queue for the @p display.
     *
     * Note: this will not automatically setup the dispatcher.
     * When using this method one needs to ensure that dispatch
     * gets invoked whenever new events need to be dispatched.
     * @see dispatch
     **/
    void setup(wl_display* display);
    /**
     * Creates the event queue for the @p connection.
     *
     * This method also connects the eventsRead signal of the ConnectionThread
     * to the dispatch method. Events will be automatically dispatched without
     * the need to call dispatch manually.
     * @see dispatch
     **/
    void setup(ConnectionThread* connection);

    /**
     * @returns @c true if EventQueue is setup.
     **/
    bool isValid();
    /**
     * Releases the wl_event_queue interface.
     * After the interface has been released the EventQueue instance is no
     * longer valid and can be setup with another wl_event_queue interface.
     **/
    void release();

    /**
     * Adds the @p proxy to the EventQueue.
     **/
    void addProxy(wl_proxy* proxy);
    /**
     * Adds the @p proxy of type wl_interface (e.g. wl_compositor) to the EventQueue.
     **/
    template<typename wl_interface>
    void addProxy(wl_interface* proxy);
    /**
     * Adds the @p proxy wrapper class of type T referencing the wl_interface to the EventQueue.
     **/
    template<typename wl_interface, typename T>
    void addProxy(T* proxy);

    operator wl_event_queue*();
    operator wl_event_queue*() const;

public Q_SLOTS:
    /**
     * Dispatches all pending events on the EventQueue.
     **/
    void dispatch();

private:
    class Private;
    std::unique_ptr<Private> d;
};

template<typename wl_interface>
inline void EventQueue::addProxy(wl_interface* proxy)
{
    addProxy(reinterpret_cast<wl_proxy*>(proxy));
}

template<typename wl_interface, typename T>
inline void EventQueue::addProxy(T* proxy)
{
    addProxy(reinterpret_cast<wl_proxy*>((wl_interface*)*(proxy)));
}

}
}

#endif
