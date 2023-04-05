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
#include "../../server/kde_idle.h"
#include "../../server/seat.h"

#include "../../tests/globals.h"

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
    void test_simulate_user_activity();

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
    server.seat = server.globals.seats
                      .emplace_back(std::make_unique<Wrapland::Server::Seat>(server.display.get()))
                      .get();
    server.seat->setName("seat0");
    server.globals.kde_idle = std::make_unique<Wrapland::Server::kde_idle>(server.display.get());

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
    // this test verifies the basic functionality of a timeout
    QSignalSpy timeout_spy(server.globals.kde_idle.get(), &Srv::kde_idle::timeout_created);
    QVERIFY(timeout_spy.isValid());

    std::unique_ptr<Clt::IdleTimeout> timeout(m_idle->getTimeout(1, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.get(), &Clt::IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.get(), &Clt::IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());

    QVERIFY(timeout_spy.wait());

    auto idle_timeout = timeout_spy.front().front().value<Srv::kde_idle_timeout*>();
    QCOMPARE(idle_timeout->duration(), std::chrono::milliseconds(1));
    QCOMPARE(idle_timeout->seat(), server.globals.seats.at(0).get());

    idle_timeout->idle();
    QVERIFY(idleSpy.wait());

    idle_timeout->resume();
    QVERIFY(resumedFormIdleSpy.wait());
}

void IdleTest::test_simulate_user_activity()
{
    // this test verifies the basic functionality of a timeout
    QSignalSpy timeout_spy(server.globals.kde_idle.get(), &Srv::kde_idle::timeout_created);
    QVERIFY(timeout_spy.isValid());

    std::unique_ptr<Clt::IdleTimeout> timeout(m_idle->getTimeout(1, m_seat));
    QVERIFY(timeout->isValid());

    QVERIFY(timeout_spy.wait());

    auto idle_timeout = timeout_spy.front().front().value<Srv::kde_idle_timeout*>();

    QSignalSpy simulate_spy(idle_timeout, &Srv::kde_idle_timeout::simulate_user_activity);
    QVERIFY(simulate_spy.isValid());

    timeout->simulateUserActivity();
    QVERIFY(simulate_spy.wait());
    QCOMPARE(simulate_spy.size(), 1);
}

QTEST_GUILESS_MAIN(IdleTest)
#include "kde_idle.moc"
