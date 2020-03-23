/********************************************************************
Copyright 2018 David Edmundson <davidedmundson@kde.org>

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
#include "../../src/client/xdgshell.h"
#include "../../src/client/xdgdecoration.h"
#include "../../src/client/surface.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/xdgshell_interface.h"
#include "../../src/server/xdgdecoration_interface.h"

class TestXdgDecoration : public QObject
{
    Q_OBJECT
public:
    explicit TestXdgDecoration(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testDecoration_data();
    void testDecoration();

private:
    Wrapland::Server::Display *m_display = nullptr;
    Wrapland::Server::CompositorInterface *m_compositorInterface = nullptr;
    Wrapland::Server::XdgShellInterface *m_xdgShellInterface = nullptr;
    Wrapland::Server::XdgDecorationManagerInterface *m_xdgDecorationManagerInterface = nullptr;

    Wrapland::Client::ConnectionThread *m_connection = nullptr;
    Wrapland::Client::Compositor *m_compositor = nullptr;
    Wrapland::Client::EventQueue *m_queue = nullptr;
    Wrapland::Client::XdgShell *m_xdgShell = nullptr;
    Wrapland::Client::XdgDecorationManager *m_xdgDecorationManager = nullptr;

    QThread *m_thread = nullptr;
    Wrapland::Client::Registry *m_registry = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-server-side-decoration-0");

TestXdgDecoration::TestXdgDecoration(QObject *parent)
    : QObject(parent)
{
}

void TestXdgDecoration::init()
{
    using namespace Wrapland::Server;
    using namespace Wrapland::Client;

    qRegisterMetaType<XdgDecoration::Mode>();
    qRegisterMetaType<XdgDecorationInterface::Mode>();

    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Registry();
    QSignalSpy compositorSpy(m_registry, &Registry::compositorAnnounced);
    QSignalSpy xdgShellSpy(m_registry, &Registry::xdgShellStableAnnounced);
    QSignalSpy xdgDecorationManagerSpy(m_registry, &Registry::xdgDecorationAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_xdgShellInterface = m_display->createXdgShell(XdgShellInterfaceVersion::Stable, m_display);
    m_xdgShellInterface->create();
    QVERIFY(m_xdgShellInterface->isValid());
    QVERIFY(xdgShellSpy.wait());
    m_xdgShell = m_registry->createXdgShell(xdgShellSpy.first().first().value<quint32>(),
                                            xdgShellSpy.first().last().value<quint32>(), this);

    m_xdgDecorationManagerInterface = m_display->createXdgDecorationManager(m_xdgShellInterface, m_display);
    m_xdgDecorationManagerInterface->create();
    QVERIFY(m_xdgDecorationManagerInterface->isValid());

    QVERIFY(xdgDecorationManagerSpy.wait());
    m_xdgDecorationManager = m_registry->createXdgDecorationManager(xdgDecorationManagerSpy.first().first().value<quint32>(),
                                                                    xdgDecorationManagerSpy.first().last().value<quint32>(), this);
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

    delete m_display;
    m_display = nullptr;
}


void TestXdgDecoration::testDecoration_data()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    QTest::addColumn<Wrapland::Server::XdgDecorationInterface::Mode>("configuredMode");
    QTest::addColumn<Wrapland::Client::XdgDecoration::Mode>("configuredModeExp");
    QTest::addColumn<Wrapland::Client::XdgDecoration::Mode>("setMode");
    QTest::addColumn<Wrapland::Server::XdgDecorationInterface::Mode>("setModeExp");

    const auto serverClient = XdgDecorationInterface::Mode::ClientSide;
    const auto serverServer = XdgDecorationInterface::Mode::ServerSide;
    const auto clientClient = XdgDecoration::Mode::ClientSide;
    const auto clientServer = XdgDecoration::Mode::ServerSide;

    QTest::newRow("client->client") << serverClient << clientClient << clientClient << serverClient;
    QTest::newRow("client->server") << serverClient << clientClient << clientServer << serverServer;
    QTest::newRow("server->client") << serverServer << clientServer << clientClient << serverClient;
    QTest::newRow("server->server") << serverServer << clientServer << clientServer << serverServer;
}

void TestXdgDecoration::testDecoration()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;

    QFETCH(Wrapland::Server::XdgDecorationInterface::Mode, configuredMode);
    QFETCH(Wrapland::Client::XdgDecoration::Mode, configuredModeExp);
    QFETCH(Wrapland::Client::XdgDecoration::Mode, setMode);
    QFETCH(Wrapland::Server::XdgDecorationInterface::Mode, setModeExp);


    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy shellSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QSignalSpy decorationCreatedSpy(m_xdgDecorationManagerInterface, &XdgDecorationManagerInterface::xdgDecorationInterfaceCreated);

    // create shell surface and deco object
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(m_xdgShell->createSurface(surface.data()));
    QScopedPointer<XdgDecoration> decoration(m_xdgDecorationManager->getToplevelDecoration(shellSurface.data()));

    //and receive all these on the "server"
    QVERIFY(surfaceCreatedSpy.count() || surfaceCreatedSpy.wait());
    QVERIFY(shellSurfaceCreatedSpy.count() || shellSurfaceCreatedSpy.wait());
    QVERIFY(decorationCreatedSpy.count() || decorationCreatedSpy.wait());

    auto shellSurfaceIface = shellSurfaceCreatedSpy.first().first().value<XdgShellSurfaceInterface*>();
    auto decorationIface = decorationCreatedSpy.first().first().value<XdgDecorationInterface*>();

    QVERIFY(decorationIface);
    QVERIFY(shellSurfaceIface);
    QCOMPARE(decorationIface->surface(), shellSurfaceIface);
    QCOMPARE(decorationIface->requestedMode(), XdgDecorationInterface::Mode::Undefined);

    QSignalSpy clientConfiguredSpy(decoration.data(), &XdgDecoration::modeChanged);
    QSignalSpy modeRequestedSpy(decorationIface, &XdgDecorationInterface::modeRequested);

    //server configuring a client
    decorationIface->configure(configuredMode);
    quint32 serial = shellSurfaceIface->configure({});
    QVERIFY(clientConfiguredSpy.wait());
    QCOMPARE(clientConfiguredSpy.first().first().value<XdgDecoration::Mode>(), configuredModeExp);

    shellSurface->ackConfigure(serial);

    //client requesting another mode
    decoration->setMode(setMode);
    QVERIFY(modeRequestedSpy.wait());
    QCOMPARE(modeRequestedSpy.first().first().value<XdgDecorationInterface::Mode>(), setModeExp);
    QCOMPARE(decorationIface->requestedMode(), setModeExp);
    modeRequestedSpy.clear();

    decoration->unsetMode();
    QVERIFY(modeRequestedSpy.wait());
    QCOMPARE(modeRequestedSpy.first().first().value<XdgDecorationInterface::Mode>(), XdgDecorationInterface::Mode::Undefined);
}

QTEST_GUILESS_MAIN(TestXdgDecoration)
#include "test_xdg_decoration.moc"
