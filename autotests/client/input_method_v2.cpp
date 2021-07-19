/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/input_method_v2.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/input_method_v2.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

#include <linux/input.h>

class input_method_v2_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_activate();
    void test_surrounding_text();
    void test_content_hints_data();
    void test_content_hints();
    void test_content_purpose_data();
    void test_content_purpose();
    void test_commit();
    void test_popup_surface();
    void test_keyboard_grab();

private:
    Wrapland::Client::input_method_v2* get_input_method();

    struct {
        Wrapland::Server::Display* display{nullptr};
        Wrapland::Server::Compositor* compositor{nullptr};
        Wrapland::Server::Seat* seat{nullptr};
        Wrapland::Server::text_input_manager_v3* text_input{nullptr};
        Wrapland::Server::input_method_manager_v2* input_method{nullptr};
    } server;

    struct client {
        Wrapland::Client::ConnectionThread* connection{nullptr};
        Wrapland::Client::EventQueue* queue{nullptr};
        Wrapland::Client::Registry* registry{nullptr};
        Wrapland::Client::Compositor* compositor{nullptr};
        Wrapland::Client::Seat* seat{nullptr};
        Wrapland::Client::text_input_manager_v3* text_input{nullptr};
        Wrapland::Client::input_method_manager_v2* input_method{nullptr};
        QThread* thread{nullptr};
    } client1;
};

constexpr auto socket_name = "wrapland-test-input-method-v2-0";

void input_method_v2_test::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    delete server.display;
    server.display = new Wrapland::Server::Display(this);
    server.display->setSocketName(std::string(socket_name));
    server.display->start();

    server.display->createShm();
    server.seat = server.display->createSeat();
    server.seat->setHasKeyboard(true);

    server.compositor = server.display->createCompositor();
    server.text_input = server.display->createTextInputManagerV3();
    server.input_method = server.display->createInputMethodManagerV2();

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

    client1.input_method = registry.createInputMethodManagerV2(
        registry.interface(Wrapland::Client::Registry::Interface::InputMethodManagerV2).name,
        registry.interface(Wrapland::Client::Registry::Interface::InputMethodManagerV2).version,
        this);
    QVERIFY(client1.input_method->isValid());
}

void input_method_v2_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(client1.input_method)
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

    CLEANUP(server.input_method)
    CLEANUP(server.text_input)
    CLEANUP(server.compositor)
    CLEANUP(server.seat)
    CLEANUP(server.display)
#undef CLEANUP
}

Wrapland::Client::input_method_v2* input_method_v2_test::get_input_method()
{
    return client1.input_method->get_input_method(client1.seat);
}

void input_method_v2_test::test_activate()
{
    // This test verifies that activation is done correctly.
    QSignalSpy input_method_spy(server.seat, &Wrapland::Server::Seat::input_method_v2_changed);
    QVERIFY(input_method_spy.isValid());

    auto input_method = std::unique_ptr<Wrapland::Client::input_method_v2>(get_input_method());
    QVERIFY(input_method);
    QVERIFY(input_method_spy.wait());

    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    QSignalSpy done_spy(input_method.get(), &Wrapland::Client::input_method_v2::done);
    QVERIFY(done_spy.isValid());

    // Now activate.
    server_input_method->set_active(true);
    server_input_method->done();

    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, true);

    // Now deactivate.
    server_input_method->set_active(false);
    server_input_method->done();

    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, false);
}

void input_method_v2_test::test_surrounding_text()
{
    // This test verifies that surrounding text is sent correctly.
    QSignalSpy input_method_spy(server.seat, &Wrapland::Server::Seat::input_method_v2_changed);
    QVERIFY(input_method_spy.isValid());

    auto input_method = std::unique_ptr<Wrapland::Client::input_method_v2>(get_input_method());
    QVERIFY(input_method);
    QVERIFY(input_method_spy.wait());

    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    QSignalSpy done_spy(input_method.get(), &Wrapland::Client::input_method_v2::done);
    QVERIFY(done_spy.isValid());

    QVERIFY(input_method->state().surrounding_text.data.empty());
    QCOMPARE(input_method->state().surrounding_text.cursor_position, 0);
    QCOMPARE(input_method->state().surrounding_text.selection_anchor, 0);
    QCOMPARE(input_method->state().surrounding_text.change_cause,
             Wrapland::Client::text_input_v3_change_cause::other);

    // Now activate and send surrounding text.
    auto const surrounding_text = "100 €, 100 $";

    server_input_method->set_active(true);
    server_input_method->set_surrounding_text(
        surrounding_text, 5, 6, Wrapland::Server::text_input_v3_change_cause::input_method);
    server_input_method->done();

    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, true);
    QCOMPARE(input_method->state().surrounding_text.data, surrounding_text);
    QCOMPARE(input_method->state().surrounding_text.cursor_position, 5);
    QCOMPARE(input_method->state().surrounding_text.selection_anchor, 6);
    QCOMPARE(input_method->state().surrounding_text.change_cause,
             Wrapland::Client::text_input_v3_change_cause::input_method);

    // Now deactivate.
    server_input_method->set_active(false);
    server_input_method->done();

    // Not active anymore but surrounding text data still available.
    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, false);
    QCOMPARE(input_method->state().surrounding_text.data, surrounding_text);
    QCOMPARE(input_method->state().surrounding_text.cursor_position, 5);
    QCOMPARE(input_method->state().surrounding_text.selection_anchor, 6);
    QCOMPARE(input_method->state().surrounding_text.change_cause,
             Wrapland::Client::text_input_v3_change_cause::input_method);

    // Activate again but do not set surrounding text.
    server_input_method->set_active(true);
    server_input_method->done();

    // Active again and state reset now.
    QVERIFY(done_spy.wait());
    QVERIFY(input_method->state().surrounding_text.data.empty());
    QCOMPARE(input_method->state().surrounding_text.cursor_position, 0);
    QCOMPARE(input_method->state().surrounding_text.selection_anchor, 0);
    QCOMPARE(input_method->state().surrounding_text.change_cause,
             Wrapland::Client::text_input_v3_change_cause::other);
}

using server_hint = Wrapland::Server::text_input_v3_content_hint;
using server_hints = Wrapland::Server::text_input_v3_content_hints;
using client_hint = Wrapland::Client::text_input_v3_content_hint;
using client_hints = Wrapland::Client::text_input_v3_content_hints;

using server_purpose = Wrapland::Server::text_input_v3_content_purpose;
using client_purpose = Wrapland::Client::text_input_v3_content_purpose;

void input_method_v2_test::test_content_hints_data()
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

void input_method_v2_test::test_content_hints()
{
    // This test verifies that content hints are sent correctly.
    QSignalSpy input_method_spy(server.seat, &Wrapland::Server::Seat::input_method_v2_changed);
    QVERIFY(input_method_spy.isValid());

    auto input_method = std::unique_ptr<Wrapland::Client::input_method_v2>(get_input_method());
    QVERIFY(input_method);
    QVERIFY(input_method_spy.wait());

    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    QSignalSpy done_spy(input_method.get(), &Wrapland::Client::input_method_v2::done);
    QVERIFY(done_spy.isValid());

    QCOMPARE(input_method->state().content.hints, client_hints());

    // Now activate and send content hints.
    server_input_method->set_active(true);
    QFETCH(server_hints, srvhints);
    QFETCH(client_hints, clthints);
    server_input_method->set_content_type(srvhints, server_purpose::normal);
    server_input_method->done();

    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, true);
    QCOMPARE(input_method->state().content.hints, clthints);

    // Now deactivate.
    server_input_method->set_active(false);
    server_input_method->done();

    // Not active anymore but content hints data still available.
    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, false);
    QCOMPARE(input_method->state().content.hints, clthints);

    // Activate again but do not set content hints.
    server_input_method->set_active(true);
    server_input_method->done();

    // Active again and state reset now.
    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().content.hints, client_hints());
}

void input_method_v2_test::test_content_purpose_data()
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

void input_method_v2_test::test_content_purpose()
{
    // This test verifies that the content purpose is sent correctly.
    QSignalSpy input_method_spy(server.seat, &Wrapland::Server::Seat::input_method_v2_changed);
    QVERIFY(input_method_spy.isValid());

    auto input_method = std::unique_ptr<Wrapland::Client::input_method_v2>(get_input_method());
    QVERIFY(input_method);
    QVERIFY(input_method_spy.wait());

    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    QSignalSpy done_spy(input_method.get(), &Wrapland::Client::input_method_v2::done);
    QVERIFY(done_spy.isValid());

    QCOMPARE(input_method->state().content.purpose, client_purpose::normal);

    // Now activate and send content hints.
    server_input_method->set_active(true);
    QFETCH(server_purpose, srvpurpose);
    QFETCH(client_purpose, cltpurpose);
    server_input_method->set_content_type(server_hints(), srvpurpose);
    server_input_method->done();

    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, true);
    QCOMPARE(input_method->state().content.purpose, cltpurpose);

    // Now deactivate.
    server_input_method->set_active(false);
    server_input_method->done();

    // Not active anymore but content hints data still available.
    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, false);
    QCOMPARE(input_method->state().content.purpose, cltpurpose);

    // Activate again but do not set content hints.
    server_input_method->set_active(true);
    server_input_method->done();

    // Active again and state reset now.
    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().content.purpose, client_purpose::normal);
}

void input_method_v2_test::test_commit()
{
    // This test verifies that commit string, preedit and delete surrounding text is sent correctly.
    QSignalSpy input_method_spy(server.seat, &Wrapland::Server::Seat::input_method_v2_changed);
    QVERIFY(input_method_spy.isValid());

    auto input_method = std::unique_ptr<Wrapland::Client::input_method_v2>(get_input_method());
    QVERIFY(input_method);
    QVERIFY(input_method_spy.wait());

    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    QSignalSpy done_spy(input_method.get(), &Wrapland::Client::input_method_v2::done);
    QVERIFY(done_spy.isValid());

    QCOMPARE(input_method->state().content.purpose, client_purpose::normal);

    // Now activate and send surrounding text to indicate support for it.
    server_input_method->set_active(true);
    server_input_method->set_surrounding_text(
        "", 0, 0, Wrapland::Server::text_input_v3_change_cause::other);
    server_input_method->done();

    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, true);
    QVERIFY(input_method->state().surrounding_text.data.empty());
    QCOMPARE(input_method->state().surrounding_text.cursor_position, 0);
    QCOMPARE(input_method->state().surrounding_text.selection_anchor, 0);
    QCOMPARE(input_method->state().surrounding_text.change_cause,
             Wrapland::Client::text_input_v3_change_cause::other);

    QSignalSpy commit_spy(server_input_method, &Wrapland::Server::input_method_v2::state_committed);
    QVERIFY(commit_spy.isValid());

    // Now send data.
    auto commit_text = "commit string text";
    auto preedit_text = "preedit string text";
    input_method->commit_string(commit_text);
    input_method->set_preedit_string(preedit_text, 1, 2);
    input_method->commit();

    QCOMPARE(server_input_method->state().preedit_string.update, false);
    QCOMPARE(server_input_method->state().preedit_string.data, "");
    QCOMPARE(server_input_method->state().preedit_string.cursor_begin, 0);
    QCOMPARE(server_input_method->state().preedit_string.cursor_end, 0);
    QCOMPARE(server_input_method->state().commit_string.update, false);
    QCOMPARE(server_input_method->state().commit_string.data, "");
    QCOMPARE(server_input_method->state().delete_surrounding_text.update, false);
    QCOMPARE(server_input_method->state().delete_surrounding_text.before_length, 0);
    QCOMPARE(server_input_method->state().delete_surrounding_text.after_length, 0);

    QVERIFY(commit_spy.wait());
    QCOMPARE(server_input_method->state().preedit_string.update, true);
    QCOMPARE(server_input_method->state().preedit_string.data, preedit_text);
    QCOMPARE(server_input_method->state().preedit_string.cursor_begin, 1);
    QCOMPARE(server_input_method->state().preedit_string.cursor_end, 2);
    QCOMPARE(server_input_method->state().commit_string.update, true);
    QCOMPARE(server_input_method->state().commit_string.data, commit_text);
    QCOMPARE(server_input_method->state().delete_surrounding_text.update, false);
    QCOMPARE(server_input_method->state().delete_surrounding_text.before_length, 0);
    QCOMPARE(server_input_method->state().delete_surrounding_text.after_length, 0);

    // Now deactivate.
    server_input_method->set_active(false);
    server_input_method->done();

    // Not active anymore but data is still available.
    QVERIFY(done_spy.wait());
    QCOMPARE(server_input_method->state().preedit_string.update, true);
    QCOMPARE(server_input_method->state().preedit_string.data, preedit_text);
    QCOMPARE(server_input_method->state().preedit_string.cursor_begin, 1);
    QCOMPARE(server_input_method->state().preedit_string.cursor_end, 2);
    QCOMPARE(server_input_method->state().commit_string.update, true);
    QCOMPARE(server_input_method->state().commit_string.data, commit_text);
    QCOMPARE(server_input_method->state().delete_surrounding_text.update, false);
    QCOMPARE(server_input_method->state().delete_surrounding_text.before_length, 0);
    QCOMPARE(server_input_method->state().delete_surrounding_text.after_length, 0);

    // Activate again. Data is reset immediately.
    server_input_method->set_active(true);
    QCOMPARE(server_input_method->state().preedit_string.update, false);
    QCOMPARE(server_input_method->state().preedit_string.data, "");
    QCOMPARE(server_input_method->state().preedit_string.cursor_begin, 0);
    QCOMPARE(server_input_method->state().preedit_string.cursor_end, 0);
    QCOMPARE(server_input_method->state().commit_string.update, false);
    QCOMPARE(server_input_method->state().commit_string.data, "");
    QCOMPARE(server_input_method->state().delete_surrounding_text.update, false);
    QCOMPARE(server_input_method->state().delete_surrounding_text.before_length, 0);
    QCOMPARE(server_input_method->state().delete_surrounding_text.after_length, 0);

    // Set surrounding text for testing deletion of surrounding text.
    auto const surrounding_text = "100 €, 100 $";

    server_input_method->set_active(true);
    server_input_method->set_surrounding_text(
        surrounding_text, 5, 6, Wrapland::Server::text_input_v3_change_cause::input_method);
    server_input_method->done();

    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().active, true);
    QCOMPARE(input_method->state().surrounding_text.data, surrounding_text);
    QCOMPARE(input_method->state().surrounding_text.cursor_position, 5);
    QCOMPARE(input_method->state().surrounding_text.selection_anchor, 6);
    QCOMPARE(input_method->state().surrounding_text.change_cause,
             Wrapland::Client::text_input_v3_change_cause::input_method);

    input_method->delete_surrounding_text(1, 2);
    input_method->commit();

    QVERIFY(commit_spy.wait());
    QCOMPARE(server_input_method->state().preedit_string.update, false);
    QCOMPARE(server_input_method->state().preedit_string.data, "");
    QCOMPARE(server_input_method->state().preedit_string.cursor_begin, 0);
    QCOMPARE(server_input_method->state().preedit_string.cursor_end, 0);
    QCOMPARE(server_input_method->state().commit_string.update, false);
    QCOMPARE(server_input_method->state().commit_string.data, "");
    QCOMPARE(server_input_method->state().delete_surrounding_text.update, true);
    QCOMPARE(server_input_method->state().delete_surrounding_text.before_length, 1);
    QCOMPARE(server_input_method->state().delete_surrounding_text.after_length, 2);
}

void input_method_v2_test::test_popup_surface()
{
    // This test verifies that the popup surface works as expected.
    auto surface = std::unique_ptr<Wrapland::Client::Surface>(client1.compositor->createSurface());
    QSignalSpy input_method_spy(server.seat, &Wrapland::Server::Seat::input_method_v2_changed);
    QVERIFY(input_method_spy.isValid());

    auto input_method = std::unique_ptr<Wrapland::Client::input_method_v2>(get_input_method());
    QVERIFY(input_method);
    QVERIFY(input_method_spy.wait());

    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    QSignalSpy popup_spy(server_input_method,
                         &Wrapland::Server::input_method_v2::popup_surface_created);
    QVERIFY(popup_spy.isValid());

    auto popup = std::unique_ptr<Wrapland::Client::input_popup_surface_v2>(
        input_method->get_input_popup_surface(surface.get()));

    QVERIFY(popup_spy.wait());
    auto server_popup
        = popup_spy.first().first().value<Wrapland::Server::input_method_popup_surface_v2*>();

    QSignalSpy rect_spy(popup.get(),
                        &Wrapland::Client::input_popup_surface_v2::text_input_rectangle_changed);
    QVERIFY(rect_spy.isValid());

    auto rect = QRect(1, 2, 3, 4);
    server_popup->set_text_input_rectangle(rect);
    QVERIFY(rect_spy.wait());

    QCOMPARE(popup->text_input_rectangle(), rect);
}

void input_method_v2_test::test_keyboard_grab()
{
    // This test verifies that a keyboard grab works as expected.
    auto surface = std::unique_ptr<Wrapland::Client::Surface>(client1.compositor->createSurface());
    QSignalSpy input_method_spy(server.seat, &Wrapland::Server::Seat::input_method_v2_changed);
    QVERIFY(input_method_spy.isValid());

    auto input_method = std::unique_ptr<Wrapland::Client::input_method_v2>(get_input_method());
    QVERIFY(input_method);
    QVERIFY(input_method_spy.wait());

    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    QSignalSpy grab_spy(server_input_method, &Wrapland::Server::input_method_v2::keyboard_grabbed);
    QVERIFY(grab_spy.isValid());

    auto grab = std::unique_ptr<Wrapland::Client::input_method_keyboard_grab_v2>(
        input_method->grab_keyboard());

    QVERIFY(grab_spy.wait());
    auto server_grab
        = grab_spy.first().first().value<Wrapland::Server::input_method_keyboard_grab_v2*>();

    QSignalSpy keymap_spy(grab.get(),
                          &Wrapland::Client::input_method_keyboard_grab_v2::keymap_changed);
    QVERIFY(keymap_spy.isValid());

    server_grab->set_keymap("foo");
    QVERIFY(keymap_spy.wait());

    auto fd = keymap_spy.first().first().toInt();
    QVERIFY(fd != -1);
    QCOMPARE(keymap_spy.first().last().value<quint32>(), 3u);

    QFile file;
    QVERIFY(file.open(fd, QIODevice::ReadOnly));
    auto address = reinterpret_cast<char*>(file.map(0, keymap_spy.first().last().value<quint32>()));
    QVERIFY(address);
    QCOMPARE(qstrcmp(address, "foo"), 0);
    file.close();

    QSignalSpy key_spy(grab.get(), &Wrapland::Client::input_method_keyboard_grab_v2::key_changed);
    QVERIFY(key_spy.isValid());

    server_grab->press_key(1, KEY_K);
    QVERIFY(key_spy.wait());

    QCOMPARE(key_spy.first().at(0).value<uint32_t>(), KEY_K);
    QCOMPARE(key_spy.first().at(1).value<Wrapland::Client::Keyboard::KeyState>(),
             Wrapland::Client::Keyboard::KeyState::Pressed);
    QCOMPARE(key_spy.first().at(2).value<uint32_t>(), 1);

    QSignalSpy modifiers_spy(grab.get(),
                             &Wrapland::Client::input_method_keyboard_grab_v2::modifiers_changed);
    QVERIFY(modifiers_spy.isValid());

    server_grab->update_modifiers(1, 2, 3, 4);
    QVERIFY(modifiers_spy.wait());

    QCOMPARE(modifiers_spy.first().at(0).value<uint32_t>(), 1);
    QCOMPARE(modifiers_spy.first().at(1).value<uint32_t>(), 2);
    QCOMPARE(modifiers_spy.first().at(2).value<uint32_t>(), 3);
    QCOMPARE(modifiers_spy.first().at(3).value<uint32_t>(), 4);

    QSignalSpy repeat_spy(grab.get(),
                          &Wrapland::Client::input_method_keyboard_grab_v2::repeat_changed);
    QVERIFY(repeat_spy.isValid());

    server_grab->set_repeat_info(1, 2);
    QVERIFY(repeat_spy.wait());

    QCOMPARE(grab->repeat_rate(), 1);
    QCOMPARE(grab->repeat_delay(), 2);
}

QTEST_GUILESS_MAIN(input_method_v2_test)
#include "input_method_v2.moc"
