/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/connection_thread.h"
#include "../../src/client/drm_lease_v1.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"

#include "../../server/display.h"
#include "../../server/drm_lease_v1.h"
#include "../../server/globals.h"

#include <deque>

class drm_lease_v1_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_device_bind();
    void test_connectors();

    void test_lease_data();
    void test_lease();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::drm_lease_device_v1* lease_device{nullptr};
    } server;

    struct client {
        Wrapland::Client::ConnectionThread* connection{nullptr};
        Wrapland::Client::EventQueue* queue{nullptr};
        Wrapland::Client::Registry* registry{nullptr};
        Wrapland::Client::drm_lease_device_v1* lease_device{nullptr};
        QThread* thread{nullptr};
    } client1, client2;

    void create_client(client& client_ref);
    void cleanup_client(client& client_ref);
    void create_lease_device(client& client_ref);
    void cleanup_lease_device(client& client_ref);
};

constexpr auto socket_name{"wrapland-test-drm-lease-v1-0"};

struct client_connector {
    std::unique_ptr<Wrapland::Client::drm_lease_connector_v1> client;
    QSignalSpy done_spy;

    client_connector(Wrapland::Client::drm_lease_connector_v1* con)
        : client{std::unique_ptr<Wrapland::Client::drm_lease_connector_v1>(con)}
        , done_spy{QSignalSpy(con, &Wrapland::Client::drm_lease_connector_v1::done)}
    {
        QVERIFY(done_spy.isValid());
        if (!con->data().enabled) {
            QVERIFY(done_spy.wait());
        }
        QVERIFY(con->data().enabled);
        done_spy.clear();
    }
};

void drm_lease_v1_test::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.drm_lease_device_v1 = server.display->createDrmLeaseDeviceV1();
    server.lease_device = server.globals.drm_lease_device_v1.get();

    create_client(client1);
}

void drm_lease_v1_test::cleanup()
{
    cleanup_lease_device(client1);
    cleanup_client(client1);
    cleanup_lease_device(client2);
    cleanup_client(client2);

    server = {};
}

void drm_lease_v1_test::create_client(client& client_ref)
{
    client_ref.connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy establishedSpy(client_ref.connection,
                              &Wrapland::Client::ConnectionThread::establishedChanged);
    client_ref.connection->setSocketName(socket_name);

    client_ref.thread = new QThread(this);
    client_ref.connection->moveToThread(client_ref.thread);
    client_ref.thread->start();

    client_ref.connection->establishConnection();
    QVERIFY(establishedSpy.wait());

    client_ref.queue = new Wrapland::Client::EventQueue(this);
    client_ref.queue->setup(client_ref.connection);
    QVERIFY(client_ref.queue->isValid());

    QSignalSpy client_connected_spy(server.display.get(),
                                    &Wrapland::Server::Display::clientConnected);
    QVERIFY(client_connected_spy.isValid());

    client_ref.registry = new Wrapland::Client::Registry();

    QSignalSpy lease_device_spy(client_ref.registry,
                                &Wrapland::Client::Registry::drmLeaseDeviceV1Announced);
    QVERIFY(lease_device_spy.isValid());
    QSignalSpy interfaces_spy(client_ref.registry,
                              &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfaces_spy.isValid());

    client_ref.registry->setEventQueue(client_ref.queue);
    client_ref.registry->create(client_ref.connection);
    QVERIFY(client_ref.registry->isValid());
    client_ref.registry->setup();

    QVERIFY(interfaces_spy.wait());
    QCOMPARE(lease_device_spy.count(), 1);
}
void drm_lease_v1_test::cleanup_client(client& client_ref)
{
    delete client_ref.queue;
    client_ref.queue = nullptr;
    delete client_ref.registry;
    client_ref.registry = nullptr;

    if (client_ref.thread) {
        client_ref.thread->quit();
        client_ref.thread->wait();
        delete client_ref.thread;
        client_ref.thread = nullptr;
    }
    delete client_ref.connection;
    client_ref.connection = nullptr;
}

void drm_lease_v1_test::create_lease_device(client& client_ref)
{
    QVERIFY(!client_ref.lease_device);

    auto Device = Wrapland::Client::Registry::Interface::DrmLeaseDeviceV1;

    client_ref.lease_device = client_ref.registry->createDrmLeaseDeviceV1(
        client_ref.registry->interface(Device).name,
        client_ref.registry->interface(Device).version,
        this);
    QVERIFY(client_ref.lease_device->isValid());
}

void drm_lease_v1_test::cleanup_lease_device(client& client_ref)
{
    delete client_ref.lease_device;
    client_ref.lease_device = nullptr;
}

void drm_lease_v1_test::test_device_bind()
{
    // This test verifies that a client can bind and unbind the device.

    QSignalSpy server_fd_spy(server.lease_device,
                             &Wrapland::Server::drm_lease_device_v1::needs_new_client_fd);
    QVERIFY(server_fd_spy.isValid());

    create_lease_device(client1);
    QVERIFY(server_fd_spy.wait());

    create_client(client2);
    create_lease_device(client2);
    QVERIFY(server_fd_spy.wait());

    cleanup_lease_device(client1);
    create_lease_device(client1);
    QVERIFY(server_fd_spy.wait());

    // Destroy the client without the device first.
    // TODO(romangg): At the moment we can't test this since the Client library requires all class
    //                instances to be cleaned up before the connection can go down.
#if 0
    cleanup_client(client1);
#endif

    cleanup_lease_device(client2);
    create_lease_device(client2);
    QVERIFY(server_fd_spy.wait());
}

void drm_lease_v1_test::test_connectors()
{
    // This test verifies that connectors are advertised correctly.

    std::vector<std::unique_ptr<Wrapland::Server::output>> server_outputs;
    auto add_output = [&server_outputs, this] {
        server_outputs.emplace_back(
            std::make_unique<Wrapland::Server::output>(server.display.get()));
    };

    add_output();
    add_output();
    add_output();

    std::deque<std::unique_ptr<Wrapland::Server::drm_lease_connector_v1>> server_connectors;
    auto add_connector = [&](auto& server_output) {
        server_connectors.emplace_back(std::unique_ptr<Wrapland::Server::drm_lease_connector_v1>(
            server.lease_device->create_connector(server_output.get())));
    };

    add_connector(server_outputs.at(0));
    add_connector(server_outputs.at(1));

    QSignalSpy server_fd_spy(server.lease_device,
                             &Wrapland::Server::drm_lease_device_v1::needs_new_client_fd);
    QVERIFY(server_fd_spy.isValid());

    client1.lease_device = client1.registry->createDrmLeaseDeviceV1(
        client1.registry->interface(Wrapland::Client::Registry::Interface::DrmLeaseDeviceV1).name,
        client1.registry->interface(Wrapland::Client::Registry::Interface::DrmLeaseDeviceV1)
            .version,
        this);
    QVERIFY(client1.lease_device->isValid());

    QSignalSpy connector_spy(client1.lease_device,
                             &Wrapland::Client::drm_lease_device_v1::connector);
    QVERIFY(connector_spy.isValid());
    QSignalSpy done_spy(client1.lease_device, &Wrapland::Client::drm_lease_device_v1::done);
    QVERIFY(done_spy.isValid());

    QVERIFY(server_fd_spy.wait());

    QTemporaryFile drm_fd_file;
    QVERIFY(drm_fd_file.open());
    server.lease_device->update_fd(drm_fd_file.handle());

    QVERIFY(done_spy.wait());
    QCOMPARE(done_spy.size(), 1);
    QCOMPARE(connector_spy.size(), 2);

    QVERIFY(client1.lease_device->drm_fd());

    auto client_connector1 = client_connector(
        connector_spy[0].first().value<Wrapland::Client::drm_lease_connector_v1*>());
    auto client_connector2 = client_connector(
        connector_spy[1].first().value<Wrapland::Client::drm_lease_connector_v1*>());

    // Add another connector at run time.
    add_connector(server_outputs.at(2));
    QVERIFY(done_spy.wait());
    QCOMPARE(done_spy.size(), 2);
    QCOMPARE(connector_spy.size(), 3);
    auto client_connector3 = client_connector(
        connector_spy[2].first().value<Wrapland::Client::drm_lease_connector_v1*>());

    // Now remove one connector again.
    server_connectors.pop_front();
    QVERIFY(client_connector1.done_spy.wait());
    QCOMPARE(client_connector1.done_spy.size(), 1);
    QVERIFY(!client_connector1.client->data().enabled);
}

void drm_lease_v1_test::test_lease_data()
{
    QTest::addColumn<bool>("server_sends_finish");

    QTest::newRow("server-finish") << true;
    QTest::newRow("client-finish") << false;
}

void drm_lease_v1_test::test_lease()
{
    // This test verifies that leases are communicated correctly.

    std::vector<std::unique_ptr<Wrapland::Server::output>> server_outputs;
    auto add_output = [&server_outputs, this] {
        server_outputs.emplace_back(
            std::make_unique<Wrapland::Server::output>(server.display.get()));
    };

    add_output();
    add_output();

    std::vector<std::unique_ptr<Wrapland::Server::drm_lease_connector_v1>> server_connectors;
    auto add_connector = [&](auto& server_output) {
        server_connectors.emplace_back(std::unique_ptr<Wrapland::Server::drm_lease_connector_v1>(
            server.lease_device->create_connector(server_output.get())));
    };

    add_connector(server_outputs.at(0));
    add_connector(server_outputs.at(1));

    QSignalSpy server_fd_spy(server.lease_device,
                             &Wrapland::Server::drm_lease_device_v1::needs_new_client_fd);
    QVERIFY(server_fd_spy.isValid());

    client1.lease_device = client1.registry->createDrmLeaseDeviceV1(
        client1.registry->interface(Wrapland::Client::Registry::Interface::DrmLeaseDeviceV1).name,
        client1.registry->interface(Wrapland::Client::Registry::Interface::DrmLeaseDeviceV1)
            .version,
        this);
    QVERIFY(client1.lease_device->isValid());

    QSignalSpy connector_spy(client1.lease_device,
                             &Wrapland::Client::drm_lease_device_v1::connector);
    QVERIFY(connector_spy.isValid());
    QSignalSpy done_spy(client1.lease_device, &Wrapland::Client::drm_lease_device_v1::done);
    QVERIFY(done_spy.isValid());

    QVERIFY(server_fd_spy.wait());

    QTemporaryFile drm_fd_file;
    QVERIFY(drm_fd_file.open());
    server.lease_device->update_fd(drm_fd_file.handle());

    QVERIFY(done_spy.wait());
    QCOMPARE(done_spy.size(), 1);
    QCOMPARE(connector_spy.size(), 2);

    QVERIFY(client1.lease_device->drm_fd());

    auto client_connector1 = client_connector(
        connector_spy[0].first().value<Wrapland::Client::drm_lease_connector_v1*>());
    auto client_connector2 = client_connector(
        connector_spy[1].first().value<Wrapland::Client::drm_lease_connector_v1*>());

    QSignalSpy server_leased_spy(server.lease_device,
                                 &Wrapland::Server::drm_lease_device_v1::leased);
    QVERIFY(server_leased_spy.isValid());

    auto lease = std::unique_ptr<Wrapland::Client::drm_lease_v1>(client1.lease_device->create_lease(
        {client_connector1.client.get(), client_connector2.client.get()}));

    QVERIFY(server_leased_spy.wait());
    auto server_lease = server_leased_spy[0].first().value<Wrapland::Server::drm_lease_v1*>();

    QTemporaryFile lease_file;
    QVERIFY(lease_file.open());

    server_lease->grant(lease_file.handle());

    QSignalSpy lease_leased_spy(lease.get(), &Wrapland::Client::drm_lease_v1::leased_fd);
    QVERIFY(lease_leased_spy.wait());
    QVERIFY(lease_leased_spy[0].first().value<int>());

    QFETCH(bool, server_sends_finish);
    if (server_sends_finish) {
        QSignalSpy lease_finished_spy(lease.get(), &Wrapland::Client::drm_lease_v1::finished);
        server_lease->finish();
        QVERIFY(lease_finished_spy.wait());
    }
}

QTEST_GUILESS_MAIN(drm_lease_v1_test)
#include "drm_lease_v1.moc"
