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
#include "../../src/client/textinput.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/seat.h"
#include "../../server/surface.h"
#include "../../server/text_input_v2.h"

class TextInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testEnterLeave_data();
    void testEnterLeave();
    void testShowHidePanel();
    void testCursorRectangle();
    void testPreferredLanguage();
    void testReset();
    void testSurroundingText();
    void testContentHints_data();
    void testContentHints();
    void testContentPurpose_data();
    void testContentPurpose();
    void testTextDirection_data();
    void testTextDirection();
    void testLanguage();
    void testKeyEvent();
    void testPreEdit();
    void testCommit();

private:
    Wrapland::Server::Surface* waitForSurface();
    Wrapland::Client::TextInput* createTextInput();
    Wrapland::Server::Display* m_display = nullptr;
    Wrapland::Server::Seat* m_serverSeat = nullptr;
    Wrapland::Server::Compositor* m_serverCompositor = nullptr;
    Wrapland::Server::TextInputManagerV2* m_textInputManagerV2Interface = nullptr;
    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    QThread* m_thread = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::Keyboard* m_keyboard = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::TextInputManager* m_textInputManagerV0 = nullptr;
    Wrapland::Client::TextInputManager* m_textInputManagerV2 = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-text-input-0");

void TextInputTest::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();
    delete m_display;
    m_display = new Wrapland::Server::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_display->createShm();
    m_serverSeat = m_display->createSeat();
    m_serverSeat->setHasKeyboard(true);
    m_serverSeat->setHasTouch(true);

    m_serverCompositor = m_display->createCompositor();

    m_textInputManagerV2Interface = m_display->createTextInputManagerV2();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

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

    m_textInputManagerV2 = registry.createTextInputManager(
        registry.interface(Wrapland::Client::Registry::Interface::TextInputManagerUnstableV2).name,
        registry.interface(Wrapland::Client::Registry::Interface::TextInputManagerUnstableV2)
            .version,
        this);
    QVERIFY(m_textInputManagerV2->isValid());
}

void TextInputTest::cleanup()
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

    CLEANUP(m_textInputManagerV2Interface)
    CLEANUP(m_serverCompositor)
    CLEANUP(m_serverSeat)
    CLEANUP(m_display)
#undef CLEANUP
}

Wrapland::Server::Surface* TextInputTest::waitForSurface()
{
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
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

Wrapland::Client::TextInput* TextInputTest::createTextInput()
{

    return m_textInputManagerV2->createTextInput(m_seat);
}

void TextInputTest::testEnterLeave_data()
{
    QTest::addColumn<bool>("updatesDirectly");
    QTest::newRow("UnstableV2") << true;
}

void TextInputTest::testEnterLeave()
{
    // this test verifies that enter leave are sent correctly
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QVERIFY(textInput != nullptr);
    QSignalSpy enteredSpy(textInput.get(), &Wrapland::Client::TextInput::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(textInput.get(), &Wrapland::Client::TextInput::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy textInputChangedSpy(m_serverSeat, &Wrapland::Server::Seat::focusedTextInputChanged);
    QVERIFY(textInputChangedSpy.isValid());

    // now let's try to enter it
    QVERIFY(!m_serverSeat->focusedTextInput());
    QVERIFY(!m_serverSeat->focusedTextInputSurface());
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedTextInputSurface(), serverSurface);

    // text input not yet set for the surface
    QFETCH(bool, updatesDirectly);
    QCOMPARE(bool(m_serverSeat->focusedTextInput()), updatesDirectly);
    QCOMPARE(textInputChangedSpy.isEmpty(), !updatesDirectly);
    textInput->enable(surface.get());

    // this should trigger on server side
    if (!updatesDirectly) {
        QVERIFY(textInputChangedSpy.wait());
    }
    QCOMPARE(textInputChangedSpy.count(), 1);
    auto serverTextInput = m_serverSeat->focusedTextInput();
    QVERIFY(serverTextInput);

    QSignalSpy enabledChangedSpy(serverTextInput, &Wrapland::Server::TextInputV2::enabledChanged);
    QVERIFY(enabledChangedSpy.isValid());
    if (updatesDirectly) {
        QVERIFY(enabledChangedSpy.wait());
        enabledChangedSpy.clear();
    }
    QCOMPARE(serverTextInput->surface().data(), serverSurface);
    QVERIFY(serverTextInput->isEnabled());

    // and trigger an enter
    if (enteredSpy.isEmpty()) {
        QVERIFY(enteredSpy.wait());
    }
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(textInput->enteredSurface(), surface.get());

    // now trigger a leave
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    QCOMPARE(textInputChangedSpy.count(), 2);
    QVERIFY(!m_serverSeat->focusedTextInput());
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
    QVERIFY(serverTextInput->isEnabled());

    // if we enter again we should directly get the text input as it's still activated
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(textInputChangedSpy.count(), 3);
    QVERIFY(m_serverSeat->focusedTextInput());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(textInput->enteredSurface(), surface.get());
    QVERIFY(serverTextInput->isEnabled());

    // let's deactivate on client side
    textInput->disable(surface.get());
    QVERIFY(enabledChangedSpy.wait());
    QCOMPARE(enabledChangedSpy.count(), 1);
    QVERIFY(!serverTextInput->isEnabled());
    // does not trigger a leave
    QCOMPARE(textInputChangedSpy.count(), 3);
    // should still be the same text input
    QCOMPARE(m_serverSeat->focusedTextInput(), serverTextInput);
    // reset
    textInput->enable(surface.get());
    QVERIFY(enabledChangedSpy.wait());

    // trigger an enter again and leave, but this
    // time we try sending an event after the surface is unbound
    // but not yet destroyed. It should work without errors
    QCOMPARE(textInput->enteredSurface(), surface.get());
    connect(serverSurface, &Wrapland::Server::Surface::resourceDestroyed, [=]() {
        m_serverSeat->setFocusedKeyboardSurface(nullptr);
    });

    // delete the client and wait for the server to catch up
    QSignalSpy unboundSpy(serverSurface, &QObject::destroyed);
    surface.reset();
    QVERIFY(unboundSpy.wait());
    QVERIFY(leftSpy.count() == 2 || leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
}

void TextInputTest::testShowHidePanel()
{
    // this test verifies that the requests for show/hide panel work
    // and that status is properly sent to the client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);

    QSignalSpy showPanelRequestedSpy(ti, &Wrapland::Server::TextInputV2::requestShowInputPanel);
    QVERIFY(showPanelRequestedSpy.isValid());
    QSignalSpy hidePanelRequestedSpy(ti, &Wrapland::Server::TextInputV2::requestHideInputPanel);
    QVERIFY(hidePanelRequestedSpy.isValid());
    QSignalSpy inputPanelStateChangedSpy(textInput.get(),
                                         &Wrapland::Client::TextInput::inputPanelStateChanged);
    QVERIFY(inputPanelStateChangedSpy.isValid());

    QCOMPARE(textInput->isInputPanelVisible(), false);
    textInput->showInputPanel();
    QVERIFY(showPanelRequestedSpy.wait());
    ti->setInputPanelState(true, QRect(0, 0, 0, 0));
    QVERIFY(inputPanelStateChangedSpy.wait());
    QCOMPARE(textInput->isInputPanelVisible(), true);

    textInput->hideInputPanel();
    QVERIFY(hidePanelRequestedSpy.wait());
    ti->setInputPanelState(false, QRect(0, 0, 0, 0));
    QVERIFY(inputPanelStateChangedSpy.wait());
    QCOMPARE(textInput->isInputPanelVisible(), false);
}

void TextInputTest::testCursorRectangle()
{
    // this test verifies that passing the cursor rectangle from client to server works
    // and that setting visibility state from server to client works
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);
    QCOMPARE(ti->cursorRectangle(), QRect());
    QSignalSpy cursorRectangleChangedSpy(ti,
                                         &Wrapland::Server::TextInputV2::cursorRectangleChanged);
    QVERIFY(cursorRectangleChangedSpy.isValid());

    textInput->setCursorRectangle(QRect(10, 20, 30, 40));
    QVERIFY(cursorRectangleChangedSpy.wait());
    QCOMPARE(ti->cursorRectangle(), QRect(10, 20, 30, 40));
}

void TextInputTest::testPreferredLanguage()
{
    // this test verifies that passing the preferred language from client to server works
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);
    QVERIFY(ti->preferredLanguage().isEmpty());

    QSignalSpy preferredLanguageChangedSpy(
        ti, &Wrapland::Server::TextInputV2::preferredLanguageChanged);
    QVERIFY(preferredLanguageChangedSpy.isValid());
    textInput->setPreferredLanguage(QStringLiteral("foo"));
    QVERIFY(preferredLanguageChangedSpy.wait());
    QCOMPARE(ti->preferredLanguage(), QStringLiteral("foo").toUtf8());
}

void TextInputTest::testReset()
{
    // this test verifies that the reset request is properly passed from client to server
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);

    QSignalSpy resetRequestedSpy(ti, &Wrapland::Server::TextInputV2::requestReset);
    QVERIFY(resetRequestedSpy.isValid());

    textInput->reset();
    QVERIFY(resetRequestedSpy.wait());
}

void TextInputTest::testSurroundingText()
{
    // this test verifies that surrounding text is properly passed around
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);
    QVERIFY(ti->surroundingText().isEmpty());
    QCOMPARE(ti->surroundingTextCursorPosition(), 0);
    QCOMPARE(ti->surroundingTextSelectionAnchor(), 0);

    QSignalSpy surroundingTextChangedSpy(ti,
                                         &Wrapland::Server::TextInputV2::surroundingTextChanged);
    QVERIFY(surroundingTextChangedSpy.isValid());

    textInput->setSurroundingText(QStringLiteral("100 €, 100 $"), 5, 6);
    QVERIFY(surroundingTextChangedSpy.wait());
    QCOMPARE(ti->surroundingText(), QStringLiteral("100 €, 100 $").toUtf8());
    QCOMPARE(ti->surroundingTextCursorPosition(),
             QStringLiteral("100 €, 100 $").toUtf8().indexOf(','));
    QCOMPARE(
        ti->surroundingTextSelectionAnchor(),
        QStringLiteral("100 €, 100 $").toUtf8().indexOf(' ', ti->surroundingTextCursorPosition()));
}

void TextInputTest::testContentHints_data()
{
    QTest::addColumn<Wrapland::Client::TextInput::ContentHints>("clientHints");
    QTest::addColumn<Wrapland::Server::TextInputV2::ContentHints>("serverHints");

    // same for version 2

    QTest::newRow("completion/v2")
        << Wrapland::Client::TextInput::ContentHints(
               Wrapland::Client::TextInput::ContentHint::AutoCompletion)
        << Wrapland::Server::TextInputV2::ContentHints(
               Wrapland::Server::TextInputV2::ContentHint::AutoCompletion);
    QTest::newRow("Correction/v2")
        << Wrapland::Client::TextInput::ContentHints(
               Wrapland::Client::TextInput::ContentHint::AutoCorrection)
        << Wrapland::Server::TextInputV2::ContentHints(
               Wrapland::Server::TextInputV2::ContentHint::AutoCorrection);
    QTest::newRow("Capitalization/v2")
        << Wrapland::Client::TextInput::ContentHints(
               Wrapland::Client::TextInput::ContentHint::AutoCapitalization)
        << Wrapland::Server::TextInputV2::ContentHints(
               Wrapland::Server::TextInputV2::ContentHint::AutoCapitalization);
    QTest::newRow("Lowercase/v2") << Wrapland::Client::TextInput::ContentHints(
        Wrapland::Client::TextInput::ContentHint::LowerCase)
                                  << Wrapland::Server::TextInputV2::ContentHints(
                                         Wrapland::Server::TextInputV2::ContentHint::LowerCase);
    QTest::newRow("Uppercase/v2") << Wrapland::Client::TextInput::ContentHints(
        Wrapland::Client::TextInput::ContentHint::UpperCase)
                                  << Wrapland::Server::TextInputV2::ContentHints(
                                         Wrapland::Server::TextInputV2::ContentHint::UpperCase);
    QTest::newRow("Titlecase/v2") << Wrapland::Client::TextInput::ContentHints(
        Wrapland::Client::TextInput::ContentHint::TitleCase)
                                  << Wrapland::Server::TextInputV2::ContentHints(
                                         Wrapland::Server::TextInputV2::ContentHint::TitleCase);
    QTest::newRow("HiddenText/v2") << Wrapland::Client::TextInput::ContentHints(
        Wrapland::Client::TextInput::ContentHint::HiddenText)
                                   << Wrapland::Server::TextInputV2::ContentHints(
                                          Wrapland::Server::TextInputV2::ContentHint::HiddenText);
    QTest::newRow("SensitiveData/v2")
        << Wrapland::Client::TextInput::ContentHints(
               Wrapland::Client::TextInput::ContentHint::SensitiveData)
        << Wrapland::Server::TextInputV2::ContentHints(
               Wrapland::Server::TextInputV2::ContentHint::SensitiveData);
    QTest::newRow("Latin/v2") << Wrapland::Client::TextInput::ContentHints(
        Wrapland::Client::TextInput::ContentHint::Latin)
                              << Wrapland::Server::TextInputV2::ContentHints(
                                     Wrapland::Server::TextInputV2::ContentHint::Latin);
    QTest::newRow("Multiline/v2") << Wrapland::Client::TextInput::ContentHints(
        Wrapland::Client::TextInput::ContentHint::MultiLine)
                                  << Wrapland::Server::TextInputV2::ContentHints(
                                         Wrapland::Server::TextInputV2::ContentHint::MultiLine);

    QTest::newRow("autos/v2") << (Wrapland::Client::TextInput::ContentHint::AutoCompletion
                                  | Wrapland::Client::TextInput::ContentHint::AutoCorrection
                                  | Wrapland::Client::TextInput::ContentHint::AutoCapitalization)
                              << (Wrapland::Server::TextInputV2::ContentHint::AutoCompletion
                                  | Wrapland::Server::TextInputV2::ContentHint::AutoCorrection
                                  | Wrapland::Server::TextInputV2::ContentHint::AutoCapitalization);

    // all has combinations which don't make sense - what's both lowercase and uppercase?
    QTest::newRow("all/v2") << (Wrapland::Client::TextInput::ContentHint::AutoCompletion
                                | Wrapland::Client::TextInput::ContentHint::AutoCorrection
                                | Wrapland::Client::TextInput::ContentHint::AutoCapitalization
                                | Wrapland::Client::TextInput::ContentHint::LowerCase
                                | Wrapland::Client::TextInput::ContentHint::UpperCase
                                | Wrapland::Client::TextInput::ContentHint::TitleCase
                                | Wrapland::Client::TextInput::ContentHint::HiddenText
                                | Wrapland::Client::TextInput::ContentHint::SensitiveData
                                | Wrapland::Client::TextInput::ContentHint::Latin
                                | Wrapland::Client::TextInput::ContentHint::MultiLine)
                            << (Wrapland::Server::TextInputV2::ContentHint::AutoCompletion
                                | Wrapland::Server::TextInputV2::ContentHint::AutoCorrection
                                | Wrapland::Server::TextInputV2::ContentHint::AutoCapitalization
                                | Wrapland::Server::TextInputV2::ContentHint::LowerCase
                                | Wrapland::Server::TextInputV2::ContentHint::UpperCase
                                | Wrapland::Server::TextInputV2::ContentHint::TitleCase
                                | Wrapland::Server::TextInputV2::ContentHint::HiddenText
                                | Wrapland::Server::TextInputV2::ContentHint::SensitiveData
                                | Wrapland::Server::TextInputV2::ContentHint::Latin
                                | Wrapland::Server::TextInputV2::ContentHint::MultiLine);
}

void TextInputTest::testContentHints()
{
    // this test verifies that content hints are properly passed from client to server
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);
    QCOMPARE(ti->contentHints(), Wrapland::Server::TextInputV2::ContentHints());

    QSignalSpy contentTypeChangedSpy(ti, &Wrapland::Server::TextInputV2::contentTypeChanged);
    QVERIFY(contentTypeChangedSpy.isValid());
    QFETCH(Wrapland::Client::TextInput::ContentHints, clientHints);
    textInput->setContentType(clientHints, Wrapland::Client::TextInput::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(ti->contentHints(), "serverHints");

    // setting to same should not trigger an update
    textInput->setContentType(clientHints, Wrapland::Client::TextInput::ContentPurpose::Normal);
    QVERIFY(!contentTypeChangedSpy.wait(100));

    // unsetting should work
    textInput->setContentType(Wrapland::Client::TextInput::ContentHints(),
                              Wrapland::Client::TextInput::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(ti->contentHints(), Wrapland::Server::TextInputV2::ContentHints());
}

void TextInputTest::testContentPurpose_data()
{
    QTest::addColumn<Wrapland::Client::TextInput::ContentPurpose>("clientPurpose");
    QTest::addColumn<Wrapland::Server::TextInputV2::ContentPurpose>("serverPurpose");

    QTest::newRow("Alpha/v2") << Wrapland::Client::TextInput::ContentPurpose::Alpha
                              << Wrapland::Server::TextInputV2::ContentPurpose::Alpha;
    QTest::newRow("Digits/v2") << Wrapland::Client::TextInput::ContentPurpose::Digits
                               << Wrapland::Server::TextInputV2::ContentPurpose::Digits;
    QTest::newRow("Number/v2") << Wrapland::Client::TextInput::ContentPurpose::Number
                               << Wrapland::Server::TextInputV2::ContentPurpose::Number;
    QTest::newRow("Phone/v2") << Wrapland::Client::TextInput::ContentPurpose::Phone
                              << Wrapland::Server::TextInputV2::ContentPurpose::Phone;
    QTest::newRow("Url/v2") << Wrapland::Client::TextInput::ContentPurpose::Url
                            << Wrapland::Server::TextInputV2::ContentPurpose::Url;
    QTest::newRow("Email/v2") << Wrapland::Client::TextInput::ContentPurpose::Email
                              << Wrapland::Server::TextInputV2::ContentPurpose::Email;
    QTest::newRow("Name/v2") << Wrapland::Client::TextInput::ContentPurpose::Name
                             << Wrapland::Server::TextInputV2::ContentPurpose::Name;
    QTest::newRow("Password/v2") << Wrapland::Client::TextInput::ContentPurpose::Password
                                 << Wrapland::Server::TextInputV2::ContentPurpose::Password;
    QTest::newRow("Date/v2") << Wrapland::Client::TextInput::ContentPurpose::Date
                             << Wrapland::Server::TextInputV2::ContentPurpose::Date;
    QTest::newRow("Time/v2") << Wrapland::Client::TextInput::ContentPurpose::Time
                             << Wrapland::Server::TextInputV2::ContentPurpose::Time;
    QTest::newRow("Datetime/v2") << Wrapland::Client::TextInput::ContentPurpose::DateTime
                                 << Wrapland::Server::TextInputV2::ContentPurpose::DateTime;
    QTest::newRow("Terminal/v2") << Wrapland::Client::TextInput::ContentPurpose::Terminal
                                 << Wrapland::Server::TextInputV2::ContentPurpose::Terminal;
}

void TextInputTest::testContentPurpose()
{
    // this test verifies that content purpose are properly passed from client to server
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);
    QCOMPARE(ti->contentPurpose(), Wrapland::Server::TextInputV2::ContentPurpose::Normal);

    QSignalSpy contentTypeChangedSpy(ti, &Wrapland::Server::TextInputV2::contentTypeChanged);
    QVERIFY(contentTypeChangedSpy.isValid());
    QFETCH(Wrapland::Client::TextInput::ContentPurpose, clientPurpose);
    textInput->setContentType(Wrapland::Client::TextInput::ContentHints(), clientPurpose);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(ti->contentPurpose(), "serverPurpose");

    // setting to same should not trigger an update
    textInput->setContentType(Wrapland::Client::TextInput::ContentHints(), clientPurpose);
    QVERIFY(!contentTypeChangedSpy.wait(100));

    // unsetting should work
    textInput->setContentType(Wrapland::Client::TextInput::ContentHints(),
                              Wrapland::Client::TextInput::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(ti->contentPurpose(), Wrapland::Server::TextInputV2::ContentPurpose::Normal);
}

void TextInputTest::testTextDirection_data()
{
    QTest::addColumn<Qt::LayoutDirection>("textDirection");

    QTest::newRow("ltr/v2") << Qt::LeftToRight;
    QTest::newRow("rtl/v2") << Qt::RightToLeft;
}

void TextInputTest::testTextDirection()
{
    // this test verifies that the text direction is sent from server to client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // default should be auto
    QCOMPARE(textInput->textDirection(), Qt::LayoutDirectionAuto);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);

    // let's send the new text direction
    QSignalSpy textDirectionChangedSpy(textInput.get(),
                                       &Wrapland::Client::TextInput::textDirectionChanged);
    QVERIFY(textDirectionChangedSpy.isValid());
    QFETCH(Qt::LayoutDirection, textDirection);
    ti->setTextDirection(textDirection);
    QVERIFY(textDirectionChangedSpy.wait());
    QCOMPARE(textInput->textDirection(), textDirection);
    // setting again should not change
    ti->setTextDirection(textDirection);
    QVERIFY(!textDirectionChangedSpy.wait(100));

    // setting back to auto
    ti->setTextDirection(Qt::LayoutDirectionAuto);
    QVERIFY(textDirectionChangedSpy.wait());
    QCOMPARE(textInput->textDirection(), Qt::LayoutDirectionAuto);
}

void TextInputTest::testLanguage()
{
    // this test verifies that language is sent from server to client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // default should be empty
    QVERIFY(textInput->language().isEmpty());
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);

    // let's send the new language
    QSignalSpy langugageChangedSpy(textInput.get(), &Wrapland::Client::TextInput::languageChanged);
    QVERIFY(langugageChangedSpy.isValid());
    ti->setLanguage(QByteArrayLiteral("foo"));
    QVERIFY(langugageChangedSpy.wait());
    QCOMPARE(textInput->language(), QByteArrayLiteral("foo"));
    // setting to same should not trigger
    ti->setLanguage(QByteArrayLiteral("foo"));
    QVERIFY(!langugageChangedSpy.wait(100));
    // but to something else should trigger again
    ti->setLanguage(QByteArrayLiteral("bar"));
    QVERIFY(langugageChangedSpy.wait());
    QCOMPARE(textInput->language(), QByteArrayLiteral("bar"));
}

void TextInputTest::testKeyEvent()
{
    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<Wrapland::Client::TextInput::KeyState>();
    // this test verifies that key events are properly sent to the client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);

    // TODO: test modifiers
    QSignalSpy keyEventSpy(textInput.get(), &Wrapland::Client::TextInput::keyEvent);
    QVERIFY(keyEventSpy.isValid());
    m_serverSeat->setTimestamp(100);
    ti->keysymPressed(2);
    QVERIFY(keyEventSpy.wait());
    QCOMPARE(keyEventSpy.count(), 1);
    QCOMPARE(keyEventSpy.last().at(0).value<quint32>(), 2u);
    QCOMPARE(keyEventSpy.last().at(1).value<Wrapland::Client::TextInput::KeyState>(),
             Wrapland::Client::TextInput::KeyState::Pressed);
    QCOMPARE(keyEventSpy.last().at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers());
    QCOMPARE(keyEventSpy.last().at(3).value<quint32>(), 100u);
    m_serverSeat->setTimestamp(101);
    ti->keysymReleased(2);
    QVERIFY(keyEventSpy.wait());
    QCOMPARE(keyEventSpy.count(), 2);
    QCOMPARE(keyEventSpy.last().at(0).value<quint32>(), 2u);
    QCOMPARE(keyEventSpy.last().at(1).value<Wrapland::Client::TextInput::KeyState>(),
             Wrapland::Client::TextInput::KeyState::Released);
    QCOMPARE(keyEventSpy.last().at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers());
    QCOMPARE(keyEventSpy.last().at(3).value<quint32>(), 101u);
}

void TextInputTest::testPreEdit()
{
    // this test verifies that pre-edit is correctly passed to the client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // verify default values
    QVERIFY(textInput->composingText().isEmpty());
    QVERIFY(textInput->composingFallbackText().isEmpty());
    QCOMPARE(textInput->composingTextCursorPosition(), 0);

    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);

    // now let's pass through some pre-edit events
    QSignalSpy composingTextChangedSpy(textInput.get(),
                                       &Wrapland::Client::TextInput::composingTextChanged);
    QVERIFY(composingTextChangedSpy.isValid());
    ti->setPreEditCursor(1);
    ti->preEdit(QByteArrayLiteral("foo"), QByteArrayLiteral("bar"));
    QVERIFY(composingTextChangedSpy.wait());
    QCOMPARE(composingTextChangedSpy.count(), 1);
    QCOMPARE(textInput->composingText(), QByteArrayLiteral("foo"));
    QCOMPARE(textInput->composingFallbackText(), QByteArrayLiteral("bar"));
    QCOMPARE(textInput->composingTextCursorPosition(), 1);

    // when no pre edit cursor is sent, it's at end of text
    ti->preEdit(QByteArrayLiteral("foobar"), QByteArray());
    QVERIFY(composingTextChangedSpy.wait());
    QCOMPARE(composingTextChangedSpy.count(), 2);
    QCOMPARE(textInput->composingText(), QByteArrayLiteral("foobar"));
    QCOMPARE(textInput->composingFallbackText(), QByteArray());
    QCOMPARE(textInput->composingTextCursorPosition(), 6);
}

void TextInputTest::testCommit()
{
    // this test verifies that the commit is handled correctly by the client
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    std::unique_ptr<Wrapland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // verify default values
    QCOMPARE(textInput->commitText(), QByteArray());
    QCOMPARE(textInput->cursorPosition(), 0);
    QCOMPARE(textInput->anchorPosition(), 0);
    QCOMPARE(textInput->deleteSurroundingText().beforeLength, 0u);
    QCOMPARE(textInput->deleteSurroundingText().afterLength, 0u);

    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_serverSeat->focusedTextInput();
    QVERIFY(ti);

    // now let's commit
    QSignalSpy committedSpy(textInput.get(), &Wrapland::Client::TextInput::committed);
    QVERIFY(committedSpy.isValid());
    ti->setCursorPosition(3, 4);
    ti->deleteSurroundingText(2, 1);
    ti->commit(QByteArrayLiteral("foo"));

    QVERIFY(committedSpy.wait());
    QCOMPARE(textInput->commitText(), QByteArrayLiteral("foo"));
    QCOMPARE(textInput->cursorPosition(), 3);
    QCOMPARE(textInput->anchorPosition(), 4);
    QCOMPARE(textInput->deleteSurroundingText().beforeLength, 2u);
    QCOMPARE(textInput->deleteSurroundingText().afterLength, 1u);
}

QTEST_GUILESS_MAIN(TextInputTest)
#include "text_input.moc"
