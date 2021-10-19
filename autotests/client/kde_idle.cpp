/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
#include "../../src/client/idle.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/kde_idle.h"
#include "../../server/seat.h"

using namespace Wrapland::Client;

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class IdleTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testTimeout();
    void testSimulateUserActivity();
    void testServerSimulateUserActivity();
    void testIdleInhibit();
    void testIdleInhibitBlocksTimeout();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

    ConnectionThread* m_connection = nullptr;
    QThread* m_thread = nullptr;
    EventQueue* m_queue = nullptr;
    Clt::Seat* m_seat = nullptr;
    Idle* m_idle = nullptr;
};

constexpr auto socket_name{"wrapland-test-idle-0"};

void IdleTest::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.seat = server.globals.seats.emplace_back(server.display->createSeat()).get();
    server.seat->setName("seat0");
    server.globals.kde_idle = server.display->createIdle();

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name,
                                 registry.interface(Registry::Interface::Seat).version,
                                 this);
    QVERIFY(m_seat->isValid());
    m_idle = registry.createIdle(registry.interface(Registry::Interface::Idle).name,
                                 registry.interface(Registry::Interface::Idle).version,
                                 this);
    QVERIFY(m_idle->isValid());
}

void IdleTest::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_idle)
    CLEANUP(m_seat)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
#undef CLEANUP

    server = {};
}

void IdleTest::testTimeout()
{
    // this test verifies the basic functionality of a timeout, that it gets fired
    // and that it resumes from idle, etc.

    std::unique_ptr<Clt::IdleTimeout> timeout(m_idle->getTimeout(1, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.get(), &Clt::IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.get(), &Clt::IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());

    // we requested a timeout of 1 msec, but the minimum the server sets is 5 sec
    QVERIFY(!idleSpy.wait(1000));
    // the default of 5 sec will now pass
    QVERIFY(idleSpy.wait());

    // simulate some activity
    QVERIFY(resumedFormIdleSpy.isEmpty());
    server.seat->setTimestamp(1);
    QVERIFY(resumedFormIdleSpy.wait());

    timeout.reset();
    m_connection->flush();
    server.display->dispatchEvents();
}

void IdleTest::testSimulateUserActivity()
{
    // this test verifies that simulate user activity doesn't fire the timer
    std::unique_ptr<Clt::IdleTimeout> timeout(m_idle->getTimeout(6000, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.get(), &Clt::IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.get(), &Clt::IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());
    m_connection->flush();

    QTest::qWait(4000);
    timeout->simulateUserActivity();
    // waiting default five sec should fail
    QVERIFY(!idleSpy.wait());
    // another 2 sec should fire
    QVERIFY(idleSpy.wait(2000));

    // now simulating user activity should emit a resumedFromIdle
    QVERIFY(resumedFormIdleSpy.isEmpty());
    timeout->simulateUserActivity();
    QVERIFY(resumedFormIdleSpy.wait());

    timeout.reset();
    m_connection->flush();
    server.display->dispatchEvents();
}

void IdleTest::testServerSimulateUserActivity()
{
    // this test verifies that simulate user activity doesn't fire the timer
    std::unique_ptr<Clt::IdleTimeout> timeout(m_idle->getTimeout(6000, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.get(), &Clt::IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.get(), &Clt::IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());
    m_connection->flush();

    QTest::qWait(4000);
    server.globals.kde_idle->simulateUserActivity();
    // waiting default five sec should fail
    QVERIFY(!idleSpy.wait());
    // another 2 sec should fire
    QVERIFY(idleSpy.wait(2000));

    // now simulating user activity should emit a resumedFromIdle
    QVERIFY(resumedFormIdleSpy.isEmpty());
    server.globals.kde_idle->simulateUserActivity();
    QVERIFY(resumedFormIdleSpy.wait());

    timeout.reset();
    m_connection->flush();
    server.display->dispatchEvents();
}

void IdleTest::testIdleInhibit()
{
    QCOMPARE(server.globals.kde_idle->isInhibited(), false);
    QSignalSpy idleInhibitedSpy(server.globals.kde_idle.get(),
                                &Wrapland::Server::KdeIdle::inhibitedChanged);
    QVERIFY(idleInhibitedSpy.isValid());
    server.globals.kde_idle->inhibit();
    QCOMPARE(server.globals.kde_idle->isInhibited(), true);
    QCOMPARE(idleInhibitedSpy.count(), 1);
    server.globals.kde_idle->inhibit();
    QCOMPARE(server.globals.kde_idle->isInhibited(), true);
    QCOMPARE(idleInhibitedSpy.count(), 1);
    server.globals.kde_idle->uninhibit();
    QCOMPARE(server.globals.kde_idle->isInhibited(), true);
    QCOMPARE(idleInhibitedSpy.count(), 1);
    server.globals.kde_idle->uninhibit();
    QCOMPARE(server.globals.kde_idle->isInhibited(), false);
    QCOMPARE(idleInhibitedSpy.count(), 2);
}

void IdleTest::testIdleInhibitBlocksTimeout()
{
    // this test verifies that a timeout does not fire when the system is inhibited

    // so first inhibit
    QCOMPARE(server.globals.kde_idle->isInhibited(), false);
    server.globals.kde_idle->inhibit();

    std::unique_ptr<Clt::IdleTimeout> timeout(m_idle->getTimeout(1, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.get(), &Clt::IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.get(), &Clt::IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());

    // we requested a timeout of 1 msec, but the minimum the server sets is 5 sec
    QVERIFY(!idleSpy.wait(500));
    // the default of 5 sec won't pass
    QVERIFY(!idleSpy.wait());

    // simulate some activity
    QVERIFY(resumedFormIdleSpy.isEmpty());
    server.seat->setTimestamp(1);
    // resume from idle should not fire
    QVERIFY(!resumedFormIdleSpy.wait());

    // let's uninhibit
    server.globals.kde_idle->uninhibit();
    QCOMPARE(server.globals.kde_idle->isInhibited(), false);
    // we requested a timeout of 1 msec, but the minimum the server sets is 5 sec
    QVERIFY(!idleSpy.wait(500));
    // the default of 5 sec will now pass
    QVERIFY(idleSpy.wait());

    // if we inhibit now it will trigger a resume from idle
    QVERIFY(resumedFormIdleSpy.isEmpty());
    server.globals.kde_idle->inhibit();
    QVERIFY(resumedFormIdleSpy.wait());

    // let's wait again just to verify that also inhibit for already existing IdleTimeout works
    QVERIFY(!idleSpy.wait(500));
    QVERIFY(!idleSpy.wait());
    QCOMPARE(idleSpy.count(), 1);

    timeout.reset();
    m_connection->flush();
    server.display->dispatchEvents();
}

QTEST_GUILESS_MAIN(IdleTest)
#include "kde_idle.moc"
