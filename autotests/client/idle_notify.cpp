/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/idle_notify_v1.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"

#include "../../server/display.h"
#include "../../server/idle_notify_v1.h"
#include "../../server/seat.h"

#include "../../tests/globals.h"

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class idle_notify_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testTimeout();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

    Clt::ConnectionThread* m_connection = nullptr;
    QThread* m_thread = nullptr;
    Clt::EventQueue* m_queue = nullptr;
    Clt::Seat* m_seat = nullptr;
    Clt::idle_notifier_v1* m_idle = nullptr;
};

constexpr auto socket_name{"wrapland-test-idle-notify-0"};

void idle_notify_test::init()
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
    server.globals.idle_notifier_v1
        = std::make_unique<Wrapland::Server::idle_notifier_v1>(server.display.get());

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Clt::EventQueue(this);
    m_queue->setup(m_connection);

    Clt::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Clt::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_seat = registry.createSeat(registry.interface(Clt::Registry::Interface::Seat).name,
                                 registry.interface(Clt::Registry::Interface::Seat).version,
                                 this);
    QVERIFY(m_seat->isValid());
    m_idle = registry.createIdleNotifierV1(
        registry.interface(Clt::Registry::Interface::IdleNotifierV1).name,
        registry.interface(Clt::Registry::Interface::IdleNotifierV1).version,
        this);
    QVERIFY(m_idle->isValid());
}

void idle_notify_test::cleanup()
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

void idle_notify_test::testTimeout()
{
    // this test verifies the basic functionality of a timeout
    QSignalSpy timeout_spy(server.globals.idle_notifier_v1.get(),
                           &Srv::idle_notifier_v1::notification_created);
    QVERIFY(timeout_spy.isValid());

    std::unique_ptr<Clt::idle_notification_v1> timeout(m_idle->get_notification(1, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.get(), &Clt::idle_notification_v1::idled);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.get(), &Clt::idle_notification_v1::resumed);
    QVERIFY(resumedFormIdleSpy.isValid());

    QVERIFY(timeout_spy.wait());

    auto idle_timeout = timeout_spy.front().front().value<Srv::idle_notification_v1*>();
    QCOMPARE(idle_timeout->duration(), std::chrono::milliseconds(1));
    QCOMPARE(idle_timeout->seat(), server.globals.seats.at(0).get());

    idle_timeout->idle();
    QVERIFY(idleSpy.wait());

    idle_timeout->resume();
    QVERIFY(resumedFormIdleSpy.wait());
}

QTEST_GUILESS_MAIN(idle_notify_test)
#include "idle_notify.moc"
