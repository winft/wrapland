/********************************************************************
Copyright 2018 David Edmundson <davidedmundson@kde.org>
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

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
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"
#include "../../src/client/xdg_shell.h"
#include "../../src/client/xdgdecoration.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/surface.h"
#include "../../server/xdg_decoration.h"
#include "../../server/xdg_shell.h"
#include "../../server/xdg_shell_toplevel.h"

#include "../../tests/globals.h"

using namespace Wrapland;

class TestXdgDecoration : public QObject
{
    Q_OBJECT
public:
    explicit TestXdgDecoration(QObject* parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void testDecoration_data();
    void testDecoration();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    Wrapland::Client::XdgShell* m_xdgShell = nullptr;
    Wrapland::Client::XdgDecorationManager* m_xdgDecorationManager = nullptr;

    QThread* m_thread = nullptr;
    Wrapland::Client::Registry* m_registry = nullptr;
};

constexpr auto socket_name{"wrapland-test-wayland-server-side-decoration-0"};

TestXdgDecoration::TestXdgDecoration(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<Server::Surface*>();
}

void TestXdgDecoration::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Client::Registry();
    QSignalSpy compositorSpy(m_registry, &Client::Registry::compositorAnnounced);
    QSignalSpy xdgShellSpy(m_registry, &Client::Registry::xdgShellAnnounced);
    QSignalSpy xdgDecorationManagerSpy(m_registry, &Client::Registry::xdgDecorationAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());
    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(),
                                                compositorSpy.first().last().value<quint32>(),
                                                this);

    server.globals.xdg_shell = std::make_unique<Wrapland::Server::XdgShell>(server.display.get());
    QVERIFY(xdgShellSpy.wait());
    m_xdgShell = m_registry->createXdgShell(xdgShellSpy.first().first().value<quint32>(),
                                            xdgShellSpy.first().last().value<quint32>(),
                                            this);

    server.globals.xdg_decoration_manager
        = std::make_unique<Wrapland::Server::XdgDecorationManager>(server.display.get(),
                                                                   server.globals.xdg_shell.get());
    QVERIFY(xdgDecorationManagerSpy.wait());
    m_xdgDecorationManager = m_registry->createXdgDecorationManager(
        xdgDecorationManagerSpy.first().first().value<quint32>(),
        xdgDecorationManagerSpy.first().last().value<quint32>(),
        this);
}

void TestXdgDecoration::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_xdgShell) {
        delete m_xdgShell;
        m_xdgShell = nullptr;
    }
    if (m_xdgDecorationManager) {
        delete m_xdgDecorationManager;
        m_xdgDecorationManager = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_registry) {
        delete m_registry;
        m_registry = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    server = {};
}

void TestXdgDecoration::testDecoration_data()
{
    QTest::addColumn<Wrapland::Server::XdgDecoration::Mode>("configuredMode");
    QTest::addColumn<Wrapland::Client::XdgDecoration::Mode>("configuredModeExp");
    QTest::addColumn<Wrapland::Client::XdgDecoration::Mode>("setMode");
    QTest::addColumn<Wrapland::Server::XdgDecoration::Mode>("setModeExp");

    auto const serverClient = Server::XdgDecoration::Mode::ClientSide;
    auto const serverServer = Server::XdgDecoration::Mode::ServerSide;
    auto const clientClient = Client::XdgDecoration::Mode::ClientSide;
    auto const clientServer = Client::XdgDecoration::Mode::ServerSide;

    QTest::newRow("client->client") << serverClient << clientClient << clientClient << serverClient;
    QTest::newRow("client->server") << serverClient << clientClient << clientServer << serverServer;
    QTest::newRow("server->client") << serverServer << clientServer << clientClient << serverClient;
    QTest::newRow("server->server") << serverServer << clientServer << clientServer << serverServer;
}

void TestXdgDecoration::testDecoration()
{
    QFETCH(Wrapland::Server::XdgDecoration::Mode, configuredMode);
    QFETCH(Wrapland::Client::XdgDecoration::Mode, configuredModeExp);
    QFETCH(Wrapland::Client::XdgDecoration::Mode, setMode);
    QFETCH(Wrapland::Server::XdgDecoration::Mode, setModeExp);

    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    QSignalSpy shellSurfaceCreatedSpy(server.globals.xdg_shell.get(),
                                      &Server::XdgShell::toplevelCreated);
    QSignalSpy decorationCreatedSpy(server.globals.xdg_decoration_manager.get(),
                                    &Server::XdgDecorationManager::decorationCreated);

    // Create shell surface and deco object.
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellToplevel> shellSurface(
        m_xdgShell->create_toplevel(surface.get()));
    std::unique_ptr<Client::XdgDecoration> decoration(
        m_xdgDecorationManager->getToplevelDecoration(shellSurface.get()));

    // And receive all these on the server.
    QVERIFY(surfaceCreatedSpy.count() || surfaceCreatedSpy.wait());
    QVERIFY(shellSurfaceCreatedSpy.count() || shellSurfaceCreatedSpy.wait());
    QVERIFY(decorationCreatedSpy.count() || decorationCreatedSpy.wait());

    auto serverShellSurface
        = shellSurfaceCreatedSpy.first().first().value<Server::XdgShellToplevel*>();
    auto serverDecoration = decorationCreatedSpy.first().first().value<Server::XdgDecoration*>();

    QVERIFY(serverDecoration);
    QVERIFY(serverShellSurface);
    QCOMPARE(serverDecoration->toplevel(), serverShellSurface);
    QCOMPARE(serverDecoration->requestedMode(), Server::XdgDecoration::Mode::Undefined);

    QSignalSpy clientConfiguredSpy(decoration.get(), &Client::XdgDecoration::modeChanged);
    QSignalSpy modeRequestedSpy(serverDecoration, &Server::XdgDecoration::modeRequested);

    // Server configuring a client.
    serverDecoration->configure(configuredMode);
    quint32 serial = serverShellSurface->configure({});
    QVERIFY(clientConfiguredSpy.wait());
    QCOMPARE(clientConfiguredSpy.first().first().value<Client::XdgDecoration::Mode>(),
             configuredModeExp);

    shellSurface->ackConfigure(serial);

    // Client requesting another mode.
    decoration->setMode(setMode);
    QVERIFY(modeRequestedSpy.wait());
    QCOMPARE(serverDecoration->requestedMode(), setModeExp);
    modeRequestedSpy.clear();

    decoration->unsetMode();
    QVERIFY(modeRequestedSpy.wait());
    QCOMPARE(serverDecoration->requestedMode(), Server::XdgDecoration::Mode::Undefined);
}

QTEST_GUILESS_MAIN(TestXdgDecoration)
#include "xdg_decoration.moc"
