/****************************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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
****************************************************************************/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard_shortcuts_inhibit.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/keyboard_shortcuts_inhibit.h"
#include "../../server/surface.h"

#include <wayland-keyboard-shortcuts-inhibit-client-protocol.h>

using namespace Wrapland::Client;
using namespace Wrapland::Server;

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class TestKeyboardShortcutsInhibitor : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testKeyboardShortcuts();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    QVector<Srv::Surface*> m_serverSurfaces;
    QVector<Clt::Surface*> m_clientSurfaces;

    ConnectionThread* m_connection = nullptr;
    QThread* m_thread = nullptr;
    EventQueue* m_queue = nullptr;
    Clt::Seat* m_seat = nullptr;
    Clt::Compositor* m_clientCompositor;
    Clt::KeyboardShortcutsInhibitManagerV1* m_keyboard_shortcuts_inhibitor = nullptr;
};

constexpr auto socket_name{"wrapland-keyboard-shortcuts-inhibitor-test-0"};

void TestKeyboardShortcutsInhibitor::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    auto server_seat = server.globals.seats.emplace_back(server.display->createSeat()).get();
    server_seat->setName("seat0");
    server.globals.keyboard_shortcuts_inhibit_manager_v1
        = server.display->createKeyboardShortcutsInhibitManager();

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);

    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    server.globals.compositor = server.display->createCompositor();

    connect(server.globals.compositor.get(),
            &Srv::Compositor::surfaceCreated,
            this,
            [this](Srv::Surface* surface) { m_serverSurfaces += surface; });

    QVERIFY(compositorSpy.wait());
    m_clientCompositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                                   compositorSpy.first().last().value<quint32>(),
                                                   this);
    QVERIFY(m_clientCompositor->isValid());

    m_seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name,
                                 registry.interface(Registry::Interface::Seat).version,
                                 this);
    QVERIFY(m_seat->isValid());

    m_keyboard_shortcuts_inhibitor = registry.createKeyboardShortcutsInhibitManagerV1(
        registry.interface(Registry::Interface::KeyboardShortcutsInhibitManagerV1).name,
        registry.interface(Registry::Interface::KeyboardShortcutsInhibitManagerV1).version,
        this);
    QVERIFY(m_keyboard_shortcuts_inhibitor->isValid());

    QSignalSpy surfaceSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    for (int i = 0; i < 3; ++i) {
        m_clientSurfaces += m_clientCompositor->createSurface(this);
    }
    QVERIFY(surfaceSpy.count() < 3 && surfaceSpy.wait(200));
    QVERIFY(m_serverSurfaces.count() == 3);
    QVERIFY(m_keyboard_shortcuts_inhibitor);
}

void TestKeyboardShortcutsInhibitor::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_keyboard_shortcuts_inhibitor)
    CLEANUP(m_seat)
    CLEANUP(m_queue)
    CLEANUP(m_clientCompositor)

    for (auto surface : m_clientSurfaces) {
        delete surface;
    }
    m_clientSurfaces.clear();

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

void TestKeyboardShortcutsInhibitor::testKeyboardShortcuts()
{
    auto surface = m_clientSurfaces[0];

    // Test
    QSignalSpy inhibitorCreatedSpy(m_keyboard_shortcuts_inhibitor,
                                   &Clt::KeyboardShortcutsInhibitManagerV1::inhibitorCreated);

    auto inhibitorClient = m_keyboard_shortcuts_inhibitor->inhibitShortcuts(surface, m_seat);
    QSignalSpy inhibitorActiveSpy(inhibitorClient,
                                  &Clt::KeyboardShortcutsInhibitorV1::inhibitorActive);
    QSignalSpy inhibitorInactiveSpy(inhibitorClient,
                                    &Clt::KeyboardShortcutsInhibitorV1::inhibitorInactive);

    QVERIFY(inhibitorCreatedSpy.wait() || inhibitorCreatedSpy.count() == 1);
    auto inhibitorServer = server.globals.keyboard_shortcuts_inhibit_manager_v1->findInhibitor(
        m_serverSurfaces[0], server.globals.seats.back().get());

    // Test deactivate
    inhibitorServer->setActive(false);
    QVERIFY(inhibitorInactiveSpy.wait() || inhibitorInactiveSpy.count() == 1);

    // Test activate
    inhibitorServer->setActive(true);
    QVERIFY(inhibitorInactiveSpy.wait() || inhibitorInactiveSpy.count() == 1);

    // Test creating for another surface
    auto inhibitorClient2
        = m_keyboard_shortcuts_inhibitor->inhibitShortcuts(m_clientSurfaces[1], m_seat);
    QVERIFY(inhibitorCreatedSpy.wait() || inhibitorCreatedSpy.count() == 2);

    // Test destroy is working
    delete inhibitorClient;
    inhibitorClient = m_keyboard_shortcuts_inhibitor->inhibitShortcuts(surface, m_seat);
    QVERIFY(inhibitorCreatedSpy.wait() || inhibitorCreatedSpy.count() == 3);

    // Test creating with same surface / seat (expect error)
    QSignalSpy errorOccured(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QCOMPARE(m_connection->error(), 0);

    auto inhibitorClient3
        = m_keyboard_shortcuts_inhibitor->inhibitShortcuts(m_clientSurfaces[0], m_seat);

    QVERIFY(errorOccured.wait());
    QCOMPARE(m_connection->error(), EPROTO);
    QVERIFY(m_connection->hasProtocolError());
    QCOMPARE(m_connection->protocolError(),
             ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ALREADY_INHIBITED);

    delete inhibitorClient;
    delete inhibitorClient2;
    delete inhibitorClient3;
}

QTEST_GUILESS_MAIN(TestKeyboardShortcutsInhibitor)
#include "keyboard_shortcuts_inhibitor.moc"
