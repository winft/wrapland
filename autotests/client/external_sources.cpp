/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/primary_selection.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/data_device.h"
#include "../../server/data_source.h"
#include "../../server/display.h"
#include "../../server/drag_pool.h"
#include "../../server/globals.h"
#include "../../server/primary_selection.h"
#include "../../server/surface.h"

#include <unistd.h>
#include <wayland-client.h>

#include <QtTest>

class external_sources_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_selection();
    void test_primary_selection();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

    struct client {
        Wrapland::Client::ConnectionThread* connection{nullptr};
        Wrapland::Client::EventQueue* queue{nullptr};
        Wrapland::Client::Registry* registry{nullptr};
        Wrapland::Client::Compositor* compositor{nullptr};
        Wrapland::Client::Seat* seat{nullptr};
        Wrapland::Client::DataDeviceManager* data{nullptr};
        Wrapland::Client::PrimarySelectionDeviceManager* prim_sel{nullptr};
        QThread* thread{nullptr};
    } client1;
};

constexpr auto socket_name{"wrapland-test-external-sources-0"};

void external_sources_test::init()
{
    qRegisterMetaType<Wrapland::Server::data_device*>();
    qRegisterMetaType<Wrapland::Server::data_source*>();
    qRegisterMetaType<Wrapland::Server::primary_selection_source*>();
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.compositor = server.display->createCompositor();

    server.globals.seats.push_back(server.display->createSeat());
    server.seat = server.globals.seats.back().get();
    server.seat->setHasKeyboard(true);

    server.globals.data_device_manager = server.display->createDataDeviceManager();
    server.globals.primary_selection_device_manager
        = server.display->createPrimarySelectionDeviceManager();

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
    registry.setEventQueue(client1.queue);
    registry.create(client1.connection);
    QVERIFY(registry.isValid());
    registry.setup();

    QSignalSpy interfaces_spy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfaces_spy.isValid());
    QVERIFY(interfaces_spy.wait());

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
}

void external_sources_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
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
#undef CLEANUP

    server = {};
}

class data_source_ext : public Wrapland::Server::data_source_ext
{
    Q_OBJECT
public:
    void accept(std::string const& mime_type) override
    {
        Q_EMIT accepted(mime_type);
    }

    void request_data(std::string const& mime_type, int32_t fd) override
    {
        Q_EMIT data_requested(mime_type, fd);
    }

    void cancel() override
    {
        Q_EMIT cancelled();
    }

    void send_dnd_drop_performed() override
    {
        Q_EMIT dropped();
    }

    void send_dnd_finished() override
    {
        Q_EMIT finished();
    }

    void send_action(Wrapland::Server::dnd_action action) override
    {
        Q_EMIT this->action(action);
    }

Q_SIGNALS:
    void accepted(std::string const& mime_type);
    void data_requested(std::string const& mime_type, qint32 fd);
    void cancelled();
    void dropped();
    void finished();
    void action(Wrapland::Server::dnd_action action);
};

class prim_sel_source_ext : public Wrapland::Server::primary_selection_source_ext
{
    Q_OBJECT
public:
    void request_data(std::string const& mime_type, int32_t fd) override
    {
        Q_EMIT data_requested(mime_type, fd);
    }

    void cancel() override
    {
        Q_EMIT cancelled();
    }

Q_SIGNALS:
    void data_requested(std::string const& mime_type, qint32 fd);
    void cancelled();
};

void external_sources_test::test_selection()
{
    QSignalSpy device_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::device_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> device(client1.data->getDevice(client1.seat));
    QVERIFY(device->isValid());

    QVERIFY(device_created_spy.wait());
    QCOMPARE(device_created_spy.count(), 1);
    auto server_device = device_created_spy.first().first().value<Wrapland::Server::data_device*>();
    QVERIFY(server_device);

    QSignalSpy server_surface_spy(server.globals.compositor.get(),
                                  &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(server_surface_spy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(client1.compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(server_surface_spy.wait());
    QCOMPARE(server_surface_spy.count(), 1);
    auto server_surface = server_surface_spy.first().first().value<Wrapland::Server::Surface*>();

    auto source_ext = std::make_unique<data_source_ext>();
    auto source = source_ext->src();

    QVERIFY(source);
    QCOMPARE(source->mime_types().size(), 0);

    source_ext->offer("text/plain");

    QSignalSpy server_selection_spy(server.seat, &Wrapland::Server::Seat::selectionChanged);
    QVERIFY(server_selection_spy.isValid());

    QSignalSpy client_offered_spy(device.get(), &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(client_offered_spy.isValid());

    server.seat->setFocusedKeyboardSurface(server_surface);

    server.seat->setSelection(source);
    QVERIFY(server_selection_spy.count());

    QVERIFY(client_offered_spy.wait());

    auto offer = client_offered_spy.first().first().value<Wrapland::Client::DataOffer*>();
    QVERIFY(offer);
    QCOMPARE(offer->offeredMimeTypes().count(), 1);
    QCOMPARE(offer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    QTemporaryFile file;
    QVERIFY(file.open());
    offer->receive(offer->offeredMimeTypes().first(), file.handle());

    QSignalSpy data_requested_spy(source_ext.get(), &data_source_ext::data_requested);
    QVERIFY(data_requested_spy.isValid());
    QVERIFY(data_requested_spy.wait());
    QCOMPARE(data_requested_spy.size(), 1);
    QCOMPARE(data_requested_spy.first().first().value<std::string>(), "text/plain");
    close(data_requested_spy.first().last().toInt());
}

void external_sources_test::test_primary_selection()
{
    QSignalSpy device_created_spy(
        server.globals.primary_selection_device_manager.get(),
        &Wrapland::Server::primary_selection_device_manager::device_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> device(
        client1.prim_sel->getDevice(client1.seat));
    QVERIFY(device->isValid());

    QVERIFY(device_created_spy.wait());
    QCOMPARE(device_created_spy.count(), 1);
    auto server_device
        = device_created_spy.first().first().value<Wrapland::Server::primary_selection_device*>();
    QVERIFY(server_device);

    QSignalSpy server_surface_spy(server.globals.compositor.get(),
                                  &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(server_surface_spy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(client1.compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(server_surface_spy.wait());
    QCOMPARE(server_surface_spy.count(), 1);
    auto server_surface = server_surface_spy.first().first().value<Wrapland::Server::Surface*>();

    auto source_ext = std::make_unique<prim_sel_source_ext>();
    auto source = source_ext->src();

    QVERIFY(source);
    QCOMPARE(source->mime_types().size(), 0);

    source_ext->offer("text/plain");

    QSignalSpy server_prim_sel_spy(server.seat, &Wrapland::Server::Seat::primarySelectionChanged);
    QVERIFY(server_prim_sel_spy.isValid());

    QSignalSpy client_offered_spy(device.get(),
                                  &Wrapland::Client::PrimarySelectionDevice::selectionOffered);
    QVERIFY(client_offered_spy.isValid());

    server.seat->setFocusedKeyboardSurface(server_surface);

    server.seat->setPrimarySelection(source);
    QVERIFY(server_prim_sel_spy.count());

    QVERIFY(client_offered_spy.wait());

    auto offer
        = client_offered_spy.first().first().value<Wrapland::Client::PrimarySelectionOffer*>();
    QVERIFY(offer);
    QCOMPARE(offer->offeredMimeTypes().count(), 1);
    QCOMPARE(offer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    QTemporaryFile file;
    QVERIFY(file.open());
    offer->receive(offer->offeredMimeTypes().first(), file.handle());

    QSignalSpy data_requested_spy(source_ext.get(), &prim_sel_source_ext::data_requested);
    QVERIFY(data_requested_spy.isValid());
    QVERIFY(data_requested_spy.wait());
    QCOMPARE(data_requested_spy.size(), 1);
    QCOMPARE(data_requested_spy.first().first().value<std::string>(), "text/plain");
    close(data_requested_spy.first().last().toInt());
}

QTEST_GUILESS_MAIN(external_sources_test)
#include "external_sources.moc"
