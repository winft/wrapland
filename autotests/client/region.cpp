/********************************************************************
Copyright © 2014 Martin Gräßlin <mgraesslin@kde.org>
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
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"

#include "../../server/client.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/region.h"

#include "../../tests/globals.h"

class TestRegion : public QObject
{
    Q_OBJECT
public:
    explicit TestRegion(QObject* parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testCreateWithRegion();
    void testCreateUniquePtr();
    void testAdd();
    void testRemove();
    void testDestroy();
    void testDisconnect();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::EventQueue* m_queue;
    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-wayland-region-0"};

TestRegion::TestRegion(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestRegion::init()
{
    qRegisterMetaType<Wrapland::Server::Region*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    // Setup connection.
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Wrapland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Wrapland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32, quint32)));
    QVERIFY(compositorSpy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);
}

void TestRegion::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    server = {};
}

void TestRegion::testCreate()
{
    QSignalSpy regionCreatedSpy(server.globals.compositor.get(),
                                SIGNAL(regionCreated(Wrapland::Server::Region*)));
    QVERIFY(regionCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Region> region(m_compositor->createRegion());
    QCOMPARE(region->region(), QRegion());

    QVERIFY(regionCreatedSpy.wait());
    QCOMPARE(regionCreatedSpy.count(), 1);
    auto serverRegion = regionCreatedSpy.first().first().value<Wrapland::Server::Region*>();
    QVERIFY(serverRegion);
    QCOMPARE(serverRegion->region(), QRegion());
}

void TestRegion::testCreateWithRegion()
{
    QSignalSpy regionCreatedSpy(server.globals.compositor.get(),
                                &Wrapland::Server::Compositor::regionCreated);
    QVERIFY(regionCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Region> region(
        m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));
    QCOMPARE(region->region(), QRegion(0, 0, 10, 20));

    QVERIFY(regionCreatedSpy.wait());
    QCOMPARE(regionCreatedSpy.count(), 1);
    auto serverRegion = regionCreatedSpy.first().first().value<Wrapland::Server::Region*>();
    QVERIFY(serverRegion);
    QCOMPARE(serverRegion->region(), QRegion(0, 0, 10, 20));
}

void TestRegion::testCreateUniquePtr()
{
    QSignalSpy regionCreatedSpy(server.globals.compositor.get(),
                                SIGNAL(regionCreated(Wrapland::Server::Region*)));
    QVERIFY(regionCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Region> region(
        m_compositor->createRegion(QRegion(0, 0, 10, 20)));
    QCOMPARE(region->region(), QRegion(0, 0, 10, 20));

    QVERIFY(regionCreatedSpy.wait());
    QCOMPARE(regionCreatedSpy.count(), 1);
    auto serverRegion = regionCreatedSpy.first().first().value<Wrapland::Server::Region*>();
    QVERIFY(serverRegion);
    QCOMPARE(serverRegion->region(), QRegion(0, 0, 10, 20));
}

void TestRegion::testAdd()
{
    QSignalSpy regionCreatedSpy(server.globals.compositor.get(),
                                SIGNAL(regionCreated(Wrapland::Server::Region*)));
    QVERIFY(regionCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Region> region(m_compositor->createRegion());
    QVERIFY(regionCreatedSpy.wait());
    auto serverRegion = regionCreatedSpy.first().first().value<Wrapland::Server::Region*>();

    QSignalSpy regionChangedSpy(serverRegion, SIGNAL(regionChanged(QRegion)));
    QVERIFY(regionChangedSpy.isValid());

    // Adding a QRect.
    region->add(QRect(0, 0, 10, 20));
    QCOMPARE(region->region(), QRegion(0, 0, 10, 20));

    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(regionChangedSpy.count(), 1);
    QCOMPARE(regionChangedSpy.last().first().value<QRegion>(), QRegion(0, 0, 10, 20));
    QCOMPARE(serverRegion->region(), QRegion(0, 0, 10, 20));

    // Adding a QRegion.
    region->add(QRegion(5, 5, 10, 50));
    QRegion compareRegion(0, 0, 10, 20);
    compareRegion = compareRegion.united(QRect(5, 5, 10, 50));
    QCOMPARE(region->region(), compareRegion);

    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(regionChangedSpy.count(), 2);
    QCOMPARE(regionChangedSpy.last().first().value<QRegion>(), compareRegion);
    QCOMPARE(serverRegion->region(), compareRegion);
}

void TestRegion::testRemove()
{
    QSignalSpy regionCreatedSpy(server.globals.compositor.get(),
                                SIGNAL(regionCreated(Wrapland::Server::Region*)));
    QVERIFY(regionCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Region> region(
        m_compositor->createRegion(QRegion(0, 0, 100, 200)));
    QVERIFY(regionCreatedSpy.wait());
    auto serverRegion = regionCreatedSpy.first().first().value<Wrapland::Server::Region*>();

    QSignalSpy regionChangedSpy(serverRegion, &Wrapland::Server::Region::regionChanged);
    QVERIFY(regionChangedSpy.isValid());

    // Subtract a QRect.
    region->subtract(QRect(0, 0, 10, 20));
    QRegion compareRegion(0, 0, 100, 200);
    compareRegion = compareRegion.subtracted(QRect(0, 0, 10, 20));
    QCOMPARE(region->region(), compareRegion);

    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(regionChangedSpy.count(), 1);
    QCOMPARE(regionChangedSpy.last().first().value<QRegion>(), compareRegion);
    QCOMPARE(serverRegion->region(), compareRegion);

    // Subtracting a QRegion.
    region->subtract(QRegion(5, 5, 10, 50));
    compareRegion = compareRegion.subtracted(QRect(5, 5, 10, 50));
    QCOMPARE(region->region(), compareRegion);

    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(regionChangedSpy.count(), 2);
    QCOMPARE(regionChangedSpy.last().first().value<QRegion>(), compareRegion);
    QCOMPARE(serverRegion->region(), compareRegion);
}

void TestRegion::testDestroy()
{
    std::unique_ptr<Wrapland::Client::Region> region(m_compositor->createRegion());

    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            region.get(),
            &Wrapland::Client::Region::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_compositor,
            &Wrapland::Client::Compositor::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_queue,
            &Wrapland::Client::EventQueue::release);
    QVERIFY(region->isValid());

    server = {};
    QTRY_VERIFY(!m_connection->established());

    // Now the region should be destroyed.
    QTRY_VERIFY(!region->isValid());

    // Calling release again should not fail.
    region->release();
}

void TestRegion::testDisconnect()
{
    // This test verifies that the server side correctly tears down the resources when the client
    // disconnects.
    std::unique_ptr<Wrapland::Client::Region> r(m_compositor->createRegion());
    QVERIFY(r != nullptr);
    QVERIFY(r->isValid());
    QSignalSpy regionCreatedSpy(server.globals.compositor.get(),
                                &Wrapland::Server::Compositor::regionCreated);
    QVERIFY(regionCreatedSpy.isValid());
    QVERIFY(regionCreatedSpy.wait());
    auto serverRegion = regionCreatedSpy.first().first().value<Wrapland::Server::Region*>();

    // Destroy client.
    QSignalSpy clientDisconnectedSpy(serverRegion->client(),
                                     &Wrapland::Server::Client::disconnected);
    QVERIFY(clientDisconnectedSpy.isValid());
    QSignalSpy regionDestroyedSpy(serverRegion, &QObject::destroyed);
    QVERIFY(regionDestroyedSpy.isValid());

    r->release();
    m_compositor->release();
    m_queue->release();

    QCOMPARE(regionDestroyedSpy.count(), 0);

    QVERIFY(m_connection);
    m_connection->deleteLater();
    m_connection = nullptr;

    QVERIFY(clientDisconnectedSpy.wait());
    QCOMPARE(clientDisconnectedSpy.count(), 1);
    QTRY_COMPARE(regionDestroyedSpy.count(), 1);
}

QTEST_GUILESS_MAIN(TestRegion)
#include "region.moc"
