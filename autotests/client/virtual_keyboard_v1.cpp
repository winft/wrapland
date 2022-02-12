/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <wayland-client-protocol.h>

#include <QtTest>
#include <qobject.h>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"
#include "../../src/client/virtual_keyboard_v1.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/surface.h"
#include "../../server/virtual_keyboard_v1.h"

#include <linux/input.h>

class virtual_keyboard_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_keymap();
    void test_key();
    void test_modifiers();

private:
    Wrapland::Server::Surface* wait_for_surface() const;
    std::unique_ptr<Wrapland::Client::virtual_keyboard_v1> create_virtual_keyboard() const;

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
        Wrapland::Client::virtual_keyboard_manager_v1* vk{nullptr};
        QThread* thread{nullptr};
    } client;
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
    server.globals.seats.push_back(server.display->createSeat());
    server.seat = server.globals.seats.back().get();
    server.seat->setHasKeyboard(true);

    server.globals.compositor = server.display->createCompositor();
    server.globals.virtual_keyboard_manager_v1
        = server.display->create_virtual_keyboard_manager_v1();
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

Wrapland::Server::Surface* virtual_keyboard_test::wait_for_surface() const
{
    auto surface_spy = QSignalSpy(server.globals.compositor.get(),
                                  &Wrapland::Server::Compositor::surfaceCreated);
    if (surface_spy.isValid() && surface_spy.wait() && surface_spy.count() == 1) {
        return surface_spy.first().first().value<Wrapland::Server::Surface*>();
    }
    return nullptr;
}

std::unique_ptr<Wrapland::Client::virtual_keyboard_v1>
virtual_keyboard_test::create_virtual_keyboard() const
{
    return std::unique_ptr<Wrapland::Client::virtual_keyboard_v1>(
        client.vk->create_virtual_keyboard(client.seat));
}

void virtual_keyboard_test::init()
{
    qRegisterMetaType<Wrapland::Server::Seat*>();
    qRegisterMetaType<Wrapland::Server::key_state>();

    setup_server(server);

    setup_client(client, this);
    client.vk = client.registry->createVirtualKeyboardManagerV1(
        client.registry->interface(Wrapland::Client::Registry::Interface::VirtualKeyboardManagerV1)
            .name,
        client.registry->interface(Wrapland::Client::Registry::Interface::VirtualKeyboardManagerV1)
            .version,
        this);
    QVERIFY(client.vk->isValid());
}

void virtual_keyboard_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete (variable);                                                                         \
        (variable) = nullptr;                                                                      \
    }

    CLEANUP(client.vk)
    CLEANUP(client.seat)
    CLEANUP(client.compositor)
    CLEANUP(client.registry)
    CLEANUP(client.queue)
    if (client.connection) {
        client.connection->deleteLater();
        client.connection = nullptr;
    }
    if (client.thread) {
        client.thread->quit();
        client.thread->wait();
        delete client.thread;
        client.thread = nullptr;
    }
#undef CLEANUP

    server = {};
}

/**
 * Verify that keymaps are transferred correctly.
 */
void virtual_keyboard_test::test_keymap()
{
    QSignalSpy keyboard_spy(server.globals.virtual_keyboard_manager_v1.get(),
                            &Wrapland::Server::virtual_keyboard_manager_v1::keyboard_created);
    QVERIFY(keyboard_spy.isValid());

    auto vk = create_virtual_keyboard();
    QVERIFY(keyboard_spy.wait());

    auto server_vk = keyboard_spy.back().front().value<Wrapland::Server::virtual_keyboard_v1*>();
    QCOMPARE(keyboard_spy.back().back().value<Wrapland::Server::Seat*>(), server.seat);

    QSignalSpy keymap_spy(server_vk, &Wrapland::Server::virtual_keyboard_v1::keymap);
    QVERIFY(keymap_spy.isValid());

    constexpr auto keymap1 = "foo";
    vk->keymap(keymap1);
    QVERIFY(keymap_spy.wait());

    QCOMPARE(keymap_spy.back().at(0).value<uint32_t>(), WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

    auto size = keymap_spy.back().at(2).value<uint32_t>();
    QCOMPARE(size, 3);

    auto fd = keymap_spy.back().at(1).value<int32_t>();
    QVERIFY(fd != -1);

    QFile file;
    QVERIFY(file.open(fd, QIODevice::ReadOnly));

    auto address = reinterpret_cast<char*>(file.map(0, size));
    QVERIFY(address);
    QCOMPARE(address, "foo");
    file.close();

    // Change the keymap.
    keymap_spy.clear();

    constexpr auto keymap2 = "bars";
    vk->keymap(keymap2);
    QVERIFY(keymap_spy.wait());

    QCOMPARE(keymap_spy.back().at(0).value<uint32_t>(), WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

    size = keymap_spy.back().at(2).value<uint32_t>();
    QCOMPARE(size, 4);

    fd = keymap_spy.back().at(1).value<int32_t>();
    QVERIFY(fd != -1);

    QVERIFY(file.open(fd, QIODevice::ReadWrite));
    address = reinterpret_cast<char*>(file.map(0, size));
    QVERIFY(address);
    QCOMPARE(address, "bars");
}

/**
 * Verify that keys are sent correctly.
 */
void virtual_keyboard_test::test_key()
{
    QSignalSpy keyboard_spy(server.globals.virtual_keyboard_manager_v1.get(),
                            &Wrapland::Server::virtual_keyboard_manager_v1::keyboard_created);
    QVERIFY(keyboard_spy.isValid());

    auto vk = create_virtual_keyboard();
    QVERIFY(keyboard_spy.wait());

    auto server_vk = keyboard_spy.back().front().value<Wrapland::Server::virtual_keyboard_v1*>();
    QCOMPARE(keyboard_spy.back().back().value<Wrapland::Server::Seat*>(), server.seat);

    // Need to set a keymap first.
    QSignalSpy keymap_spy(server_vk, &Wrapland::Server::virtual_keyboard_v1::keymap);
    QVERIFY(keymap_spy.isValid());

    constexpr auto keymap1 = "foo";
    vk->keymap(keymap1);
    QVERIFY(keymap_spy.wait());

    // Now press some key.
    QSignalSpy key_spy(server_vk, &Wrapland::Server::virtual_keyboard_v1::key);
    QVERIFY(key_spy.isValid());

    vk->key(std::chrono::milliseconds(1000), KEY_E, Wrapland::Client::key_state::pressed);
    QVERIFY(key_spy.wait());
    QCOMPARE(key_spy.back().at(0).value<uint32_t>(), 1000);
    QCOMPARE(key_spy.back().at(1).value<uint32_t>(), KEY_E);
    QCOMPARE(key_spy.back().at(2).value<Wrapland::Server::key_state>(),
             Wrapland::Server::key_state::pressed);

    // Release again
    vk->key(std::chrono::milliseconds(1001), KEY_E, Wrapland::Client::key_state::released);
    QVERIFY(key_spy.wait());
    QCOMPARE(key_spy.back().at(0).value<uint32_t>(), 1001);
    QCOMPARE(key_spy.back().at(1).value<uint32_t>(), KEY_E);
    QCOMPARE(key_spy.back().at(2).value<Wrapland::Server::key_state>(),
             Wrapland::Server::key_state::released);
}

/**
 * Verify that modifiers are sent correctly.
 */
void virtual_keyboard_test::test_modifiers()
{
    QSignalSpy keyboard_spy(server.globals.virtual_keyboard_manager_v1.get(),
                            &Wrapland::Server::virtual_keyboard_manager_v1::keyboard_created);
    QVERIFY(keyboard_spy.isValid());

    auto vk = create_virtual_keyboard();
    QVERIFY(keyboard_spy.wait());

    auto server_vk = keyboard_spy.back().front().value<Wrapland::Server::virtual_keyboard_v1*>();
    QCOMPARE(keyboard_spy.back().back().value<Wrapland::Server::Seat*>(), server.seat);

    // Need to set a keymap first.
    QSignalSpy keymap_spy(server_vk, &Wrapland::Server::virtual_keyboard_v1::keymap);
    QVERIFY(keymap_spy.isValid());

    constexpr auto keymap1 = "foo";
    vk->keymap(keymap1);
    QVERIFY(keymap_spy.wait());

    // Now press some modifier.
    QSignalSpy modifiers_spy(server_vk, &Wrapland::Server::virtual_keyboard_v1::modifiers);
    QVERIFY(modifiers_spy.isValid());

    vk->modifiers(1, 2, 3, 4);
    QVERIFY(modifiers_spy.wait());
    QCOMPARE(modifiers_spy.back().at(0).value<uint32_t>(), 1);
    QCOMPARE(modifiers_spy.back().at(1).value<uint32_t>(), 2);
    QCOMPARE(modifiers_spy.back().at(2).value<uint32_t>(), 3);
    QCOMPARE(modifiers_spy.back().at(3).value<uint32_t>(), 4);
}

QTEST_GUILESS_MAIN(virtual_keyboard_test)
#include "virtual_keyboard_v1.moc"
