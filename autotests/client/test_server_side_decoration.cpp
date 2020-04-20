/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
// Qt
#include <QtTest>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/server_decoration.h"
#include "../../src/client/surface.h"

#include "../../server/display.h"
#include "../../server/compositor.h"
#include "../../server/surface.h"

#include "../../src/server/server_decoration_interface.h"

class TestServerSideDecoration : public QObject
{
    Q_OBJECT
public:
    explicit TestServerSideDecoration(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate_data();
    void testCreate();

    void testRequest_data();
    void testRequest();

    void testSurfaceDestroy();

private:
    Wrapland::Server::D_isplay *m_display = nullptr;
    Wrapland::Server::Compositor *m_serverCompositor = nullptr;
    Wrapland::Server::ServerSideDecorationManagerInterface *m_serverSideDecorationManagerInterface = nullptr;

    Wrapland::Client::ConnectionThread *m_connection = nullptr;
    Wrapland::Client::Compositor *m_compositor = nullptr;
    Wrapland::Client::EventQueue *m_queue = nullptr;
    Wrapland::Client::ServerSideDecorationManager *m_serverSideDecorationManager = nullptr;
    QThread *m_thread = nullptr;
    Wrapland::Client::Registry *m_registry = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-server-side-decoration-0");

TestServerSideDecoration::TestServerSideDecoration(QObject *parent)
    : QObject(parent)
{
}

void TestServerSideDecoration::init()
{
    m_display = new Wrapland::Server::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

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
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Wrapland::Client::Registry();
    QSignalSpy compositorSpy(m_registry, &Wrapland::Client::Registry::compositorAnnounced);
    QVERIFY(compositorSpy.isValid());
    QSignalSpy serverSideDecoManagerSpy(m_registry, &Wrapland::Client::Registry::serverSideDecorationManagerAnnounced);
    QVERIFY(serverSideDecoManagerSpy.isValid());

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    m_serverCompositor = m_display->createCompositor(m_display);

    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_serverSideDecorationManagerInterface = m_display->createServerSideDecorationManager(m_display);
    m_serverSideDecorationManagerInterface->create();
    QVERIFY(m_serverSideDecorationManagerInterface->isValid());

    QVERIFY(serverSideDecoManagerSpy.wait());
    m_serverSideDecorationManager = m_registry->createServerSideDecorationManager(serverSideDecoManagerSpy.first().first().value<quint32>(),
                                                                                  serverSideDecoManagerSpy.first().last().value<quint32>(), this);
}

void TestServerSideDecoration::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_serverSideDecorationManager) {
        delete m_serverSideDecorationManager;
        m_serverSideDecorationManager = nullptr;
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

    delete m_display;
    m_display = nullptr;
}

void TestServerSideDecoration::testCreate_data()
{
    QTest::addColumn<Wrapland::Server::ServerSideDecorationManagerInterface::Mode>("serverMode");
    QTest::addColumn<Wrapland::Client::ServerSideDecoration::Mode>("clientMode");

    QTest::newRow("none") << Wrapland::Server::ServerSideDecorationManagerInterface::Mode::None     << Wrapland::Client::ServerSideDecoration::Mode::None;
    QTest::newRow("client") << Wrapland::Server::ServerSideDecorationManagerInterface::Mode::Client << Wrapland::Client::ServerSideDecoration::Mode::Client;
    QTest::newRow("server") << Wrapland::Server::ServerSideDecorationManagerInterface::Mode::Server << Wrapland::Client::ServerSideDecoration::Mode::Server;
}

void TestServerSideDecoration::testCreate()
{
    QFETCH(Wrapland::Server::ServerSideDecorationManagerInterface::Mode, serverMode);
    m_serverSideDecorationManagerInterface->setDefaultMode(serverMode);
    QCOMPARE(m_serverSideDecorationManagerInterface->defaultMode(), serverMode);

    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &Wrapland::Server::ServerSideDecorationManagerInterface::decorationCreated);
    QVERIFY(decorationCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(!Wrapland::Server::ServerSideDecorationInterface::get(serverSurface));

    // create server side deco
    std::unique_ptr<Wrapland::Client::ServerSideDecoration> serverSideDecoration(m_serverSideDecorationManager->create(surface.get()));
    QCOMPARE(serverSideDecoration->mode(), Wrapland::Client::ServerSideDecoration::Mode::None);
    QSignalSpy modeChangedSpy(serverSideDecoration.get(), &Wrapland::Client::ServerSideDecoration::modeChanged);
    QVERIFY(modeChangedSpy.isValid());

    QVERIFY(decorationCreated.wait());

    auto serverDeco = decorationCreated.first().first().value<Wrapland::Server::ServerSideDecorationInterface*>();
    QVERIFY(serverDeco);
    QCOMPARE(serverDeco, Wrapland::Server::ServerSideDecorationInterface::get(serverSurface));
    QCOMPARE(serverDeco->surface(), serverSurface);

    // after binding the client should get the default mode
    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 1);
    QTEST(serverSideDecoration->mode(), "clientMode");

    // and destroy
    QSignalSpy destroyedSpy(serverDeco, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    serverSideDecoration.reset();
    QVERIFY(destroyedSpy.wait());
}

void TestServerSideDecoration::testRequest_data()
{
    QTest::addColumn<Wrapland::Server::ServerSideDecorationManagerInterface::Mode>("defaultMode");
    QTest::addColumn<Wrapland::Client::ServerSideDecoration::Mode>("clientMode");
    QTest::addColumn<Wrapland::Client::ServerSideDecoration::Mode>("clientRequestMode");
    QTest::addColumn<Wrapland::Server::ServerSideDecorationManagerInterface::Mode>("serverRequestedMode");

    const auto serverNone = Wrapland::Server::ServerSideDecorationManagerInterface::Mode::None;
    const auto serverClient = Wrapland::Server::ServerSideDecorationManagerInterface::Mode::Client;
    const auto serverServer = Wrapland::Server::ServerSideDecorationManagerInterface::Mode::Server;
    const auto clientNone = Wrapland::Client::ServerSideDecoration::Mode::None;
    const auto clientClient = Wrapland::Client::ServerSideDecoration::Mode::Client;
    const auto clientServer = Wrapland::Client::ServerSideDecoration::Mode::Server;

    QTest::newRow("none->none")     << serverNone   << clientNone   << clientNone   << serverNone;
    QTest::newRow("none->client")   << serverNone   << clientNone   << clientClient << serverClient;
    QTest::newRow("none->server")   << serverNone   << clientNone   << clientServer << serverServer;
    QTest::newRow("client->none")   << serverClient << clientClient << clientNone   << serverNone;
    QTest::newRow("client->client") << serverClient << clientClient << clientClient << serverClient;
    QTest::newRow("client->server") << serverClient << clientClient << clientServer << serverServer;
    QTest::newRow("server->none")   << serverServer << clientServer << clientNone   << serverNone;
    QTest::newRow("server->client") << serverServer << clientServer << clientClient << serverClient;
    QTest::newRow("server->server") << serverServer << clientServer << clientServer << serverServer;
}

void TestServerSideDecoration::testRequest()
{
    QFETCH(Wrapland::Server::ServerSideDecorationManagerInterface::Mode, defaultMode);
    m_serverSideDecorationManagerInterface->setDefaultMode(defaultMode);
    QCOMPARE(m_serverSideDecorationManagerInterface->defaultMode(), defaultMode);

    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &Wrapland::Server::ServerSideDecorationManagerInterface::decorationCreated);
    QVERIFY(decorationCreated.isValid());

    // create server side deco
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::ServerSideDecoration> serverSideDecoration(m_serverSideDecorationManager->create(surface.get()));
    QCOMPARE(serverSideDecoration->mode(), Wrapland::Client::ServerSideDecoration::Mode::None);
    QSignalSpy modeChangedSpy(serverSideDecoration.get(), &Wrapland::Client::ServerSideDecoration::modeChanged);
    QVERIFY(modeChangedSpy.isValid());
    QVERIFY(decorationCreated.wait());

    auto serverDeco = decorationCreated.first().first().value<Wrapland::Server::ServerSideDecorationInterface*>();
    QVERIFY(serverDeco);
    QSignalSpy modeSpy(serverDeco, &Wrapland::Server::ServerSideDecorationInterface::modeRequested);
    QVERIFY(modeSpy.isValid());

    // after binding the client should get the default mode
    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 1);
    QTEST(serverSideDecoration->mode(), "clientMode");

    // request a change
    QFETCH(Wrapland::Client::ServerSideDecoration::Mode, clientRequestMode);
    serverSideDecoration->requestMode(clientRequestMode);
    // mode not yet changed
    QTEST(serverSideDecoration->mode(), "clientMode");

    QVERIFY(modeSpy.wait());
    QCOMPARE(modeSpy.count(), 1);
    QFETCH(Wrapland::Server::ServerSideDecorationManagerInterface::Mode, serverRequestedMode);
    QCOMPARE(modeSpy.first().first().value<Wrapland::Server::ServerSideDecorationManagerInterface::Mode>(), serverRequestedMode);

    // mode not yet changed
    QCOMPARE(serverDeco->mode(), defaultMode);
    serverDeco->setMode(serverRequestedMode);
    QCOMPARE(serverDeco->mode(), serverRequestedMode);

    // should be sent to client
    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 2);
    QCOMPARE(serverSideDecoration->mode(), clientRequestMode);
}

void TestServerSideDecoration::testSurfaceDestroy()
{
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &Wrapland::Server::ServerSideDecorationManagerInterface::decorationCreated);
    QVERIFY(decorationCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    std::unique_ptr<Wrapland::Client::ServerSideDecoration> serverSideDecoration(m_serverSideDecorationManager->create(surface.get()));
    QCOMPARE(serverSideDecoration->mode(), Wrapland::Client::ServerSideDecoration::Mode::None);
    QVERIFY(decorationCreated.wait());
    auto serverDeco = decorationCreated.first().first().value<Wrapland::Server::ServerSideDecorationInterface*>();
    QVERIFY(serverDeco);

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &Wrapland::Server::Surface::resourceDestroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());
    QSignalSpy decorationDestroyedSpy(serverDeco, &QObject::destroyed);
    QVERIFY(decorationDestroyedSpy.isValid());
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(decorationDestroyedSpy.isEmpty());
    // destroy the blur
    serverSideDecoration.reset();
    QVERIFY(decorationDestroyedSpy.wait());
}

QTEST_GUILESS_MAIN(TestServerSideDecoration)
#include "test_server_side_decoration.moc"
