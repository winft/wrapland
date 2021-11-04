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
#include "../../src/client/text_input_v2.h"
#include "../../src/client/text_input_v3.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/surface.h"
#include "../../server/text_input_pool.h"
#include "../../server/text_input_v2.h"
#include "../../server/text_input_v3.h"

class test_text_input_method_version_compat : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_text_input_v2_v3_same_client();
    void test_text_input_v2_v3_different_clients_data();
    void test_text_input_v2_v3_different_clients();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

    struct client {
        ~client()
        {
            binds = {};

            if (connection) {
                connection->deleteLater();
            }
            if (thread) {
                thread->quit();
                thread->wait();
                delete thread;
            }
        }

        Wrapland::Client::ConnectionThread* connection{nullptr};
        QThread* thread{nullptr};

        struct {
            std::unique_ptr<Wrapland::Client::EventQueue> queue;
            std::unique_ptr<Wrapland::Client::Registry> registry;
            std::unique_ptr<Wrapland::Client::Compositor> compositor;
            std::unique_ptr<Wrapland::Client::Seat> seat;
            std::unique_ptr<Wrapland::Client::TextInputManagerV2> text_input_v2;
            std::unique_ptr<Wrapland::Client::text_input_manager_v3> text_input_v3;
        } binds;
    };

    struct client_text_input_v2 {
        client_text_input_v2(client& clt)
        {
            auto ti = clt.binds.text_input_v2->createTextInput(clt.binds.seat.get());
            text_input.reset(ti);

            spies.entered
                = std::make_unique<QSignalSpy>(ti, &Wrapland::Client::TextInputV2::entered);
            QVERIFY(spies.entered->isValid());
            spies.left = std::make_unique<QSignalSpy>(ti, &Wrapland::Client::TextInputV2::left);
            QVERIFY(spies.left->isValid());

            spies.input_panel_state_changed = std::make_unique<QSignalSpy>(
                ti, &Wrapland::Client::TextInputV2::inputPanelStateChanged);
            QVERIFY(spies.input_panel_state_changed->isValid());
            spies.text_direction_changed = std::make_unique<QSignalSpy>(
                ti, &Wrapland::Client::TextInputV2::textDirectionChanged);
            QVERIFY(spies.text_direction_changed->isValid());
            spies.language_changed
                = std::make_unique<QSignalSpy>(ti, &Wrapland::Client::TextInputV2::languageChanged);
            QVERIFY(spies.language_changed->isValid());
            spies.key_event
                = std::make_unique<QSignalSpy>(ti, &Wrapland::Client::TextInputV2::keyEvent);
            QVERIFY(spies.key_event->isValid());
            spies.composing_text_changed = std::make_unique<QSignalSpy>(
                ti, &Wrapland::Client::TextInputV2::composingTextChanged);
            QVERIFY(spies.composing_text_changed->isValid());
            spies.committed
                = std::make_unique<QSignalSpy>(ti, &Wrapland::Client::TextInputV2::committed);
            QVERIFY(spies.committed->isValid());
        }
        std::unique_ptr<Wrapland::Client::TextInputV2> text_input;
        struct {
            std::unique_ptr<QSignalSpy> entered;
            std::unique_ptr<QSignalSpy> left;
            std::unique_ptr<QSignalSpy> input_panel_state_changed;
            std::unique_ptr<QSignalSpy> text_direction_changed;
            std::unique_ptr<QSignalSpy> language_changed;
            std::unique_ptr<QSignalSpy> key_event;
            std::unique_ptr<QSignalSpy> composing_text_changed;
            std::unique_ptr<QSignalSpy> committed;
        } spies;
    };

    struct client_text_input_v3 {
        client_text_input_v3(client& clt)
        {
            auto ti = clt.binds.text_input_v3->get_text_input(clt.binds.seat.get());
            text_input.reset(ti);

            spies.entered
                = std::make_unique<QSignalSpy>(ti, &Wrapland::Client::text_input_v3::entered);
            QVERIFY(spies.entered->isValid());
            spies.left = std::make_unique<QSignalSpy>(ti, &Wrapland::Client::text_input_v3::left);
            QVERIFY(spies.left->isValid());
            spies.done = std::make_unique<QSignalSpy>(ti, &Wrapland::Client::text_input_v3::done);
            QVERIFY(spies.done->isValid());
        }
        std::unique_ptr<Wrapland::Client::text_input_v3> text_input;
        struct {
            std::unique_ptr<QSignalSpy> entered;
            std::unique_ptr<QSignalSpy> left;
            std::unique_ptr<QSignalSpy> done;
        } spies;
    };

    void setup_client(client& clt);
    void bind_text_input_v2(client& clt);
    void bind_text_input_v3(client& clt);

    Wrapland::Server::Surface* wait_for_surface();
};

constexpr auto socket_name{"wrapland-test-text-input-method-version-compat-0"};

void test_text_input_method_version_compat::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.compositor = server.display->createCompositor();

    server.seat = server.globals.seats.emplace_back(server.display->createSeat()).get();
    server.seat->setHasKeyboard(true);

    server.globals.text_input_manager_v2 = server.display->createTextInputManagerV2();
    server.globals.text_input_manager_v3 = server.display->createTextInputManagerV3();
}

void test_text_input_method_version_compat::cleanup()
{
    server = {};
}

void test_text_input_method_version_compat::setup_client(client& clt)
{
    clt.connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(clt.connection,
                            &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    clt.connection->setSocketName(socket_name);

    clt.thread = new QThread;
    clt.connection->moveToThread(clt.thread);
    clt.thread->start();

    clt.connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    clt.binds.queue = std::make_unique<Wrapland::Client::EventQueue>();
    clt.binds.queue->setup(clt.connection);

    clt.binds.registry = std::make_unique<Wrapland::Client::Registry>();
    QSignalSpy interfacesAnnouncedSpy(clt.binds.registry.get(),
                                      &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    clt.binds.registry->setEventQueue(clt.binds.queue.get());
    clt.binds.registry->create(clt.connection);
    QVERIFY(clt.binds.registry->isValid());
    clt.binds.registry->setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    clt.binds.seat.reset(clt.binds.registry->createSeat(
        clt.binds.registry->interface(Wrapland::Client::Registry::Interface::Seat).name,
        clt.binds.registry->interface(Wrapland::Client::Registry::Interface::Seat).version));
    QVERIFY(clt.binds.seat->isValid());

    clt.binds.compositor.reset(clt.binds.registry->createCompositor(
        clt.binds.registry->interface(Wrapland::Client::Registry::Interface::Compositor).name,
        clt.binds.registry->interface(Wrapland::Client::Registry::Interface::Compositor).version));
    QVERIFY(clt.binds.compositor->isValid());
}

void test_text_input_method_version_compat::bind_text_input_v2(client& clt)
{
    clt.binds.text_input_v2.reset(clt.binds.registry->createTextInputManagerV2(
        clt.binds.registry->interface(Wrapland::Client::Registry::Interface::TextInputManagerV2)
            .name,
        clt.binds.registry->interface(Wrapland::Client::Registry::Interface::TextInputManagerV2)
            .version));
    QVERIFY(clt.binds.text_input_v2->isValid());
}

void test_text_input_method_version_compat::bind_text_input_v3(client& clt)
{
    clt.binds.text_input_v3.reset(clt.binds.registry->createTextInputManagerV3(
        clt.binds.registry->interface(Wrapland::Client::Registry::Interface::TextInputManagerV3)
            .name,
        clt.binds.registry->interface(Wrapland::Client::Registry::Interface::TextInputManagerV3)
            .version));
    QVERIFY(clt.binds.text_input_v3->isValid());
}

Wrapland::Server::Surface* test_text_input_method_version_compat::wait_for_surface()
{
    auto surface_spy = QSignalSpy(server.globals.compositor.get(),
                                  &Wrapland::Server::Compositor::surfaceCreated);
    if (!surface_spy.isValid()) {
        return nullptr;
    }
    if (!surface_spy.wait(500)) {
        return nullptr;
    }
    if (surface_spy.count() != 1) {
        return nullptr;
    }
    return surface_spy.first().first().value<Wrapland::Server::Surface*>();
}

/// Check that a client with text-input v2 and v3 support makes use of v3.
void test_text_input_method_version_compat::test_text_input_v2_v3_same_client()
{
    client client1;
    setup_client(client1);

    bind_text_input_v2(client1);
    bind_text_input_v3(client1);

    auto text_input1 = client_text_input_v2(client1);
    auto text_input2 = client_text_input_v3(client1);

    auto surface1
        = std::unique_ptr<Wrapland::Client::Surface>(client1.binds.compositor->createSurface());

    auto server_surface1 = wait_for_surface();
    QVERIFY(server_surface1);

    auto surface2
        = std::unique_ptr<Wrapland::Client::Surface>(client1.binds.compositor->createSurface());

    auto server_surface2 = wait_for_surface();
    QVERIFY(server_surface2);

    QSignalSpy text_input_focus_spy(server.seat, &Wrapland::Server::Seat::focusedTextInputChanged);
    QVERIFY(text_input_focus_spy.isValid());

    // Enter first surface1.
    QVERIFY(!server.seat->text_inputs().v2.text_input);
    QVERIFY(!server.seat->text_inputs().v3.text_input);
    QVERIFY(!server.seat->text_inputs().focus.surface);
    server.seat->setFocusedKeyboardSurface(server_surface1);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface1);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);
    QVERIFY(!server.seat->text_inputs().v2.text_input);
    QCOMPARE(text_input_focus_spy.count(), 1);

    QVERIFY(text_input2.spies.entered->wait());
    QCOMPARE(text_input1.spies.entered->size(), 0);
    QCOMPARE(text_input2.spies.entered->size(), 1);
    QVERIFY(!text_input1.text_input->enteredSurface());
    QCOMPARE(text_input2.text_input->entered_surface(), surface1.get());

    QSignalSpy enable_v2_spy(server.seat, &Wrapland::Server::Seat::text_input_v2_enabled_changed);
    QVERIFY(enable_v2_spy.isValid());
    QSignalSpy enable_v3_spy(server.seat, &Wrapland::Server::Seat::text_input_v3_enabled_changed);
    QVERIFY(enable_v3_spy.isValid());
    QSignalSpy commit_spy(server_text_input, &Wrapland::Server::text_input_v3::state_committed);
    QVERIFY(commit_spy.isValid());

    text_input2.text_input->enable();
    text_input2.text_input->commit();

    QCOMPARE(server_text_input->state().enabled, false);
    QVERIFY(enable_v3_spy.wait());
    QCOMPARE(server_text_input->state().enabled, true);
    QCOMPARE(enable_v2_spy.size(), 0);
    QCOMPARE(enable_v3_spy.size(), 1);
    QCOMPARE(commit_spy.size(), 1);

    // Switch focus now to the second surface. The text-input v3 interface stays the same.
    server.seat->setFocusedKeyboardSurface(server_surface2);
    QCOMPARE(text_input_focus_spy.count(), 1);

    // text-input v2 stays enabled, but server-side it's now deactivated.
    QVERIFY(!server.seat->text_inputs().v2.text_input);
    QCOMPARE(server_text_input, server.seat->text_inputs().v3.text_input);

    // Delete surface of client1.
    QSignalSpy server_surface_destroy_spy(server_surface2,
                                          &Wrapland::Server::Surface::resourceDestroyed);
    surface2.reset();

    QVERIFY(server.seat->text_inputs().v3.text_input);
    QVERIFY(server_surface_destroy_spy.wait());
    QVERIFY(!server.seat->text_inputs().v2.text_input);
    QVERIFY(!server.seat->text_inputs().v3.text_input);
    QVERIFY(!server_text_input->entered_surface());
    QCOMPARE(text_input_focus_spy.count(), 2);
}

void test_text_input_method_version_compat::test_text_input_v2_v3_different_clients_data()
{
    QTest::addColumn<bool>("v2_and_v3");

    QTest::newRow("client2 with v2 + v3") << true;
    QTest::newRow("client2 only with v3") << false;
}

/// Check that we can switch the focus surface between clients with text-input v2 and v3.
void test_text_input_method_version_compat::test_text_input_v2_v3_different_clients()
{
    client client1;
    client client2;
    setup_client(client1);
    setup_client(client2);

    // Client1 supports text-input v2 only while client2 supports v3 only.
    bind_text_input_v2(client1);

    QFETCH(bool, v2_and_v3);
    if (v2_and_v3) {
        bind_text_input_v2(client2);
    }
    bind_text_input_v3(client2);

    auto surface1
        = std::unique_ptr<Wrapland::Client::Surface>(client1.binds.compositor->createSurface());
    auto text_input1 = client_text_input_v2(client1);

    auto server_surface1 = wait_for_surface();
    QVERIFY(server_surface1);

    auto surface2
        = std::unique_ptr<Wrapland::Client::Surface>(client2.binds.compositor->createSurface());
    auto text_input2 = client_text_input_v3(client2);

    auto server_surface2 = wait_for_surface();
    QVERIFY(server_surface2);

    QSignalSpy text_input_focus_spy(server.seat, &Wrapland::Server::Seat::focusedTextInputChanged);
    QVERIFY(text_input_focus_spy.isValid());

    // Enter first client1 with text-input v2.
    QVERIFY(!server.seat->text_inputs().v2.text_input);
    QVERIFY(!server.seat->text_inputs().v3.text_input);
    QVERIFY(!server.seat->text_inputs().focus.surface);
    server.seat->setFocusedKeyboardSurface(server_surface1);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface1);

    auto server_text_input1 = server.seat->text_inputs().v2.text_input;
    QVERIFY(server_text_input1);
    QVERIFY(!server.seat->text_inputs().v3.text_input);
    QCOMPARE(text_input_focus_spy.count(), 1);

    QVERIFY(text_input1.spies.entered->wait());
    QCOMPARE(text_input1.spies.entered->size(), 1);
    QCOMPARE(text_input1.text_input->enteredSurface(), surface1.get());

    QSignalSpy enable_v2_spy(server.seat, &Wrapland::Server::Seat::text_input_v2_enabled_changed);
    QVERIFY(enable_v2_spy.isValid());

    text_input1.text_input->enable(surface1.get());

    QCOMPARE(server_text_input1->state().enabled, false);
    QVERIFY(enable_v2_spy.wait());
    QCOMPARE(server_text_input1->state().enabled, true);
    QCOMPARE(server_text_input1->surface(), server_surface1);

    // Switch focus now to client2 with text-input v3.
    server.seat->setFocusedKeyboardSurface(server_surface2);
    QCOMPARE(text_input_focus_spy.count(), 2);

    // text-input v2 stays enabled, but server-side it's now deactivated.
    QVERIFY(server_text_input1->state().enabled);
    QVERIFY(!server.seat->text_inputs().v2.text_input);

    auto server_text_input2 = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input2);

    QVERIFY(text_input2.spies.entered->wait());
    QCOMPARE(text_input2.spies.entered->size(), 1);
    QCOMPARE(text_input2.text_input->entered_surface(), surface2.get());

    // Should be received prior but could race with the other client connection.
    QVERIFY(text_input1.spies.left->size() == 1 || text_input1.spies.left->wait());

    QSignalSpy commit_spy(server_text_input2, &Wrapland::Server::text_input_v3::state_committed);
    QVERIFY(commit_spy.isValid());

    QSignalSpy enable_v3_spy(server.seat, &Wrapland::Server::Seat::text_input_v3_enabled_changed);
    QVERIFY(enable_v3_spy.isValid());

    text_input2.text_input->enable();
    text_input2.text_input->commit();

    QCOMPARE(server_text_input2->state().enabled, false);
    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input2->state().enabled, true);
    QCOMPARE(enable_v3_spy.size(), 1);

    // Now switch back from client2 with text-input v3 to client1 with text-input v2.
    server.seat->setFocusedKeyboardSurface(server_surface1);
    QCOMPARE(text_input_focus_spy.count(), 3);
    QVERIFY(server_text_input1->state().enabled);
    QVERIFY(!server_text_input2->state().enabled);
    QVERIFY(!server.seat->text_inputs().v3.text_input);

    QCOMPARE(server_text_input1, server.seat->text_inputs().v2.text_input);

    QVERIFY(text_input1.spies.entered->wait());
    QCOMPARE(text_input1.spies.entered->size(), 2);

    // Should be received prior but could race with the other client connection.
    QVERIFY(text_input2.spies.left->size() == 1 || text_input2.spies.left->wait());

    // Delete surface of client1.
    QSignalSpy server_surface_destroy_spy(server_surface1,
                                          &Wrapland::Server::Surface::resourceDestroyed);
    surface1.reset();

    QVERIFY(server.seat->text_inputs().v2.text_input);
    QVERIFY(server_surface_destroy_spy.wait());
    QVERIFY(!server.seat->text_inputs().v2.text_input);
    QVERIFY(!server.seat->text_inputs().v3.text_input);
    QVERIFY(!server_text_input1->surface());
    QCOMPARE(text_input_focus_spy.count(), 4);
}

QTEST_GUILESS_MAIN(test_text_input_method_version_compat)
#include "text_input_method_version_compat.moc"
