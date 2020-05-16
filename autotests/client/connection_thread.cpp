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
#include <QtTest>

#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"

#include "../../server/client.h"
#include "../../server/display.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

#include <errno.h>

namespace Cnt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class TestWaylandConnectionThread : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandConnectionThread(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testInitConnectionNoThread();
    void testConnectionFailure();
    void testConnectionDying();
    void testConnectionThread();
    void testConnectFd();
    void testConnectFdNoSocketName();

private:
    Srv::Display *m_display;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-connection-0");

TestWaylandConnectionThread::TestWaylandConnectionThread(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
{
}

void TestWaylandConnectionThread::init()
{
    m_display = new Srv::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->running());
    m_display->createShm();
}

void TestWaylandConnectionThread::cleanup()
{
    delete m_display;
    m_display = nullptr;
}

void TestWaylandConnectionThread::testInitConnectionNoThread()
{
    QVERIFY(Cnt::ConnectionThread::connections().isEmpty());
    std::unique_ptr<Cnt::ConnectionThread> connection(new Cnt::ConnectionThread);
    QVERIFY(Cnt::ConnectionThread::connections().contains(connection.get()));

    QCOMPARE(connection->socketName(), QStringLiteral("wayland-0"));
    connection->setSocketName(s_socketName);
    QCOMPARE(connection->socketName(), s_socketName);

    QSignalSpy connectedSpy(connection.get(), &Cnt::ConnectionThread::establishedChanged);
    QSignalSpy failedSpy(connection.get(), &Cnt::ConnectionThread::failed);
    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(connection->display());

    connection.reset();
    QVERIFY(Cnt::ConnectionThread::connections().isEmpty());
}

void TestWaylandConnectionThread::testConnectionFailure()
{
    std::unique_ptr<Cnt::ConnectionThread> connection(new Cnt::ConnectionThread);
    connection->setSocketName(QStringLiteral("kwin-test-socket-does-not-exist"));

    QSignalSpy connectedSpy(connection.get(), &Cnt::ConnectionThread::establishedChanged);
    QSignalSpy failedSpy(connection.get(), &Cnt::ConnectionThread::failed);
    connection->establishConnection();
    QVERIFY(failedSpy.wait());
    QCOMPARE(connectedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(!connection->display());
}

static void registryHandleGlobal(void *data, struct wl_registry *registry,
                                 uint32_t name, const char *interface, uint32_t version)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
    Q_UNUSED(interface)
    Q_UNUSED(version)
}

static void registryHandleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
}

static const struct wl_registry_listener s_registryListener = {
    registryHandleGlobal,
    registryHandleGlobalRemove
};

void TestWaylandConnectionThread::testConnectionThread()
{
    auto *connection = new Cnt::ConnectionThread;
    connection->setSocketName(s_socketName);

    QThread *connectionThread = new QThread(this);
    connection->moveToThread(connectionThread);
    connectionThread->start();

    QSignalSpy connectedSpy(connection, &Cnt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    QSignalSpy failedSpy(connection, &Cnt::ConnectionThread::failed);
    QVERIFY(failedSpy.isValid());
    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(connection->display());

    // Now we have the connection ready. let's get some events.
    QSignalSpy eventsSpy(connection, &Cnt::ConnectionThread::eventsRead);
    QVERIFY(eventsSpy.isValid());

    wl_display *display = connection->display();
    std::unique_ptr<Cnt::EventQueue> queue(new Cnt::EventQueue);
    queue->setup(display);
    QVERIFY(queue->isValid());

    connect(connection, &Cnt::ConnectionThread::eventsRead,
            queue.get(), &Cnt::EventQueue::dispatch,
            Qt::QueuedConnection);

    wl_registry *registry = wl_display_get_registry(display);
    wl_proxy_set_queue((wl_proxy*)registry, *(queue.get()));

    wl_registry_add_listener(registry, &s_registryListener, this);
    wl_display_flush(display);

    if (eventsSpy.isEmpty()) {
        QVERIFY(eventsSpy.wait());
    }
    QVERIFY(!eventsSpy.isEmpty());

    wl_registry_destroy(registry);
    queue.reset();

    connection->deleteLater();
    connectionThread->quit();
    connectionThread->wait();
    delete connectionThread;
}

void TestWaylandConnectionThread::testConnectionDying()
{
    std::unique_ptr<Cnt::ConnectionThread> connection(new Cnt::ConnectionThread);

    QSignalSpy connectedSpy(connection.get(), &Cnt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());

    connection->setSocketName(s_socketName);
    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);
    QVERIFY(connection->display());

    m_display->terminate();
    QVERIFY(!m_display->running());

    QVERIFY(connectedSpy.count() == 2 || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 2);
    QEXPECT_FAIL("", "Looking at Wayland library a clean exit should have EPIPE.", Continue);
    QCOMPARE(connection->error(), EPIPE);
    QCOMPARE(connection->error(), ECONNRESET);
    QVERIFY(!connection->protocolError());

    connectedSpy.clear();
    QVERIFY(connectedSpy.isEmpty());

    // Restart the server.
    m_display->start();
    QVERIFY(m_display->running());

    // Try to reuse the connection thread instance.
    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);
}

void TestWaylandConnectionThread::testConnectFd()
{
    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) >= 0);
    auto c = m_display->createClient(sv[0]);
    QVERIFY(c);
    QSignalSpy disconnectedSpy(c, &Srv::Client::disconnected);
    QVERIFY(disconnectedSpy.isValid());

    auto *connection = new Cnt::ConnectionThread;
    QSignalSpy connectedSpy(connection, &Cnt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    connection->setSocketFd(sv[1]);

    QThread *connectionThread = new QThread(this);
    connection->moveToThread(connectionThread);
    connectionThread->start();
    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    // Create the Registry.
    std::unique_ptr<Cnt::Registry> registry(new Cnt::Registry);
    QSignalSpy announcedSpy(registry.get(), &Cnt::Registry::interfacesAnnounced);
    QVERIFY(announcedSpy.isValid());
    registry->create(connection);
    std::unique_ptr<Cnt::EventQueue> queue(new Cnt::EventQueue);
    queue->setup(connection);
    registry->setEventQueue(queue.get());
    registry->setup();
    QVERIFY(announcedSpy.wait());

    registry.reset();
    queue.reset();
    connection->deleteLater();
    connectionThread->quit();
    connectionThread->wait();
    delete connectionThread;

    c->destroy();
    QCOMPARE(disconnectedSpy.count(), 1);
}

void TestWaylandConnectionThread::testConnectFdNoSocketName()
{
    delete m_display;
    m_display = nullptr;

    Srv::Display display;
    display.start(Srv::Display::StartMode::ConnectClientsOnly);
    QVERIFY(display.running());

    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) >= 0);
    QVERIFY(display.createClient(sv[0]));

    auto *connection = new Cnt::ConnectionThread;
    QSignalSpy connectedSpy(connection, &Cnt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    connection->setSocketFd(sv[1]);

    QThread *connectionThread = new QThread(this);
    connection->moveToThread(connectionThread);
    connectionThread->start();
    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    // Create the Registry.
    std::unique_ptr<Cnt::Registry> registry(new Cnt::Registry);
    QSignalSpy announcedSpy(registry.get(), &Cnt::Registry::interfacesAnnounced);
    QVERIFY(announcedSpy.isValid());
    registry->create(connection);
    std::unique_ptr<Cnt::EventQueue> queue(new Cnt::EventQueue);
    queue->setup(connection);
    registry->setEventQueue(queue.get());
    registry->setup();
    QVERIFY(announcedSpy.wait());

    registry.reset();
    queue.reset();
    connection->deleteLater();
    connectionThread->quit();
    connectionThread->wait();
    delete connectionThread;
}

QTEST_GUILESS_MAIN(TestWaylandConnectionThread)
#include "connection_thread.moc"
