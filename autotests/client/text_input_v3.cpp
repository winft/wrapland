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
#include "../../src/client/text_input_v3.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/seat.h"
#include "../../server/surface.h"
#include "../../server/text_input_pool.h"
#include "../../server/text_input_v3.h"

class text_input_v3_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_enter_leave();
    void test_cursor_rectangle();
    void test_surrounding_text();
    void test_content_hints_data();
    void test_content_hints();
    void test_content_purpose_data();
    void test_content_purpose();
    void test_preedit();
    // TODO(romangg): test that outdated done events are ignored.

private:
    Wrapland::Server::Surface* wait_for_surface();
    Wrapland::Client::text_input_v3* create_text_input();

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
        Wrapland::Client::text_input_manager_v3* text_input{nullptr};
        QThread* thread{nullptr};
    } client1;
};

constexpr auto socket_name{"wrapland-test-text-input-v3-0"};

void text_input_v3_test::init()
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

    server.globals.text_input_manager_v3 = server.display->createTextInputManagerV3();

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

    client1.text_input = registry.createTextInputManagerV3(
        registry.interface(Wrapland::Client::Registry::Interface::TextInputManagerV3).name,
        registry.interface(Wrapland::Client::Registry::Interface::TextInputManagerV3).version,
        this);
    QVERIFY(client1.text_input->isValid());
}

void text_input_v3_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(client1.text_input)
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

    server = {};
#undef CLEANUP
}

Wrapland::Server::Surface* text_input_v3_test::wait_for_surface()
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

Wrapland::Client::text_input_v3* text_input_v3_test::create_text_input()
{
    return client1.text_input->get_text_input(client1.seat);
}

void text_input_v3_test::test_enter_leave()
{
    // This test verifies that enter/leave events are sent correctly.
    auto surface = std::unique_ptr<Wrapland::Client::Surface>(client1.compositor->createSurface());
    auto text_input = std::unique_ptr<Wrapland::Client::text_input_v3>(create_text_input());
    QVERIFY(text_input);

    auto server_surface = wait_for_surface();
    QVERIFY(server_surface);

    QSignalSpy entered_spy(text_input.get(), &Wrapland::Client::text_input_v3::entered);
    QVERIFY(entered_spy.isValid());
    QSignalSpy left_spy(text_input.get(), &Wrapland::Client::text_input_v3::left);
    QVERIFY(left_spy.isValid());

    QSignalSpy text_input_focus_spy(server.seat, &Wrapland::Server::Seat::focusedTextInputChanged);
    QVERIFY(text_input_focus_spy.isValid());

    // Enter now.
    QVERIFY(!server.seat->text_inputs().v3.text_input);
    QVERIFY(!server.seat->text_inputs().focus.surface);
    server.seat->setFocusedKeyboardSurface(server_surface);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);
    QCOMPARE(text_input_focus_spy.count(), 1);
    QCOMPARE(server_text_input->entered_surface(), server_surface);

    QVERIFY(entered_spy.wait());
    QCOMPARE(entered_spy.count(), 1);
    QCOMPARE(text_input->entered_surface(), surface.get());

    QSignalSpy commit_spy(server_text_input, &Wrapland::Server::text_input_v3::state_committed);
    QVERIFY(commit_spy.isValid());

    text_input->enable();
    text_input->commit();

    QCOMPARE(server_text_input->state().enabled, false);
    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().enabled, true);

    // Leave now.
    server.seat->setFocusedKeyboardSurface(nullptr);
    QCOMPARE(text_input_focus_spy.count(), 2);
    QVERIFY(!server.seat->text_inputs().v3.text_input);
    QVERIFY(!server_text_input->state().enabled);

    QVERIFY(left_spy.wait());
    QCOMPARE(left_spy.count(), 1);
    QVERIFY(!text_input->entered_surface());
    QVERIFY(!server_text_input->entered_surface());

    // Enter one more time. All data is reset.
    server.seat->setFocusedKeyboardSurface(server_surface);
    QCOMPARE(server_text_input, server.seat->text_inputs().v3.text_input);
    QCOMPARE(text_input_focus_spy.count(), 3);

    QVERIFY(entered_spy.wait());
    QCOMPARE(entered_spy.count(), 2);
    QCOMPARE(text_input->entered_surface(), surface.get());
    QCOMPARE(server_text_input->entered_surface(), server_surface);

    text_input->enable();
    text_input->commit();

    QCOMPARE(server_text_input->state().enabled, false);
    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().enabled, true);

    // Deactivate on client side.
    text_input->disable();
    text_input->commit();

    QCOMPARE(server_text_input->state().enabled, true);
    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().enabled, false);

    // No focus change.
    QCOMPARE(text_input_focus_spy.count(), 3);
    QCOMPARE(server.seat->text_inputs().v3.text_input, server_text_input);
    QCOMPARE(server_text_input->entered_surface(), server_surface);

    // Enable again.
    text_input->enable();
    text_input->commit();

    QCOMPARE(server_text_input->state().enabled, false);
    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().enabled, true);

    QCOMPARE(text_input->entered_surface(), surface.get());
    QCOMPARE(server_text_input->entered_surface(), server_surface);

    // delete the client and wait for the server to catch up
    QSignalSpy server_surface_destroy_spy(server_surface, &QObject::destroyed);
    surface.reset();
    QVERIFY(!text_input->entered_surface());

    QVERIFY(server.seat->text_inputs().v3.text_input);
    QVERIFY(server_surface_destroy_spy.wait());
    QVERIFY(!server.seat->text_inputs().v3.text_input);
    QVERIFY(!server_text_input->entered_surface());
}

void text_input_v3_test::test_cursor_rectangle()
{
    // This test verifies that the cursor rectangle is properly passed from client to server.
    auto surface = std::unique_ptr<Wrapland::Client::Surface>(client1.compositor->createSurface());
    auto text_input = std::unique_ptr<Wrapland::Client::text_input_v3>(create_text_input());
    QVERIFY(text_input);

    auto server_surface = wait_for_surface();
    QVERIFY(server_surface);

    QSignalSpy entered_spy(text_input.get(), &Wrapland::Client::text_input_v3::entered);
    QVERIFY(entered_spy.isValid());

    // Enter now.
    server.seat->setFocusedKeyboardSurface(server_surface);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);

    QVERIFY(entered_spy.wait());
    QCOMPARE(entered_spy.count(), 1);
    QCOMPARE(text_input->entered_surface(), surface.get());

    QSignalSpy commit_spy(server_text_input, &Wrapland::Server::text_input_v3::state_committed);
    QVERIFY(commit_spy.isValid());

    auto const rect = QRect(10, 20, 30, 40);
    text_input->enable();
    text_input->set_cursor_rectangle(rect);
    text_input->commit();

    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().enabled, true);
    QCOMPARE(server_text_input->state().cursor_rectangle, rect);
}

void text_input_v3_test::test_surrounding_text()
{
    // This test verifies that surrounding text is properly passed from client to server.
    auto surface = std::unique_ptr<Wrapland::Client::Surface>(client1.compositor->createSurface());
    auto text_input = std::unique_ptr<Wrapland::Client::text_input_v3>(create_text_input());
    QVERIFY(text_input);

    auto server_surface = wait_for_surface();
    QVERIFY(server_surface);

    QSignalSpy entered_spy(text_input.get(), &Wrapland::Client::text_input_v3::entered);
    QVERIFY(entered_spy.isValid());

    // Enter now.
    server.seat->setFocusedKeyboardSurface(server_surface);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);

    QVERIFY(entered_spy.wait());
    QCOMPARE(entered_spy.count(), 1);
    QCOMPARE(text_input->entered_surface(), surface.get());

    QVERIFY(server_text_input->state().surrounding_text.data.empty());
    QCOMPARE(server_text_input->state().surrounding_text.cursor_position, 0);
    QCOMPARE(server_text_input->state().surrounding_text.selection_anchor, 0);
    QCOMPARE(server_text_input->state().surrounding_text.change_cause,
             Wrapland::Server::text_input_v3_change_cause::other);

    QSignalSpy commit_spy(server_text_input, &Wrapland::Server::text_input_v3::state_committed);
    QVERIFY(commit_spy.isValid());

    auto const surrounding_text = "100 €, 100 $";
    text_input->enable();
    text_input->set_surrounding_text(
        surrounding_text, 5, 6, Wrapland::Client::text_input_v3_change_cause::input_method);
    text_input->commit();

    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().surrounding_text.data, surrounding_text);
    QCOMPARE(server_text_input->state().surrounding_text.cursor_position, 5);
    QCOMPARE(server_text_input->state().surrounding_text.selection_anchor, 6);
    QCOMPARE(server_text_input->state().surrounding_text.change_cause,
             Wrapland::Server::text_input_v3_change_cause::input_method);
}

using server_hint = Wrapland::Server::text_input_v3_content_hint;
using server_hints = Wrapland::Server::text_input_v3_content_hints;
using client_hint = Wrapland::Client::text_input_v3_content_hint;
using client_hints = Wrapland::Client::text_input_v3_content_hints;

using server_purpose = Wrapland::Server::text_input_v3_content_purpose;
using client_purpose = Wrapland::Client::text_input_v3_content_purpose;

void text_input_v3_test::test_content_hints_data()
{
    QTest::addColumn<client_hints>("clthints");
    QTest::addColumn<server_hints>("srvhints");

    QTest::newRow("completion") << client_hints(client_hint::completion)
                                << server_hints(server_hint::completion);
    QTest::newRow("Correction") << client_hints(client_hint::spellcheck)
                                << server_hints(server_hint::spellcheck);
    QTest::newRow("Capitalization") << client_hints(client_hint::auto_capitalization)
                                    << server_hints(server_hint::auto_capitalization);
    QTest::newRow("lowercase") << client_hints(client_hint::lowercase)
                               << server_hints(server_hint::lowercase);
    QTest::newRow("uppercase") << client_hints(client_hint::uppercase)
                               << server_hints(server_hint::uppercase);
    QTest::newRow("titlecase") << client_hints(client_hint::titlecase)
                               << server_hints(server_hint::titlecase);
    QTest::newRow("hidden_text") << client_hints(client_hint::hidden_text)
                                 << server_hints(server_hint::hidden_text);
    QTest::newRow("sensitive_data")
        << client_hints(client_hint::sensitive_data) << server_hints(server_hint::sensitive_data);
    QTest::newRow("latin") << client_hints(client_hint::latin) << server_hints(server_hint::latin);
    QTest::newRow("multiline") << client_hints(client_hint::multiline)
                               << server_hints(server_hint::multiline);

    QTest::newRow("autos") << (client_hint::completion | client_hint::spellcheck
                               | client_hint::auto_capitalization)
                           << (server_hint::completion | server_hint::spellcheck
                               | server_hint::auto_capitalization);

    // all has combinations which don't make sense - for example both lowercase and uppercase.
    QTest::newRow("all") << (client_hint::completion | client_hint::spellcheck
                             | client_hint::auto_capitalization | client_hint::lowercase
                             | client_hint::uppercase | client_hint::titlecase
                             | client_hint::hidden_text | client_hint::sensitive_data
                             | client_hint::latin | client_hint::multiline)
                         << (server_hint::completion | server_hint::spellcheck
                             | server_hint::auto_capitalization | server_hint::lowercase
                             | server_hint::uppercase | server_hint::titlecase
                             | server_hint::hidden_text | server_hint::sensitive_data
                             | server_hint::latin | server_hint::multiline);
}

void text_input_v3_test::test_content_hints()
{
    // This test verifies that content hints are properly passed from client to server.
    auto surface = std::unique_ptr<Wrapland::Client::Surface>(client1.compositor->createSurface());
    auto text_input = std::unique_ptr<Wrapland::Client::text_input_v3>(create_text_input());
    QVERIFY(text_input);

    auto server_surface = wait_for_surface();
    QVERIFY(server_surface);

    QSignalSpy entered_spy(text_input.get(), &Wrapland::Client::text_input_v3::entered);
    QVERIFY(entered_spy.isValid());

    // Enter now.
    server.seat->setFocusedKeyboardSurface(server_surface);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);

    QVERIFY(entered_spy.wait());
    QCOMPARE(entered_spy.count(), 1);
    QCOMPARE(text_input->entered_surface(), surface.get());

    QCOMPARE(server_text_input->state().content.hints, server_hints());

    QSignalSpy commit_spy(server_text_input, &Wrapland::Server::text_input_v3::state_committed);
    QVERIFY(commit_spy.isValid());

    QFETCH(client_hints, clthints);
    text_input->set_content_type(clthints, client_purpose::normal);
    text_input->commit();

    QVERIFY(commit_spy.wait());
    QTEST(server_text_input->state().content.hints, "srvhints");

    // Unset.
    text_input->set_content_type(client_hints(), client_purpose::normal);
    text_input->commit();
    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().content.hints, server_hints());
}

void text_input_v3_test::test_content_purpose_data()
{
    QTest::addColumn<client_purpose>("cltpurpose");
    QTest::addColumn<server_purpose>("srvpurpose");

    QTest::newRow("alpha") << client_purpose::alpha << server_purpose::alpha;
    QTest::newRow("digits") << client_purpose::digits << server_purpose::digits;
    QTest::newRow("number") << client_purpose::number << server_purpose::number;
    QTest::newRow("phone") << client_purpose::phone << server_purpose::phone;
    QTest::newRow("url") << client_purpose::url << server_purpose::url;
    QTest::newRow("email") << client_purpose::email << server_purpose::email;
    QTest::newRow("name") << client_purpose::name << server_purpose::name;
    QTest::newRow("password") << client_purpose::password << server_purpose::password;
    QTest::newRow("date") << client_purpose::date << server_purpose::date;
    QTest::newRow("time") << client_purpose::time << server_purpose::time;
    QTest::newRow("datetime") << client_purpose::datetime << server_purpose::datetime;
    QTest::newRow("terminal") << client_purpose::terminal << server_purpose::terminal;
}

void text_input_v3_test::test_content_purpose()
{
    // This test verifies that the content purpose is properly passed from client to server.
    auto surface = std::unique_ptr<Wrapland::Client::Surface>(client1.compositor->createSurface());
    auto text_input = std::unique_ptr<Wrapland::Client::text_input_v3>(create_text_input());
    QVERIFY(text_input);

    auto server_surface = wait_for_surface();
    QVERIFY(server_surface);

    QSignalSpy entered_spy(text_input.get(), &Wrapland::Client::text_input_v3::entered);
    QVERIFY(entered_spy.isValid());

    // Enter now.
    server.seat->setFocusedKeyboardSurface(server_surface);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);

    QVERIFY(entered_spy.wait());
    QCOMPARE(entered_spy.count(), 1);
    QCOMPARE(text_input->entered_surface(), surface.get());

    QCOMPARE(server_text_input->state().content.purpose, server_purpose());

    QSignalSpy commit_spy(server_text_input, &Wrapland::Server::text_input_v3::state_committed);
    QVERIFY(commit_spy.isValid());

    QFETCH(client_purpose, cltpurpose);
    text_input->set_content_type(client_hint::none, cltpurpose);
    text_input->commit();

    QVERIFY(commit_spy.wait());
    QTEST(server_text_input->state().content.purpose, "srvpurpose");

    // Unset.
    text_input->set_content_type(client_hint::none, client_purpose());
    text_input->commit();
    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().content.purpose, server_purpose());
}

void text_input_v3_test::test_preedit()
{
    // This test verifies that the preedit data is properly passed from server to client.
    auto surface = std::unique_ptr<Wrapland::Client::Surface>(client1.compositor->createSurface());
    auto text_input = std::unique_ptr<Wrapland::Client::text_input_v3>(create_text_input());
    QVERIFY(text_input);

    auto server_surface = wait_for_surface();
    QVERIFY(server_surface);

    QSignalSpy entered_spy(text_input.get(), &Wrapland::Client::text_input_v3::entered);
    QVERIFY(entered_spy.isValid());

    // Enter now.
    server.seat->setFocusedKeyboardSurface(server_surface);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);

    QVERIFY(entered_spy.wait());
    QCOMPARE(entered_spy.count(), 1);
    QCOMPARE(text_input->entered_surface(), surface.get());

    QSignalSpy done_spy(text_input.get(), &Wrapland::Client::text_input_v3::done);
    QVERIFY(done_spy.isValid());

    server_text_input->set_preedit_string("foo", 1, 2);
    server_text_input->done();

    QVERIFY(done_spy.wait());

    QCOMPARE(text_input->state().preedit_string.data, "foo");
    QCOMPARE(text_input->state().preedit_string.cursor_begin, 1);
    QCOMPARE(text_input->state().preedit_string.cursor_end, 2);
}

QTEST_GUILESS_MAIN(text_input_v3_test)
#include "text_input_v3.moc"
