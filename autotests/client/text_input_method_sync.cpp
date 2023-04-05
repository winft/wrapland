/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>
#include <qobject.h>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/input_method_v2.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"
#include "../../src/client/text_input_v3.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/input_method_v2.h"
#include "../../server/seat.h"
#include "../../server/surface.h"
#include "../../server/text_input_pool.h"

#include "../../tests/globals.h"

class text_input_method_sync_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_sync_text_input();
    void test_sync_input_method();

private:
    Wrapland::Server::Surface* wait_for_surface() const;
    Wrapland::Client::input_method_v2* get_input_method() const;
    Wrapland::Client::text_input_v3* create_text_input() const;

    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

    struct {
        Wrapland::Client::ConnectionThread* connection{nullptr};
        Wrapland::Client::EventQueue* queue{nullptr};
        Wrapland::Client::Registry* registry{nullptr};
        Wrapland::Client::Compositor* compositor{nullptr};
        Wrapland::Client::Seat* seat{nullptr};
        Wrapland::Client::text_input_manager_v3* text_input{nullptr};
        QThread* thread{nullptr};
    } ti_client;

    struct {
        Wrapland::Client::ConnectionThread* connection{nullptr};
        Wrapland::Client::EventQueue* queue{nullptr};
        Wrapland::Client::Registry* registry{nullptr};
        Wrapland::Client::Compositor* compositor{nullptr};
        Wrapland::Client::Seat* seat{nullptr};
        Wrapland::Client::input_method_manager_v2* input_method{nullptr};
        QThread* thread{nullptr};
    } im_client;

    std::unique_ptr<Wrapland::Client::Surface> surface;
    std::unique_ptr<Wrapland::Client::text_input_v3> text_input;
    std::unique_ptr<Wrapland::Client::input_method_v2> input_method;
};

constexpr auto socket_name{"wrapland-test-text-input-v3-0"};

template<typename Server>
void setup_server(Server& server)
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.seats.emplace_back(
        std::make_unique<Wrapland::Server::Seat>(server.display.get()));
    server.seat = server.globals.seats.back().get();
    server.seat->setHasKeyboard(true);

    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());
    server.globals.text_input_manager_v3
        = std::make_unique<Wrapland::Server::text_input_manager_v3>(server.display.get());
    server.globals.input_method_manager_v2
        = std::make_unique<Wrapland::Server::input_method_manager_v2>(server.display.get());
}

template<typename Client>
void setup_client(Client& client, QObject* parent = nullptr)
{
    client.connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(client.connection,
                            &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    client.connection->setSocketName(socket_name);

    client.thread = new QThread(parent);
    client.connection->moveToThread(client.thread);
    client.thread->start();

    client.connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    client.queue = new Wrapland::Client::EventQueue(parent);
    client.queue->setup(client.connection);

    client.registry = new Wrapland::Client::Registry;
    QSignalSpy interfacesAnnouncedSpy(client.registry,
                                      &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    client.registry->setEventQueue(client.queue);
    client.registry->create(client.connection);
    QVERIFY(client.registry->isValid());
    client.registry->setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    client.seat = client.registry->createSeat(
        client.registry->interface(Wrapland::Client::Registry::Interface::Seat).name,
        client.registry->interface(Wrapland::Client::Registry::Interface::Seat).version,
        parent);
    QVERIFY(client.seat->isValid());

    client.compositor = client.registry->createCompositor(
        client.registry->interface(Wrapland::Client::Registry::Interface::Compositor).name,
        client.registry->interface(Wrapland::Client::Registry::Interface::Compositor).version,
        parent);
    QVERIFY(client.compositor->isValid());
}

Wrapland::Server::Surface* text_input_method_sync_test::wait_for_surface() const
{
    auto surface_spy = QSignalSpy(server.globals.compositor.get(),
                                  &Wrapland::Server::Compositor::surfaceCreated);
    if (surface_spy.isValid() && surface_spy.wait() && surface_spy.count() == 1) {
        return surface_spy.first().first().value<Wrapland::Server::Surface*>();
    }
    return nullptr;
}

Wrapland::Client::input_method_v2* text_input_method_sync_test::get_input_method() const
{
    return im_client.input_method->get_input_method(im_client.seat);
}

Wrapland::Client::text_input_v3* text_input_method_sync_test::create_text_input() const
{
    return ti_client.text_input->get_text_input(ti_client.seat);
}

void text_input_method_sync_test::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();
    setup_server(server);

    setup_client(ti_client, this);
    ti_client.text_input = ti_client.registry->createTextInputManagerV3(
        ti_client.registry->interface(Wrapland::Client::Registry::Interface::TextInputManagerV3)
            .name,
        ti_client.registry->interface(Wrapland::Client::Registry::Interface::TextInputManagerV3)
            .version,
        this);
    QVERIFY(ti_client.text_input->isValid());

    setup_client(im_client, this);
    im_client.input_method = im_client.registry->createInputMethodManagerV2(
        im_client.registry->interface(Wrapland::Client::Registry::Interface::InputMethodManagerV2)
            .name,
        im_client.registry->interface(Wrapland::Client::Registry::Interface::InputMethodManagerV2)
            .version,
        this);
    QVERIFY(im_client.input_method->isValid());

    QSignalSpy input_method_spy(server.seat, &Wrapland::Server::Seat::input_method_v2_changed);
    QVERIFY(input_method_spy.isValid());

    input_method.reset(get_input_method());
    QVERIFY(input_method);
    QVERIFY(input_method_spy.wait());

    surface.reset(ti_client.compositor->createSurface());
    text_input.reset(create_text_input());
    QVERIFY(text_input);

    auto server_surface = wait_for_surface();
    QVERIFY(server_surface);

    QSignalSpy entered_spy(text_input.get(), &Wrapland::Client::text_input_v3::entered);
    QVERIFY(entered_spy.isValid());

    server.seat->setFocusedKeyboardSurface(server_surface);
    QCOMPARE(server.seat->text_inputs().focus.surface, server_surface);

    QVERIFY(entered_spy.wait());
    QCOMPARE(entered_spy.count(), 1);
    QCOMPARE(text_input->entered_surface(), surface.get());
}

void text_input_method_sync_test::cleanup()
{
    text_input.reset();
    input_method.reset();
    surface.reset();

#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete (variable);                                                                         \
        (variable) = nullptr;                                                                      \
    }

    CLEANUP(ti_client.text_input)
    CLEANUP(ti_client.seat)
    CLEANUP(ti_client.compositor)
    CLEANUP(ti_client.registry)
    CLEANUP(ti_client.queue)
    if (ti_client.connection) {
        ti_client.connection->deleteLater();
        ti_client.connection = nullptr;
    }
    if (ti_client.thread) {
        ti_client.thread->quit();
        ti_client.thread->wait();
        delete ti_client.thread;
        ti_client.thread = nullptr;
    }

    CLEANUP(im_client.input_method)
    CLEANUP(im_client.seat)
    CLEANUP(im_client.compositor)
    CLEANUP(im_client.registry)
    CLEANUP(im_client.queue)
    if (im_client.connection) {
        im_client.connection->deleteLater();
        im_client.connection = nullptr;
    }
    if (im_client.thread) {
        im_client.thread->quit();
        im_client.thread->wait();
        delete im_client.thread;
        im_client.thread = nullptr;
    }
#undef CLEANUP

    server = {};
}

/*
 * Verify that text input reacts to input method changes.
 *
 * Send a simple request from an input method client, eg, send a commit string.
 * A text input event should be triggered.
 */
void text_input_method_sync_test::test_sync_text_input()
{
    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);

    QSignalSpy commit_spy(server_input_method, &Wrapland::Server::input_method_v2::state_committed);
    QVERIFY(commit_spy.isValid());

    QSignalSpy done_spy(text_input.get(), &Wrapland::Client::text_input_v3::done);
    QVERIFY(done_spy.isValid());

    auto commit_text = "commit string text";
    auto preedit_text = "preedit string text";
    input_method->commit_string(commit_text);
    input_method->set_preedit_string(preedit_text, 1, 2);
    input_method->commit();

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

    QVERIFY(done_spy.wait());
    QCOMPARE(text_input->state().commit_string.data, commit_text);
    QCOMPARE(text_input->state().preedit_string.data, preedit_text);
    QCOMPARE(text_input->state().preedit_string.cursor_begin, 1);
    QCOMPARE(text_input->state().preedit_string.cursor_end, 2);
}

/*
 * Verify that input method reacts to text input changes.
 *
 * Send a simple request from an input method client, eg, set the surrounding text.
 * An input method event should be triggered.
 */
void text_input_method_sync_test::test_sync_input_method()
{
    auto server_input_method = server.seat->get_input_method_v2();
    QVERIFY(server_input_method);

    auto server_text_input = server.seat->text_inputs().v3.text_input;
    QVERIFY(server_text_input);

    QSignalSpy commit_spy(server_text_input, &Wrapland::Server::text_input_v3::state_committed);
    QVERIFY(commit_spy.isValid());

    QSignalSpy done_spy(input_method.get(), &Wrapland::Client::input_method_v2::done);
    QVERIFY(done_spy.isValid());

    auto const surrounding_text = "surrounding string text";
    uint32_t const cursor = 1;
    uint32_t const anchor = 2;
    auto const change_cause = Wrapland::Client::text_input_v3_change_cause::input_method;
    text_input->set_surrounding_text(surrounding_text, cursor, anchor, change_cause);
    text_input->commit();

    QVERIFY(commit_spy.wait());
    QCOMPARE(server_text_input->state().surrounding_text.data, surrounding_text);
    QCOMPARE(server_text_input->state().surrounding_text.cursor_position, cursor);
    QCOMPARE(server_text_input->state().surrounding_text.selection_anchor, anchor);
    QCOMPARE(server_text_input->state().surrounding_text.change_cause,
             Wrapland::Server::text_input_v3_change_cause::input_method);

    QVERIFY(done_spy.wait());
    QCOMPARE(input_method->state().surrounding_text.data, surrounding_text);
    QCOMPARE(input_method->state().surrounding_text.cursor_position, cursor);
    QCOMPARE(input_method->state().surrounding_text.selection_anchor, anchor);
    QCOMPARE(input_method->state().surrounding_text.change_cause, change_cause);
}

QTEST_GUILESS_MAIN(text_input_method_sync_test)
#include "text_input_method_sync.moc"
