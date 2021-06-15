/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"
#include "../../src/client/xdg_activation_v1.h"

#include "../../server/buffer.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/surface.h"
#include "../../server/xdg_activation_v1.h"

using namespace Wrapland;

class TestXdgActivation : public QObject
{
    Q_OBJECT
public:
    explicit TestXdgActivation(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void test_single_client();
    void test_multi_client();

private:
    struct {
        Server::Display* display{nullptr};
        Server::Compositor* compositor{nullptr};
        Server::Seat* seat{nullptr};
        Server::XdgActivationV1* activation{nullptr};
    } server;

    struct client {
        Client::ConnectionThread* connection{nullptr};
        Client::EventQueue* queue{nullptr};
        Client::Registry* registry{nullptr};
        Client::Compositor* compositor{nullptr};
        Client::Seat* seat{nullptr};
        Client::XdgActivationV1* activation{nullptr};
        QThread* thread{nullptr};
    } client1, client2;

    void create_client(client& client_ref);
    void cleanup_client(client& client_ref);
};

static const QString s_socketName = QStringLiteral("wrapland-test-xdg-activation-0");

TestXdgActivation::TestXdgActivation(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<std::string>();
}

void TestXdgActivation::init()
{
    qRegisterMetaType<Server::Surface*>();

    server.display = new Server::Display(this);
    server.display->setSocketName(s_socketName);
    server.display->start();
    QVERIFY(server.display->running());

    server.compositor = server.display->createCompositor(server.display);
    QVERIFY(server.compositor);

    server.seat = server.display->createSeat(server.display);
    QVERIFY(server.seat);

    server.activation = server.display->createXdgActivationV1(server.display);
    QVERIFY(server.activation);

    create_client(client1);
}

void TestXdgActivation::cleanup()
{
    cleanup_client(client1);
    cleanup_client(client2);

    delete server.display;
    server.display = nullptr;
}

void TestXdgActivation::create_client(client& client_ref)
{
    client_ref.connection = new Client::ConnectionThread;
    QSignalSpy establishedSpy(client_ref.connection, &Client::ConnectionThread::establishedChanged);
    client_ref.connection->setSocketName(s_socketName);

    client_ref.thread = new QThread(this);
    client_ref.connection->moveToThread(client_ref.thread);
    client_ref.thread->start();

    client_ref.connection->establishConnection();
    QVERIFY(establishedSpy.wait());

    client_ref.queue = new Client::EventQueue(this);
    client_ref.queue->setup(client_ref.connection);
    QVERIFY(client_ref.queue->isValid());

    QSignalSpy clientConnectedSpy(server.display, &Server::Display::clientConnected);
    QVERIFY(clientConnectedSpy.isValid());

    client_ref.registry = new Client::Registry();
    QSignalSpy compositor_spy(client_ref.registry, &Client::Registry::compositorAnnounced);
    QSignalSpy seat_spy(client_ref.registry, &Client::Registry::seatAnnounced);
    QSignalSpy activation_spy(client_ref.registry, &Client::Registry::xdgActivationV1Announced);
    QSignalSpy allAnnounced(client_ref.registry, &Client::Registry::interfacesAnnounced);
    QVERIFY(compositor_spy.isValid());
    QVERIFY(seat_spy.isValid());
    QVERIFY(activation_spy.isValid());
    QVERIFY(allAnnounced.isValid());

    client_ref.registry->setEventQueue(client_ref.queue);
    client_ref.registry->create(client_ref.connection->display());
    QVERIFY(client_ref.registry->isValid());
    client_ref.registry->setup();

    QVERIFY(allAnnounced.wait());
    client_ref.compositor
        = client_ref.registry->createCompositor(compositor_spy.first().first().value<quint32>(),
                                                compositor_spy.first().last().value<quint32>(),
                                                this);
    QVERIFY(client_ref.compositor->isValid());

    client_ref.seat = client_ref.registry->createSeat(
        seat_spy.first().first().value<quint32>(), seat_spy.first().last().value<quint32>(), this);
    QVERIFY(client_ref.seat->isValid());

    client_ref.activation = client_ref.registry->createXdgActivationV1(
        activation_spy.first().first().value<quint32>(),
        activation_spy.first().last().value<quint32>(),
        this);
    QVERIFY(client_ref.activation->isValid());

    QVERIFY(clientConnectedSpy.wait());
}

void TestXdgActivation::cleanup_client(client& client_ref)
{
    delete client_ref.activation;
    client_ref.activation = nullptr;
    delete client_ref.seat;
    client_ref.seat = nullptr;
    delete client_ref.compositor;
    client_ref.compositor = nullptr;
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

void TestXdgActivation::test_single_client()
{
    auto token = std::unique_ptr<Client::XdgActivationTokenV1>(client1.activation->create_token());

    QSignalSpy surface_spy(server.compositor, &Server::Compositor::surfaceCreated);
    QVERIFY(surface_spy.isValid());
    auto surface = std::unique_ptr<Client::Surface>(client1.compositor->createSurface());
    QVERIFY(surface_spy.wait());

    auto server_surface = surface_spy.first().first().value<Server::Surface*>();
    QVERIFY(server_surface);

    QSignalSpy token_spy(server.activation, &Server::XdgActivationV1::token_requested);
    QVERIFY(token_spy.isValid());

    auto const app_id = "app-id-1";

    token->set_serial(1, client1.seat);
    token->set_app_id(app_id);
    token->set_surface(surface.get());
    token->commit();

    QVERIFY(token_spy.wait());

    auto server_token = token_spy.first().first().value<Server::XdgActivationTokenV1*>();
    QCOMPARE(server_token->serial(), 1);
    QCOMPARE(server_token->app_id(), app_id);
    QCOMPARE(server_token->surface(), server_surface);

    QSignalSpy done_spy(token.get(), &Client::XdgActivationTokenV1::done);
    QVERIFY(done_spy.isValid());

    auto const token_string = "token123";
    server_token->done(token_string);

    QVERIFY(done_spy.wait());

    QSignalSpy activate_spy(server.activation, &Server::XdgActivationV1::activate);
    QVERIFY(activate_spy.isValid());

    client1.activation->activate(token_string, surface.get());
    QVERIFY(activate_spy.wait());

    QCOMPARE(activate_spy.first().first().value<std::string>(), token_string);
    QCOMPARE(activate_spy.first().last().value<Server::Surface*>(), server_surface);
}

void TestXdgActivation::test_multi_client()
{
    create_client(client2);

    auto token = std::unique_ptr<Client::XdgActivationTokenV1>(client1.activation->create_token());

    QSignalSpy token_spy(server.activation, &Server::XdgActivationV1::token_requested);
    QVERIFY(token_spy.isValid());

    auto const app_id = "app-id-1";

    token->set_serial(2, client1.seat);
    token->set_app_id(app_id);
    token->commit();

    QVERIFY(token_spy.wait());

    auto server_token = token_spy.first().first().value<Server::XdgActivationTokenV1*>();
    QCOMPARE(server_token->serial(), 2);
    QCOMPARE(server_token->app_id(), app_id);
    QCOMPARE(server_token->surface(), nullptr);

    QSignalSpy done_spy(token.get(), &Client::XdgActivationTokenV1::done);
    QVERIFY(done_spy.isValid());

    auto const token_string = "token456";
    server_token->done(token_string);

    QVERIFY(done_spy.wait());

    QSignalSpy activate_spy(server.activation, &Server::XdgActivationV1::activate);
    QVERIFY(activate_spy.isValid());

    QSignalSpy surface_spy(server.compositor, &Server::Compositor::surfaceCreated);
    QVERIFY(surface_spy.isValid());
    auto surface = std::unique_ptr<Client::Surface>(client2.compositor->createSurface());
    QVERIFY(surface_spy.wait());

    auto server_surface = surface_spy.first().first().value<Server::Surface*>();
    QVERIFY(server_surface);

    client2.activation->activate(token_string, surface.get());
    QVERIFY(activate_spy.wait());

    QCOMPARE(activate_spy.first().first().value<std::string>(), token_string);
    QCOMPARE(activate_spy.first().last().value<Server::Surface*>(), server_surface);
}

QTEST_GUILESS_MAIN(TestXdgActivation)
#include "xdg_activation.moc"
