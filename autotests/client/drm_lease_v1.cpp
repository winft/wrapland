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
#include "../../server/output.h"

#include <deque>

class drm_lease_v1_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_connectors();
    void test_lease();

private:
    struct {
        Wrapland::Server::Display* display{nullptr};
        Wrapland::Server::drm_lease_device_v1* lease_device{nullptr};
    } server;

    struct client {
        Wrapland::Client::ConnectionThread* connection{nullptr};
        Wrapland::Client::EventQueue* queue{nullptr};
        Wrapland::Client::Registry* registry{nullptr};
        Wrapland::Client::drm_lease_device_v1* lease_device{nullptr};
        QThread* thread{nullptr};
    } client1;
};

constexpr auto socket_name = "wrapland-test-drm-lease-v1-0";

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
    delete server.display;
    server.display = new Wrapland::Server::Display(this);
    server.display->setSocketName(std::string(socket_name));
    server.display->start();

    server.display->createShm();
    server.lease_device = server.display->createDrmLeaseDeviceV1();

    // setup connection
    client1.connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(client1.connection,
                            &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    client1.connection->setSocketName(socket_name);

    client1.thread = new QThread(this);
    client1.connection->moveToThread(client1.thread);
    client1.thread->start();

    client1.connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    client1.queue = new Wrapland::Client::EventQueue(this);
    client1.queue->setup(client1.connection);

    client1.registry = new Wrapland::Client::Registry;

    QSignalSpy interfacesAnnouncedSpy(client1.registry,
                                      &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    client1.registry->setEventQueue(client1.queue);
    client1.registry->create(client1.connection);

    QVERIFY(client1.registry->isValid());
    client1.registry->setup();
    QVERIFY(interfacesAnnouncedSpy.wait());
}

void drm_lease_v1_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(client1.lease_device)
    CLEANUP(client1.registry)
    CLEANUP(client1.queue)
    if (client1.connection) {
        client1.connection->deleteLater();
        client1.connection = nullptr;
    }
    if (client1.thread) {
        client1.thread->quit();
        client1.thread->wait();
        delete client1.thread;
        client1.thread = nullptr;
    }

    CLEANUP(server.lease_device)
    CLEANUP(server.display)
#undef CLEANUP
}

void drm_lease_v1_test::test_connectors()
{
    // This test verifies that connectors are advertised correctly.

    std::vector<std::unique_ptr<Wrapland::Server::Output>> server_outputs;
    auto add_output = [&server_outputs, this] {
        server_outputs.emplace_back(std::make_unique<Wrapland::Server::Output>(server.display));
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

void drm_lease_v1_test::test_lease()
{
    // This test verifies that leases are communicated correctly.

    std::vector<std::unique_ptr<Wrapland::Server::Output>> server_outputs;
    auto add_output = [&server_outputs, this] {
        server_outputs.emplace_back(std::make_unique<Wrapland::Server::Output>(server.display));
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

    QSignalSpy lease_finished_spy(lease.get(), &Wrapland::Client::drm_lease_v1::finished);
    server_lease->finish();
    QVERIFY(lease_finished_spy.wait());
}

QTEST_GUILESS_MAIN(drm_lease_v1_test)
#include "drm_lease_v1.moc"
