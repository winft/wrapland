/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/security_context_v1.h"

#include "../../server/client.h"
#include "../../server/display.h"
#include "../../server/security_context_v1.h"

#include "../../tests/globals.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class security_context_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_flatpak();

private:
    struct {
        std::unique_ptr<Srv::Display> display;
        Srv::globals globals;
    } server;

    Clt::ConnectionThread* m_connection{nullptr};
    QThread* m_thread{nullptr};
    Clt::EventQueue* m_queue{nullptr};
    Clt::security_context_manager_v1* m_context{nullptr};
};

constexpr auto socket_name{"wrapland-test-security-context-0"};

void security_context_test::init()
{
    server.display = std::make_unique<Srv::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.security_context_manager_v1
        = std::make_unique<Srv::security_context_manager_v1>(server.display.get());

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connected_spy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connected_spy.isValid());
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connected_spy.count() || connected_spy.wait());
    QCOMPARE(connected_spy.count(), 1);

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

    m_context = registry.createSecurityContextManagerV1(
        registry.interface(Clt::Registry::Interface::SecurityContextManagerV1).name,
        registry.interface(Clt::Registry::Interface::SecurityContextManagerV1).version,
        this);
    QVERIFY(m_context->isValid());
}

void security_context_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_context)
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

void security_context_test::test_flatpak()
{
    // Tests a mock flatpak server creating a Security Context connecting a client to the newly
    // created server and making sure everything is torn down after the close-fd is closed.
    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    QVERIFY(listen_fd != 0);

    QTemporaryDir temp_dir;

    sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    snprintf(sockaddr.sun_path,
             sizeof(sockaddr.sun_path),
             "%s",
             temp_dir.filePath("socket").toUtf8().constData());
    qDebug() << "listening socket:" << sockaddr.sun_path;
    QVERIFY(bind(listen_fd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == 0);
    QVERIFY(listen(listen_fd, 0) == 0);

    int sync_fds[2];
    QVERIFY(pipe(sync_fds) >= 0);
    int close_fd_to_keep = sync_fds[0];
    int close_fd_to_give = sync_fds[1];

    auto security_context = std::unique_ptr<Clt::security_context_v1>(
        m_context->create_listener(listen_fd, close_fd_to_give));
    close(close_fd_to_give);
    close(listen_fd);

    security_context->set_instance_id("kde.unitest.instance_id");
    security_context->set_app_id("kde.unittest.app_id");
    security_context->set_sandbox_engine("test_sandbox_engine");
    security_context->commit();
    security_context = {};

    qputenv("WAYLAND_DISPLAY", temp_dir.filePath("socket").toUtf8());
    QSignalSpy client_connected_spy(server.display.get(), &Srv::Display::clientConnected);

    // Secuirty context client connects
    QVERIFY(client_connected_spy.wait());
    QCOMPARE(client_connected_spy.size(), 1);

    struct restricted_client {
        restricted_client()
            : connected_spy{&connection, &Clt::ConnectionThread::establishedChanged}
        {
            connection.moveToThread(&thread);
            thread.start();
            connection.establishConnection();
        }

        ~restricted_client()
        {
            thread.quit();
            thread.wait();
        }

        Clt::ConnectionThread connection;
        QThread thread;
        QSignalSpy connected_spy;
    };

    // connect a client using the newly created listening socket
    restricted_client restricted_client1;
    QVERIFY(restricted_client1.connected_spy.wait());

    // Verify that our new restricted client is seen with the right security context
    QCOMPARE(client_connected_spy.size(), 2);
    QCOMPARE(client_connected_spy.back().front().value<Srv::Client*>()->security_context_app_id(),
             "kde.unittest.app_id");

    // Close the mock flatpak close-fd
    close(close_fd_to_keep);

    // security context properties should have not changed after close-fd is closed
    QTest::qWait(500);
    QCOMPARE(client_connected_spy.back().front().value<Srv::Client*>()->security_context_app_id(),
             "kde.unittest.app_id");

    // New clients can't connect anymore.
    restricted_client restricted_client2;

    QSignalSpy failed_spy(&restricted_client2.connection, &Clt::ConnectionThread::failed);
    client_connected_spy.clear();

    QVERIFY(restricted_client2.connected_spy.wait());
    QVERIFY(client_connected_spy.isEmpty());

    QEXPECT_FAIL("", "No fail on connect.", Continue);
    QVERIFY(!failed_spy.isEmpty());
}

QTEST_GUILESS_MAIN(security_context_test)
#include "security_context.moc"
