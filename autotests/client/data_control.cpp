/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/data_control_v1.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/primary_selection.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/data_control_v1.h"
#include "../../server/data_device.h"
#include "../../server/data_device_manager.h"
#include "../../server/data_source.h"
#include "../../server/display.h"
#include "../../server/primary_selection.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

class data_control_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_create();
    void test_set_selection();
    void test_set_primary_selection();

private:
    struct ctrl_device_pair {
        std::unique_ptr<Wrapland::Client::data_control_device_v1> client;
        Wrapland::Server::data_control_device_v1* server{nullptr};
    };
    struct data_device_pair {
        std::unique_ptr<Wrapland::Client::DataDevice> client;
        Wrapland::Server::data_device* server{nullptr};
    };
    struct prim_sel_device_pair {
        std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> client;
        Wrapland::Server::primary_selection_device* server{nullptr};
    };

    void create_ctrl_device(ctrl_device_pair& device);
    void create_data_device(data_device_pair& device);
    void create_prim_sel_device(prim_sel_device_pair& device);

    using ctrl_source_t = std::unique_ptr<Wrapland::Client::data_control_source_v1>;
    struct data_source_pair {
        std::unique_ptr<Wrapland::Client::DataSource> client;
        Wrapland::Server::data_source* server{nullptr};
    };
    struct prim_sel_source_pair {
        std::unique_ptr<Wrapland::Client::PrimarySelectionSource> client;
        Wrapland::Server::primary_selection_source* server{nullptr};
    };
    void create_ctrl_source(ctrl_source_t& source);
    void create_data_source(data_source_pair& source);
    void create_prim_sel_source(prim_sel_source_pair& source);

    struct {
        Wrapland::Server::Display* display{nullptr};
        Wrapland::Server::Compositor* compositor{nullptr};
        Wrapland::Server::Seat* seat{nullptr};
        Wrapland::Server::data_device_manager* data{nullptr};
        Wrapland::Server::primary_selection_device_manager* prim_sel{nullptr};
        Wrapland::Server::data_control_manager_v1* data_control{nullptr};
    } server;

    struct client {
        Wrapland::Client::ConnectionThread* connection{nullptr};
        Wrapland::Client::EventQueue* queue{nullptr};
        Wrapland::Client::Registry* registry{nullptr};
        Wrapland::Client::Compositor* compositor{nullptr};
        Wrapland::Client::Seat* seat{nullptr};
        Wrapland::Client::DataDeviceManager* data{nullptr};
        Wrapland::Client::PrimarySelectionDeviceManager* prim_sel{nullptr};
        Wrapland::Client::data_control_manager_v1* data_control{nullptr};
        QThread* thread{nullptr};
    } client1;
};

constexpr auto socket_name{"wrapland-test-text-input-v3-0"};

void data_control_test::init()
{
    qRegisterMetaType<Wrapland::Server::data_device*>();
    qRegisterMetaType<Wrapland::Server::data_source*>();
    qRegisterMetaType<Wrapland::Server::Surface*>();

    delete server.display;
    server.display = new Wrapland::Server::Display(this);
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();

    server.display->createShm();
    server.seat = server.display->createSeat();
    server.seat->setHasKeyboard(true);

    server.compositor = server.display->createCompositor();

    server.data = server.display->createDataDeviceManager();
    server.prim_sel = server.display->createPrimarySelectionDeviceManager();
    server.data_control = server.display->create_data_control_manager_v1();

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

    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(client1.queue);
    registry.create(client1.connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    client1.seat = registry.createSeat(
        registry.interface(Wrapland::Client::Registry::Interface::Seat).name,
        registry.interface(Wrapland::Client::Registry::Interface::Seat).version,
        this);
    QVERIFY(client1.seat->isValid());

    client1.compositor = registry.createCompositor(
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).name,
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).version,
        this);
    QVERIFY(client1.compositor->isValid());

    client1.data = registry.createDataDeviceManager(
        registry.interface(Wrapland::Client::Registry::Interface::DataDeviceManager).name,
        registry.interface(Wrapland::Client::Registry::Interface::DataDeviceManager).version,
        this);
    QVERIFY(client1.data->isValid());

    client1.prim_sel = registry.createPrimarySelectionDeviceManager(
        registry.interface(Wrapland::Client::Registry::Interface::PrimarySelectionDeviceManager)
            .name,
        registry.interface(Wrapland::Client::Registry::Interface::PrimarySelectionDeviceManager)
            .version,
        this);
    QVERIFY(client1.data->isValid());

    client1.data_control = registry.createDataControlManagerV1(
        registry.interface(Wrapland::Client::Registry::Interface::DataControlManagerV1).name,
        registry.interface(Wrapland::Client::Registry::Interface::DataControlManagerV1).version,
        this);
    QVERIFY(client1.data_control->isValid());
}

void data_control_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(client1.data_control)
    CLEANUP(client1.prim_sel)
    CLEANUP(client1.data)
    CLEANUP(client1.seat)
    CLEANUP(client1.compositor)
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

    CLEANUP(server.data_control)
    CLEANUP(server.prim_sel)
    CLEANUP(server.data)
    CLEANUP(server.compositor)
    CLEANUP(server.seat)
    CLEANUP(server.display)
#undef CLEANUP
}

void data_control_test::create_ctrl_device(ctrl_device_pair& device)
{
    QSignalSpy device_spy(server.data_control,
                          &Wrapland::Server::data_control_manager_v1::device_created);
    QVERIFY(device_spy.isValid());

    device.client.reset(client1.data_control->get_device(client1.seat));
    QVERIFY(device.client->isValid());

    QVERIFY(device_spy.wait());
    QCOMPARE(device_spy.count(), 1);

    device.server = device_spy.first().first().value<Wrapland::Server::data_control_device_v1*>();
    QVERIFY(device.server);
}

void data_control_test::create_data_device(data_device_pair& device)
{
    QSignalSpy device_spy(server.data, &Wrapland::Server::data_device_manager::device_created);
    QVERIFY(device_spy.isValid());

    device.client.reset(client1.data->getDevice(client1.seat));
    QVERIFY(device.client->isValid());

    QVERIFY(device_spy.wait());
    QCOMPARE(device_spy.count(), 1);

    device.server = device_spy.first().first().value<Wrapland::Server::data_device*>();
    QVERIFY(device.server);
}

void data_control_test::create_prim_sel_device(prim_sel_device_pair& device)
{
    QSignalSpy device_spy(server.prim_sel,
                          &Wrapland::Server::primary_selection_device_manager::device_created);
    QVERIFY(device_spy.isValid());

    device.client.reset(client1.prim_sel->getDevice(client1.seat));
    QVERIFY(device.client->isValid());

    QVERIFY(device_spy.wait());
    QCOMPARE(device_spy.count(), 1);

    device.server = device_spy.first().first().value<Wrapland::Server::primary_selection_device*>();
    QVERIFY(device.server);
}

void data_control_test::create_data_source(data_source_pair& source)
{
    QSignalSpy source_spy(server.data, &Wrapland::Server::data_device_manager::source_created);
    QVERIFY(source_spy.isValid());

    source.client.reset(client1.data->createSource());
    QVERIFY(source.client->isValid());

    QVERIFY(source_spy.wait());
    QCOMPARE(source_spy.count(), 1);

    source.server = source_spy.first().first().value<Wrapland::Server::data_source*>();
    QVERIFY(source.server);
}

void data_control_test::create_prim_sel_source(prim_sel_source_pair& source)
{
    QSignalSpy source_spy(server.prim_sel,
                          &Wrapland::Server::primary_selection_device_manager::source_created);
    QVERIFY(source_spy.isValid());

    source.client.reset(client1.prim_sel->createSource());
    QVERIFY(source.client->isValid());

    QVERIFY(source_spy.wait());
    QCOMPARE(source_spy.count(), 1);

    source.server = source_spy.first().first().value<Wrapland::Server::primary_selection_source*>();
    QVERIFY(source.server);
}

void data_control_test::create_ctrl_source(ctrl_source_t& source)
{
    QSignalSpy source_spy(server.data_control,
                          &Wrapland::Server::data_control_manager_v1::source_created);
    QVERIFY(source_spy.isValid());

    source.reset(client1.data_control->create_source());
    QVERIFY(source->isValid());

    QVERIFY(source_spy.wait());
    QCOMPARE(source_spy.count(), 1);
}

void data_control_test::test_create()
{
    ctrl_device_pair devices;
    create_ctrl_device(devices);

    QVERIFY(!devices.server->selection());
    QVERIFY(!server.seat->selection());

    server.seat->setSelection(devices.server->selection());
    QCOMPARE(server.seat->selection(), devices.server->selection());

    // And destroy.
    QSignalSpy destroyed_spy(devices.server, &QObject::destroyed);
    QVERIFY(destroyed_spy.isValid());
    devices.client.reset();
    QVERIFY(destroyed_spy.wait());
    QVERIFY(!server.seat->selection());
}

void data_control_test::test_set_selection()
{
    data_device_pair data_devices;
    create_data_device(data_devices);

    ctrl_device_pair ctrl_devices;
    create_ctrl_device(ctrl_devices);

    QCOMPARE(data_devices.server->seat(), server.seat);
    QVERIFY(!data_devices.server->selection());

    QVERIFY(!server.seat->selection());

    QSignalSpy surface_created_spy(server.compositor,
                                   &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surface_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(client1.compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surface_created_spy.wait());

    auto server_surface = surface_created_spy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(server_surface);
    server.seat->setFocusedKeyboardSurface(server_surface);

    // First let the data control listen in on a selection change.
    QSignalSpy server_sel_changed_spy(data_devices.server,
                                      &Wrapland::Server::data_device::selection_changed);
    QVERIFY(server_sel_changed_spy.isValid());

    QSignalSpy client_ctrl_sel_offered_spy(
        ctrl_devices.client.get(), &Wrapland::Client::data_control_device_v1::selectionOffered);
    QVERIFY(client_ctrl_sel_offered_spy.isValid());

    QVERIFY(!ctrl_devices.client->offered_selection());

    data_source_pair data_sources;
    create_data_source(data_sources);

    data_sources.client->offer(QStringLiteral("text/plain"));

    data_devices.client->setSelection(1, data_sources.client.get());
    QVERIFY(client_ctrl_sel_offered_spy.wait());
    QVERIFY(ctrl_devices.client->offered_selection());
    QVERIFY(!ctrl_devices.server->selection());
    QCOMPARE(data_devices.server->selection(), data_sources.server);

    data_devices.client->setSelection(2, nullptr);
    QVERIFY(client_ctrl_sel_offered_spy.wait());
    QVERIFY(!ctrl_devices.client->offered_selection());
    QVERIFY(!ctrl_devices.server->selection());
    QVERIFY(!data_devices.server->selection());

    // Now let the data control set the selection.
    QSignalSpy client_sel_offered_spy(data_devices.client.get(),
                                      &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(client_sel_offered_spy.isValid());

    ctrl_source_t ctrl_source;
    create_ctrl_source(ctrl_source);
    ctrl_devices.client->set_selection(ctrl_source.get());

    QVERIFY(client_sel_offered_spy.wait());
    QVERIFY(data_devices.client->offeredSelection());
    QVERIFY(ctrl_devices.server->selection());
    QVERIFY(!data_devices.server->selection());

    ctrl_devices.client->set_selection(nullptr);
    QVERIFY(client_sel_offered_spy.wait());
    QVERIFY(!data_devices.client->offeredSelection());
    QVERIFY(!ctrl_devices.server->selection());
    QVERIFY(!data_devices.server->selection());
}

void data_control_test::test_set_primary_selection()
{
    prim_sel_device_pair prim_sel_devices;
    create_prim_sel_device(prim_sel_devices);

    ctrl_device_pair ctrl_devices;
    create_ctrl_device(ctrl_devices);

    QCOMPARE(prim_sel_devices.server->seat(), server.seat);
    QVERIFY(!prim_sel_devices.server->selection());

    QVERIFY(!server.seat->selection());

    QSignalSpy surface_created_spy(server.compositor,
                                   &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surface_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(client1.compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surface_created_spy.wait());

    auto server_surface = surface_created_spy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(server_surface);
    server.seat->setFocusedKeyboardSurface(server_surface);

    // First let the data control listen in on a selection change.
    QSignalSpy server_sel_changed_spy(
        prim_sel_devices.server, &Wrapland::Server::primary_selection_device::selection_changed);
    QVERIFY(server_sel_changed_spy.isValid());

    QSignalSpy client_ctrl_sel_offered_spy(
        ctrl_devices.client.get(),
        &Wrapland::Client::data_control_device_v1::primary_selection_offered);
    QVERIFY(client_ctrl_sel_offered_spy.isValid());

    QVERIFY(!ctrl_devices.client->offered_primary_selection());

    prim_sel_source_pair prim_sel_sources;
    create_prim_sel_source(prim_sel_sources);

    prim_sel_sources.client->offer(QStringLiteral("text/plain"));

    prim_sel_devices.client->setSelection(1, prim_sel_sources.client.get());
    QVERIFY(client_ctrl_sel_offered_spy.wait());
    QVERIFY(ctrl_devices.client->offered_primary_selection());
    QVERIFY(!ctrl_devices.server->selection());
    QCOMPARE(prim_sel_devices.server->selection(), prim_sel_sources.server);

    prim_sel_devices.client->setSelection(2, nullptr);
    QVERIFY(client_ctrl_sel_offered_spy.wait());
    QVERIFY(!ctrl_devices.client->offered_primary_selection());
    QVERIFY(!ctrl_devices.server->selection());
    QVERIFY(!prim_sel_devices.server->selection());

    // Now let the data control set the selection.
    QSignalSpy client_sel_offered_spy(prim_sel_devices.client.get(),
                                      &Wrapland::Client::PrimarySelectionDevice::selectionOffered);
    QVERIFY(client_sel_offered_spy.isValid());

    ctrl_source_t ctrl_source;
    create_ctrl_source(ctrl_source);
    ctrl_devices.client->set_primary_selection(ctrl_source.get());

    QVERIFY(client_sel_offered_spy.wait());
    QVERIFY(prim_sel_devices.client->offeredSelection());
    QVERIFY(ctrl_devices.server->primary_selection());
    QVERIFY(!prim_sel_devices.server->selection());

    ctrl_devices.client->set_primary_selection(nullptr);
    QVERIFY(client_sel_offered_spy.wait());
    QVERIFY(!prim_sel_devices.client->offeredSelection());
    QVERIFY(!ctrl_devices.server->primary_selection());
    QVERIFY(!prim_sel_devices.server->selection());
}

QTEST_GUILESS_MAIN(data_control_test)
#include "data_control.moc"
