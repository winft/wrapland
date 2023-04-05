/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/plasma_activation_feedback.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/buffer.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/plasma_activation_feedback.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

#include "../../tests/globals.h"

using namespace Wrapland;

class test_plasma_activation_feedback : public QObject
{
    Q_OBJECT
public:
    explicit test_plasma_activation_feedback(QObject* parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void test_app_id();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    struct client {
        Client::ConnectionThread* connection{nullptr};
        Client::EventQueue* queue{nullptr};
        Client::Registry* registry{nullptr};
        Client::Compositor* compositor{nullptr};
        Client::Seat* seat{nullptr};
        Client::plasma_activation_feedback* activation_feedback{nullptr};
        QThread* thread{nullptr};
    } client1;

    void create_client(client& client_ref);
    void cleanup_client(client& client_ref);
};

constexpr auto socket_name{"wrapland-test-plasma-activation-feedback-0"};

test_plasma_activation_feedback::test_plasma_activation_feedback(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<std::string>();
}

void test_plasma_activation_feedback::init()
{
    qRegisterMetaType<Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());
    server.globals.seats.emplace_back(
        std::make_unique<Wrapland::Server::Seat>(server.display.get()));
    server.globals.plasma_activation_feedback
        = std::make_unique<Wrapland::Server::plasma_activation_feedback>(server.display.get());

    create_client(client1);
}

void test_plasma_activation_feedback::cleanup()
{
    cleanup_client(client1);

    server = {};
}

void test_plasma_activation_feedback::create_client(client& client_ref)
{
    client_ref.connection = new Client::ConnectionThread;
    QSignalSpy establishedSpy(client_ref.connection, &Client::ConnectionThread::establishedChanged);
    client_ref.connection->setSocketName(socket_name);

    client_ref.thread = new QThread(this);
    client_ref.connection->moveToThread(client_ref.thread);
    client_ref.thread->start();

    client_ref.connection->establishConnection();
    QVERIFY(establishedSpy.wait());

    client_ref.queue = new Client::EventQueue(this);
    client_ref.queue->setup(client_ref.connection);
    QVERIFY(client_ref.queue->isValid());

    QSignalSpy clientConnectedSpy(server.display.get(), &Server::Display::clientConnected);
    QVERIFY(clientConnectedSpy.isValid());

    client_ref.registry = new Client::Registry();
    QSignalSpy compositor_spy(client_ref.registry, &Client::Registry::compositorAnnounced);
    QSignalSpy seat_spy(client_ref.registry, &Client::Registry::seatAnnounced);
    QSignalSpy activation_spy(client_ref.registry,
                              &Client::Registry::plasmaActivationFeedbackAnnounced);
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

    client_ref.activation_feedback = client_ref.registry->createPlasmaActivationFeedback(
        activation_spy.first().first().value<quint32>(),
        activation_spy.first().last().value<quint32>(),
        this);
    QVERIFY(client_ref.activation_feedback->isValid());

    QVERIFY(clientConnectedSpy.wait());
}

void test_plasma_activation_feedback::cleanup_client(client& client_ref)
{
    delete client_ref.activation_feedback;
    client_ref.activation_feedback = nullptr;
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

void test_plasma_activation_feedback::test_app_id()
{
    QSignalSpy activation_spy(client1.activation_feedback,
                              &Client::plasma_activation_feedback::activation);
    QVERIFY(activation_spy.isValid());

    auto const app_id = "app-id-1";
    server.globals.plasma_activation_feedback->app_id(app_id);
    QVERIFY(activation_spy.wait());

    auto activation = activation_spy.first().first().value<Client::plasma_activation*>();
    QVERIFY(activation);

    QSignalSpy app_id_spy(activation, &Client::plasma_activation::app_id_changed);
    QVERIFY(app_id_spy.isValid());

    if (activation->app_id().empty()) {
        QVERIFY(app_id_spy.wait());
    }

    QCOMPARE(activation->app_id(), app_id);

    QSignalSpy finished_spy(activation, &Client::plasma_activation::finished);
    QVERIFY(finished_spy.isValid());
    QVERIFY(!activation->is_finished());

    server.globals.plasma_activation_feedback->finished(app_id);
    QVERIFY(finished_spy.wait());
    QVERIFY(activation->is_finished());
    delete activation;
}

QTEST_GUILESS_MAIN(test_plasma_activation_feedback)
#include "plasma_activation_feedback.moc"
