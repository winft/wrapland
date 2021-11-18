/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"
#include "../../src/client/text_input_v2.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/surface.h"
#include "../../server/text_input_pool.h"
#include "../../server/text_input_v2.h"

class text_input_v2_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_enter_leave_data();
    void test_enter_leave();
    void test_show_hide_panel();
    void test_cursor_rectangle();
    void test_preferred_language();
    void test_reset();
    void test_surrounding_text();
    void test_content_hints_data();
    void test_content_hints();
    void test_content_purpose_data();
    void test_content_purpose();
    void test_text_direction_data();
    void test_text_direction();
    void test_language();
    void test_key_event();
    void test_preedit();
    void test_commit();

private:
    Wrapland::Server::Surface* waitForSurface();
    Wrapland::Client::TextInputV2* createTextInput();

    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    QThread* m_thread = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::Keyboard* m_keyboard = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::TextInputManagerV2* m_textInputManagerV2 = nullptr;
};

constexpr auto socket_name{"wrapland-test-text-input-v2-0"};

void text_input_v2_test::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.compositor = server.display->createCompositor();

    server.globals.seats.push_back(server.display->createSeat());
    server.seat = server.globals.seats.back().get();
    server.seat->setHasKeyboard(true);
    server.seat->setHasTouch(true);

    server.globals.text_input_manager_v2 = server.display->createTextInputManagerV2();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Wrapland::Client::EventQueue(this);
    m_queue->setup(m_connection);

    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_seat = registry.createSeat(
        registry.interface(Wrapland::Client::Registry::Interface::Seat).name,
        registry.interface(Wrapland::Client::Registry::Interface::Seat).version,
        this);
    QVERIFY(m_seat->isValid());
    QSignalSpy hasKeyboardSpy(m_seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    QVERIFY(hasKeyboardSpy.isValid());
    QVERIFY(hasKeyboardSpy.wait());
    m_keyboard = m_seat->createKeyboard(this);
    QVERIFY(m_keyboard->isValid());

    m_compositor = registry.createCompositor(
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).name,
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).version,
        this);
    QVERIFY(m_compositor->isValid());

    m_textInputManagerV2 = registry.createTextInputManagerV2(
        registry.interface(Wrapland::Client::Registry::Interface::TextInputManagerV2).name,
        registry.interface(Wrapland::Client::Registry::Interface::TextInputManagerV2).version,
        this);
    QVERIFY(m_textInputManagerV2->isValid());
}

void text_input_v2_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_textInputManagerV2)
    CLEANUP(m_keyboard)
    CLEANUP(m_seat)
    CLEANUP(m_compositor)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
#undef CLEANUP

    server = {};
}

Wrapland::Server::Surface* text_input_v2_test::waitForSurface()
{
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    if (!surfaceCreatedSpy.isValid()) {
        return nullptr;
    }
    if (!surfaceCreatedSpy.wait(500)) {
        return nullptr;
    }
    if (surfaceCreatedSpy.count() != 1) {
        return nullptr;
    }
    return surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
}

Wrapland::Client::TextInputV2* text_input_v2_test::createTextInput()
{
    return m_textInputManagerV2->createTextInput(m_seat);
}

void text_input_v2_test::test_enter_leave_data()
{
    QTest::addColumn<bool>("updatesDirectly");
    QTest::newRow("UnstableV2") << true;
}

void text_input_v2_test::test_enter_leave()
{
    // this test verifies that enter leave are sent correctly
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QVERIFY(textInput != nullptr);
    QSignalSpy enteredSpy(textInput.get(), &Wrapland::Client::TextInputV2::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(textInput.get(), &Wrapland::Client::TextInputV2::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy textInputChangedSpy(server.seat, &Wrapland::Server::Seat::focusedTextInputChanged);
    QVERIFY(textInputChangedSpy.isValid());

    // now let's try to enter it
    auto& server_text_inputs = server.seat->text_inputs();
    QVERIFY(!server_text_inputs.v2.text_input);
    QVERIFY(!server_text_inputs.focus.surface);
    server.seat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(server_text_inputs.focus.surface, serverSurface);

    // text input not yet set for the surface
    QFETCH(bool, updatesDirectly);
    QCOMPARE(bool(server_text_inputs.v2.text_input), updatesDirectly);
    QCOMPARE(textInputChangedSpy.isEmpty(), !updatesDirectly);
    textInput->enable(surface.get());

    // this should trigger on server side
    if (!updatesDirectly) {
        QVERIFY(textInputChangedSpy.wait());
    }
    QCOMPARE(textInputChangedSpy.count(), 1);
    auto serverTextInput = server_text_inputs.v2.text_input;
    QVERIFY(serverTextInput);

    QSignalSpy enabledChangedSpy(server.seat,
                                 &Wrapland::Server::Seat::text_input_v2_enabled_changed);
    QVERIFY(enabledChangedSpy.isValid());
    if (updatesDirectly) {
        QVERIFY(enabledChangedSpy.wait());
        enabledChangedSpy.clear();
    }
    QCOMPARE(serverTextInput->surface().data(), serverSurface);
    QVERIFY(serverTextInput->state().enabled);

    // and trigger an enter
    if (enteredSpy.isEmpty()) {
        QVERIFY(enteredSpy.wait());
    }
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(textInput->enteredSurface(), surface.get());

    // now trigger a leave
    server.seat->setFocusedKeyboardSurface(nullptr);
    QCOMPARE(textInputChangedSpy.count(), 2);
    QVERIFY(!server_text_inputs.v2.text_input);
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
    QVERIFY(serverTextInput->state().enabled);

    // if we enter again we should directly get the text input as it's still activated
    server.seat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(textInputChangedSpy.count(), 3);
    QVERIFY(server_text_inputs.v2.text_input);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(textInput->enteredSurface(), surface.get());
    QVERIFY(serverTextInput->state().enabled);

    // let's deactivate on client side
    textInput->disable(surface.get());
    QVERIFY(enabledChangedSpy.wait());
    QCOMPARE(enabledChangedSpy.count(), 1);
    QVERIFY(!serverTextInput->state().enabled);
    // does not trigger a leave
    QCOMPARE(textInputChangedSpy.count(), 3);
    // should still be the same text input
    QCOMPARE(server_text_inputs.v2.text_input, serverTextInput);
    // reset
    textInput->enable(surface.get());
    QVERIFY(enabledChangedSpy.wait());

    // trigger an enter again and leave, but this
    // time we try sending an event after the surface is unbound
    // but not yet destroyed. It should work without errors
    QCOMPARE(textInput->enteredSurface(), surface.get());
    connect(serverSurface, &Wrapland::Server::Surface::resourceDestroyed, [=]() {
        server.seat->setFocusedKeyboardSurface(nullptr);
    });

    // delete the client and wait for the server to catch up
    QSignalSpy unboundSpy(serverSurface, &QObject::destroyed);
    surface.reset();
    QVERIFY(unboundSpy.wait());
    QVERIFY(leftSpy.count() == 2 || leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
}

void text_input_v2_test::test_show_hide_panel()
{
    // this test verifies that the requests for show/hide panel work
    // and that status is properly sent to the client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);

    QSignalSpy showPanelRequestedSpy(ti,
                                     &Wrapland::Server::text_input_v2::show_input_panel_requested);
    QVERIFY(showPanelRequestedSpy.isValid());
    QSignalSpy hidePanelRequestedSpy(ti,
                                     &Wrapland::Server::text_input_v2::hide_input_panel_requested);
    QVERIFY(hidePanelRequestedSpy.isValid());
    QSignalSpy inputPanelStateChangedSpy(textInput.get(),
                                         &Wrapland::Client::TextInputV2::inputPanelStateChanged);
    QVERIFY(inputPanelStateChangedSpy.isValid());

    QCOMPARE(textInput->isInputPanelVisible(), false);
    textInput->showInputPanel();
    QVERIFY(showPanelRequestedSpy.wait());
    ti->set_input_panel_state(true, QRect(0, 0, 0, 0));
    QVERIFY(inputPanelStateChangedSpy.wait());
    QCOMPARE(textInput->isInputPanelVisible(), true);

    textInput->hideInputPanel();
    QVERIFY(hidePanelRequestedSpy.wait());
    ti->set_input_panel_state(false, QRect(0, 0, 0, 0));
    QVERIFY(inputPanelStateChangedSpy.wait());
    QCOMPARE(textInput->isInputPanelVisible(), false);
}

void text_input_v2_test::test_cursor_rectangle()
{
    // this test verifies that passing the cursor rectangle from client to server works
    // and that setting visibility state from server to client works
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);
    QCOMPARE(ti->state().cursor_rectangle, QRect());
    QSignalSpy cursorRectangleChangedSpy(
        ti, &Wrapland::Server::text_input_v2::cursor_rectangle_changed);
    QVERIFY(cursorRectangleChangedSpy.isValid());

    textInput->setCursorRectangle(QRect(10, 20, 30, 40));
    QVERIFY(cursorRectangleChangedSpy.wait());
    QCOMPARE(ti->state().cursor_rectangle, QRect(10, 20, 30, 40));
}

void text_input_v2_test::test_preferred_language()
{
    // this test verifies that passing the preferred language from client to server works
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);
    QVERIFY(ti->state().preferred_language.empty());

    QSignalSpy preferredLanguageChangedSpy(
        ti, &Wrapland::Server::text_input_v2::preferred_language_changed);
    QVERIFY(preferredLanguageChangedSpy.isValid());
    textInput->setPreferredLanguage(QStringLiteral("foo"));
    QVERIFY(preferredLanguageChangedSpy.wait());
    QCOMPARE(ti->state().preferred_language, "foo");
}

void text_input_v2_test::test_reset()
{
    // this test verifies that the reset request is properly passed from client to server
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);

    QSignalSpy resetRequestedSpy(ti, &Wrapland::Server::text_input_v2::reset_requested);
    QVERIFY(resetRequestedSpy.isValid());

    textInput->reset();
    QVERIFY(resetRequestedSpy.wait());
}

void text_input_v2_test::test_surrounding_text()
{
    // this test verifies that surrounding text is properly passed around
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);
    QVERIFY(ti->state().surrounding_text.data.empty());
    QCOMPARE(ti->state().surrounding_text.cursor_position, 0);
    QCOMPARE(ti->state().surrounding_text.selection_anchor, 0);

    QSignalSpy surroundingTextChangedSpy(
        ti, &Wrapland::Server::text_input_v2::surrounding_text_changed);
    QVERIFY(surroundingTextChangedSpy.isValid());

    textInput->setSurroundingText(QStringLiteral("100 €, 100 $"), 5, 6);
    QVERIFY(surroundingTextChangedSpy.wait());
    QCOMPARE(ti->state().surrounding_text.data, "100 €, 100 $");
    QCOMPARE(ti->state().surrounding_text.cursor_position,
             QStringLiteral("100 €, 100 $").toUtf8().indexOf(','));
    QCOMPARE(ti->state().surrounding_text.selection_anchor,
             QStringLiteral("100 €, 100 $")
                 .toUtf8()
                 .indexOf(' ', ti->state().surrounding_text.cursor_position));
}

void text_input_v2_test::test_content_hints_data()
{
    namespace WC = Wrapland::Client;
    namespace WS = Wrapland::Server;

    QTest::addColumn<WC::TextInputV2::ContentHints>("clientHints");
    QTest::addColumn<WS::text_input_v2_content_hints>("serverHints");

    // same for version 2

    QTest::newRow("completion/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::AutoCompletion)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::completion);
    QTest::newRow("Correction/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::AutoCorrection)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::correction);
    QTest::newRow("Capitalization/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::AutoCapitalization)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::capitalization);
    QTest::newRow("Lowercase/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::LowerCase)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::lowercase);
    QTest::newRow("Uppercase/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::UpperCase)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::uppercase);
    QTest::newRow("Titlecase/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::TitleCase)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::titlecase);
    QTest::newRow("HiddenText/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::HiddenText)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::hidden_text);
    QTest::newRow("SensitiveData/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::SensitiveData)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::sensitive_data);
    QTest::newRow("Latin/v2") << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::Latin)
                              << WS::text_input_v2_content_hints(
                                     WS::text_input_v2_content_hint::latin);
    QTest::newRow("Multiline/v2")
        << WC::TextInputV2::ContentHints(WC::TextInputV2::ContentHint::MultiLine)
        << WS::text_input_v2_content_hints(WS::text_input_v2_content_hint::multiline);

    QTest::newRow("autos/v2") << (WC::TextInputV2::ContentHint::AutoCompletion
                                  | WC::TextInputV2::ContentHint::AutoCorrection
                                  | WC::TextInputV2::ContentHint::AutoCapitalization)
                              << (Wrapland::Server::text_input_v2_content_hint::completion
                                  | WS::text_input_v2_content_hint::correction
                                  | WS::text_input_v2_content_hint::capitalization);

    // all has combinations which don't make sense - what's both lowercase and uppercase?
    QTest::newRow("all/v2")
        << (WC::TextInputV2::ContentHint::AutoCompletion
            | WC::TextInputV2::ContentHint::AutoCorrection
            | WC::TextInputV2::ContentHint::AutoCapitalization
            | WC::TextInputV2::ContentHint::LowerCase | WC::TextInputV2::ContentHint::UpperCase
            | WC::TextInputV2::ContentHint::TitleCase | WC::TextInputV2::ContentHint::HiddenText
            | WC::TextInputV2::ContentHint::SensitiveData | WC::TextInputV2::ContentHint::Latin
            | WC::TextInputV2::ContentHint::MultiLine)
        << (Wrapland::Server::text_input_v2_content_hint::completion
            | WS::text_input_v2_content_hint::correction
            | WS::text_input_v2_content_hint::capitalization
            | WS::text_input_v2_content_hint::lowercase | WS::text_input_v2_content_hint::uppercase
            | WS::text_input_v2_content_hint::titlecase
            | WS::text_input_v2_content_hint::hidden_text
            | WS::text_input_v2_content_hint::sensitive_data | WS::text_input_v2_content_hint::latin
            | WS::text_input_v2_content_hint::multiline);
}

void text_input_v2_test::test_content_hints()
{
    // this test verifies that content hints are properly passed from client to server
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);
    QCOMPARE(ti->state().content.hints, Wrapland::Server::text_input_v2_content_hints());

    QSignalSpy contentTypeChangedSpy(ti, &Wrapland::Server::text_input_v2::content_type_changed);
    QVERIFY(contentTypeChangedSpy.isValid());
    QFETCH(Wrapland::Client::TextInputV2::ContentHints, clientHints);
    textInput->setContentType(clientHints, Wrapland::Client::TextInputV2::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(ti->state().content.hints, "serverHints");

    // setting to same should not trigger an update
    textInput->setContentType(clientHints, Wrapland::Client::TextInputV2::ContentPurpose::Normal);
    QVERIFY(!contentTypeChangedSpy.wait(100));

    // unsetting should work
    textInput->setContentType(Wrapland::Client::TextInputV2::ContentHints(),
                              Wrapland::Client::TextInputV2::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(ti->state().content.hints, Wrapland::Server::text_input_v2_content_hints());
}

void text_input_v2_test::test_content_purpose_data()
{
    namespace WC = Wrapland::Client;
    namespace WS = Wrapland::Server;

    QTest::addColumn<WC::TextInputV2::ContentPurpose>("clientPurpose");
    QTest::addColumn<WS::text_input_v2_content_purpose>("serverPurpose");

    QTest::newRow("Alpha/v2") << WC::TextInputV2::ContentPurpose::Alpha
                              << WS::text_input_v2_content_purpose::alpha;
    QTest::newRow("Digits/v2") << WC::TextInputV2::ContentPurpose::Digits
                               << WS::text_input_v2_content_purpose::digits;
    QTest::newRow("Number/v2") << WC::TextInputV2::ContentPurpose::Number
                               << WS::text_input_v2_content_purpose::number;
    QTest::newRow("Phone/v2") << WC::TextInputV2::ContentPurpose::Phone
                              << WS::text_input_v2_content_purpose::phone;
    QTest::newRow("Url/v2") << WC::TextInputV2::ContentPurpose::Url
                            << WS::text_input_v2_content_purpose::url;
    QTest::newRow("Email/v2") << WC::TextInputV2::ContentPurpose::Email
                              << WS::text_input_v2_content_purpose::email;
    QTest::newRow("Name/v2") << WC::TextInputV2::ContentPurpose::Name
                             << WS::text_input_v2_content_purpose::name;
    QTest::newRow("Password/v2") << WC::TextInputV2::ContentPurpose::Password
                                 << WS::text_input_v2_content_purpose::password;
    QTest::newRow("Date/v2") << WC::TextInputV2::ContentPurpose::Date
                             << WS::text_input_v2_content_purpose::date;
    QTest::newRow("Time/v2") << WC::TextInputV2::ContentPurpose::Time
                             << WS::text_input_v2_content_purpose::time;
    QTest::newRow("Datetime/v2") << WC::TextInputV2::ContentPurpose::DateTime
                                 << WS::text_input_v2_content_purpose::datetime;
    QTest::newRow("Terminal/v2") << WC::TextInputV2::ContentPurpose::Terminal
                                 << WS::text_input_v2_content_purpose::terminal;
}

void text_input_v2_test::test_content_purpose()
{
    // this test verifies that content purpose are properly passed from client to server
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);
    QCOMPARE(ti->state().content.purpose, Wrapland::Server::text_input_v2_content_purpose::normal);

    QSignalSpy contentTypeChangedSpy(ti, &Wrapland::Server::text_input_v2::content_type_changed);
    QVERIFY(contentTypeChangedSpy.isValid());
    QFETCH(Wrapland::Client::TextInputV2::ContentPurpose, clientPurpose);
    textInput->setContentType(Wrapland::Client::TextInputV2::ContentHints(), clientPurpose);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(ti->state().content.purpose, "serverPurpose");

    // setting to same should not trigger an update
    textInput->setContentType(Wrapland::Client::TextInputV2::ContentHints(), clientPurpose);
    QVERIFY(!contentTypeChangedSpy.wait(100));

    // unsetting should work
    textInput->setContentType(Wrapland::Client::TextInputV2::ContentHints(),
                              Wrapland::Client::TextInputV2::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(ti->state().content.purpose, Wrapland::Server::text_input_v2_content_purpose::normal);
}

void text_input_v2_test::test_text_direction_data()
{
    QTest::addColumn<Qt::LayoutDirection>("textDirection");

    QTest::newRow("ltr/v2") << Qt::LeftToRight;
    QTest::newRow("rtl/v2") << Qt::RightToLeft;
}

void text_input_v2_test::test_text_direction()
{
    // this test verifies that the text direction is sent from server to client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // default should be auto
    QCOMPARE(textInput->textDirection(), Qt::LayoutDirectionAuto);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);

    // let's send the new text direction
    QSignalSpy textDirectionChangedSpy(textInput.get(),
                                       &Wrapland::Client::TextInputV2::textDirectionChanged);
    QVERIFY(textDirectionChangedSpy.isValid());
    QFETCH(Qt::LayoutDirection, textDirection);
    ti->set_text_direction(textDirection);
    QVERIFY(textDirectionChangedSpy.wait());
    QCOMPARE(textInput->textDirection(), textDirection);
    // setting again should not change
    ti->set_text_direction(textDirection);
    QVERIFY(!textDirectionChangedSpy.wait(100));

    // setting back to auto
    ti->set_text_direction(Qt::LayoutDirectionAuto);
    QVERIFY(textDirectionChangedSpy.wait());
    QCOMPARE(textInput->textDirection(), Qt::LayoutDirectionAuto);
}

void text_input_v2_test::test_language()
{
    // this test verifies that language is sent from server to client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // default should be empty
    QVERIFY(textInput->language().isEmpty());
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);

    // let's send the new language
    QSignalSpy langugageChangedSpy(textInput.get(),
                                   &Wrapland::Client::TextInputV2::languageChanged);
    QVERIFY(langugageChangedSpy.isValid());
    ti->set_language("foo");
    QVERIFY(langugageChangedSpy.wait());
    QCOMPARE(textInput->language(), QByteArrayLiteral("foo"));

    // setting to same should not trigger
    ti->set_language("foo");
    QVERIFY(!langugageChangedSpy.wait(100));

    // but to something else should trigger again
    ti->set_language("bar");
    QVERIFY(langugageChangedSpy.wait());
    QCOMPARE(textInput->language(), QByteArrayLiteral("bar"));
}

void text_input_v2_test::test_key_event()
{
    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<Wrapland::Client::TextInputV2::KeyState>();
    // this test verifies that key events are properly sent to the client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);

    // TODO: test modifiers
    QSignalSpy keyEventSpy(textInput.get(), &Wrapland::Client::TextInputV2::keyEvent);
    QVERIFY(keyEventSpy.isValid());
    server.seat->setTimestamp(100);
    ti->keysym_pressed(2);
    QVERIFY(keyEventSpy.wait());
    QCOMPARE(keyEventSpy.count(), 1);
    QCOMPARE(keyEventSpy.last().at(0).value<quint32>(), 2u);
    QCOMPARE(keyEventSpy.last().at(1).value<Wrapland::Client::TextInputV2::KeyState>(),
             Wrapland::Client::TextInputV2::KeyState::Pressed);
    QCOMPARE(keyEventSpy.last().at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers());
    QCOMPARE(keyEventSpy.last().at(3).value<quint32>(), 100u);
    server.seat->setTimestamp(101);
    ti->keysym_released(2);
    QVERIFY(keyEventSpy.wait());
    QCOMPARE(keyEventSpy.count(), 2);
    QCOMPARE(keyEventSpy.last().at(0).value<quint32>(), 2u);
    QCOMPARE(keyEventSpy.last().at(1).value<Wrapland::Client::TextInputV2::KeyState>(),
             Wrapland::Client::TextInputV2::KeyState::Released);
    QCOMPARE(keyEventSpy.last().at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers());
    QCOMPARE(keyEventSpy.last().at(3).value<quint32>(), 101u);
}

void text_input_v2_test::test_preedit()
{
    // this test verifies that pre-edit is correctly passed to the client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // verify default values
    QVERIFY(textInput->composingText().isEmpty());
    QVERIFY(textInput->composingFallbackText().isEmpty());
    QCOMPARE(textInput->composingTextCursorPosition(), 0);

    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);

    // now let's pass through some pre-edit events
    QSignalSpy composingTextChangedSpy(textInput.get(),
                                       &Wrapland::Client::TextInputV2::composingTextChanged);
    QVERIFY(composingTextChangedSpy.isValid());
    ti->set_preedit_cursor(1);
    ti->set_preedit_string("foo", "bar");
    QVERIFY(composingTextChangedSpy.wait());
    QCOMPARE(composingTextChangedSpy.count(), 1);
    QCOMPARE(textInput->composingText(), QByteArrayLiteral("foo"));
    QCOMPARE(textInput->composingFallbackText(), QByteArrayLiteral("bar"));
    QCOMPARE(textInput->composingTextCursorPosition(), 1);

    // when no pre edit cursor is sent, it's at end of text
    ti->set_preedit_string("foobar", {});
    QVERIFY(composingTextChangedSpy.wait());
    QCOMPARE(composingTextChangedSpy.count(), 2);
    QCOMPARE(textInput->composingText(), QByteArrayLiteral("foobar"));
    QCOMPARE(textInput->composingFallbackText(), QByteArray());
    QCOMPARE(textInput->composingTextCursorPosition(), 6);
}

void text_input_v2_test::test_commit()
{
    // this test verifies that the commit is handled correctly by the client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInputV2> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // verify default values
    QCOMPARE(textInput->commitText(), QByteArray());
    QCOMPARE(textInput->cursorPosition(), 0);
    QCOMPARE(textInput->anchorPosition(), 0);
    QCOMPARE(textInput->deleteSurroundingText().beforeLength, 0u);
    QCOMPARE(textInput->deleteSurroundingText().afterLength, 0u);

    textInput->enable(surface.get());
    m_connection->flush();
    server.display->dispatchEvents();

    server.seat->setFocusedKeyboardSurface(serverSurface);
    auto ti = server.seat->text_inputs().v2.text_input;
    QVERIFY(ti);

    // now let's commit
    QSignalSpy committedSpy(textInput.get(), &Wrapland::Client::TextInputV2::committed);
    QVERIFY(committedSpy.isValid());
    ti->set_cursor_position(3, 4);
    ti->delete_surrounding_text(2, 1);
    ti->commit("foo");

    QVERIFY(committedSpy.wait());
    QCOMPARE(textInput->commitText(), QByteArrayLiteral("foo"));
    QCOMPARE(textInput->cursorPosition(), 3);
    QCOMPARE(textInput->anchorPosition(), 4);
    QCOMPARE(textInput->deleteSurroundingText().beforeLength, 2u);
    QCOMPARE(textInput->deleteSurroundingText().afterLength, 1u);
}

QTEST_GUILESS_MAIN(text_input_v2_test)
#include "text_input_v2.moc"
