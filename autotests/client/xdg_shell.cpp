/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2017  David Edmundson <davidedmundson@kde.org>
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
#include "../../src/client/output.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/surface.h"
#include "../../src/client/xdg_shell.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/surface.h"
#include "../../server/xdg_shell_popup.h"
#include "../../server/xdg_shell_toplevel.h"

#include <wayland-xdg-shell-client-protocol.h>

using namespace Wrapland;

class XdgShellTest : public QObject
{
    Q_OBJECT
public:
    explicit XdgShellTest(QObject* parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateSurface();
    void testTitle();
    void testWindowClass();
    void testMaximize();
    void testMinimize();
    void testFullscreen();
    void testShowWindowMenu();
    void testMove();
    void testResize_data();
    void testResize();
    void testTransient();
    void testPing();
    void testClose();
    void testConfigureStates_data();
    void testConfigureStates();
    void testConfigureMultipleAcks();

    void testMaxSize();
    void testMinSize();

    void testPopup_data();
    void testPopup();

    void testMultipleRoles1();
    void testMultipleRoles2();
    void test_role_after_buffer();

    void testWindowGeometry();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;

        Wrapland::Server::Seat* seat{nullptr};
        Server::Output* output1{nullptr};
        Server::Output* output2{nullptr};
    } server;

    Client::ConnectionThread* m_connection;
    Client::EventQueue* m_queue;
    Client::Compositor* m_compositor;
    Client::XdgShell* m_xdgShell;
    Client::ShmPool* m_shmPool;
    Client::Output* m_output1;
    Client::Output* m_output2;
    Client::Seat* m_seat;
    QThread* m_thread;
};

XdgShellTest::XdgShellTest(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_compositor(nullptr)
    , m_xdgShell(nullptr)
    , m_shmPool(nullptr)
    , m_output1(nullptr)
    , m_output2(nullptr)
    , m_seat(nullptr)
    , m_thread(nullptr)
{
    qRegisterMetaType<Server::Surface*>();
    qRegisterMetaType<Server::XdgShellToplevel*>();
    qRegisterMetaType<Server::XdgShellPopup*>();
    qRegisterMetaType<Server::Output*>();
    qRegisterMetaType<Server::Seat*>();
    qRegisterMetaType<Client::XdgShellToplevel::States>();
    qRegisterMetaType<std::string>();
    qRegisterMetaType<uint32_t>();
}

constexpr auto socket_name{"wrapland-test-xdg-shell-0"};

void XdgShellTest::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.compositor = server.display->createCompositor();
    server.globals.xdg_shell = server.display->createXdgShell();

    server.globals.outputs.push_back(
        std::make_unique<Wrapland::Server::Output>(server.display.get()));
    server.output1 = server.globals.outputs.back().get();
    server.output1->add_mode(Server::Output::Mode{QSize(1024, 768)});
    server.output1->set_enabled(true);
    server.output1->done();

    server.globals.outputs.push_back(
        std::make_unique<Wrapland::Server::Output>(server.display.get()));
    server.output2 = server.globals.outputs.back().get();
    server.output2->add_mode(Server::Output::Mode{QSize(1024, 768)});
    server.output2->set_enabled(true);
    server.output2->done();

    server.globals.seats.push_back(server.display->createSeat());
    server.seat = server.globals.seats.back().get();
    server.seat->setHasKeyboard(true);
    server.seat->setHasPointer(true);
    server.seat->setHasTouch(true);

    // Setup connection.
    m_connection = new Client::ConnectionThread;
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
    m_queue->setup(m_connection);

    Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy interfaceAnnouncedSpy(&registry, &Client::Registry::interfaceAnnounced);
    QVERIFY(interfaceAnnouncedSpy.isValid());
    QSignalSpy outputAnnouncedSpy(&registry, &Client::Registry::outputAnnounced);
    QVERIFY(outputAnnouncedSpy.isValid());

    QSignalSpy xdgShellAnnouncedSpy(&registry, &Client::Registry::xdgShellAnnounced);
    QVERIFY(xdgShellAnnouncedSpy.isValid());

    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(interfacesAnnouncedSpy.wait());

    QCOMPARE(outputAnnouncedSpy.count(), 2);
    m_output1 = registry.createOutput(outputAnnouncedSpy.first().at(0).value<quint32>(),
                                      outputAnnouncedSpy.first().at(1).value<quint32>(),
                                      this);
    m_output2 = registry.createOutput(outputAnnouncedSpy.last().at(0).value<quint32>(),
                                      outputAnnouncedSpy.last().at(1).value<quint32>(),
                                      this);

    m_shmPool = registry.createShmPool(registry.interface(Client::Registry::Interface::Shm).name,
                                       registry.interface(Client::Registry::Interface::Shm).version,
                                       this);
    QVERIFY(m_shmPool);
    QVERIFY(m_shmPool->isValid());

    m_compositor = registry.createCompositor(
        registry.interface(Client::Registry::Interface::Compositor).name,
        registry.interface(Client::Registry::Interface::Compositor).version,
        this);
    QVERIFY(m_compositor);
    QVERIFY(m_compositor->isValid());

    m_seat = registry.createSeat(registry.interface(Client::Registry::Interface::Seat).name,
                                 registry.interface(Client::Registry::Interface::Seat).version,
                                 this);
    QVERIFY(m_seat);
    QVERIFY(m_seat->isValid());

    QCOMPARE(xdgShellAnnouncedSpy.count(), 1);
    QCOMPARE(registry.interface(Client::Registry::Interface::XdgShell).version, 2);

    m_xdgShell
        = registry.createXdgShell(registry.interface(Client::Registry::Interface::XdgShell).name,
                                  registry.interface(Client::Registry::Interface::XdgShell).version,
                                  this);
    QVERIFY(m_xdgShell);
    QVERIFY(m_xdgShell->isValid());
}

void XdgShellTest::cleanup()
{
#define CLEANUP(variable)                                                                          \
    delete variable;                                                                               \
    variable = nullptr;

    CLEANUP(m_xdgShell)
    CLEANUP(m_compositor)
    CLEANUP(m_shmPool)
    CLEANUP(m_output1)
    CLEANUP(m_output2)
    CLEANUP(m_seat)
    CLEANUP(m_queue)

    Q_ASSERT(m_connection);
    m_connection->deleteLater();
    m_connection = nullptr;

    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
#undef CLEANUP

    server = {};
}

#define SURFACE                                                                                    \
    QSignalSpy xdgSurfaceCreatedSpy(server.globals.xdg_shell.get(),                                \
                                    &Server::XdgShell::toplevelCreated);                           \
    QVERIFY(xdgSurfaceCreatedSpy.isValid());                                                       \
    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());                       \
    std::unique_ptr<Client::XdgShellToplevel> xdgSurface(                                          \
        m_xdgShell->create_toplevel(surface.get()));                                               \
    QCOMPARE(xdgSurface->size(), QSize());                                                         \
    QVERIFY(xdgSurfaceCreatedSpy.wait());                                                          \
    auto serverXdgSurface                                                                          \
        = xdgSurfaceCreatedSpy.first().first().value<Server::XdgShellToplevel*>();                 \
    QVERIFY(serverXdgSurface);

void XdgShellTest::testCreateSurface()
{
    // This test verifies that we can create a surface.

    // First created the signal spies for the server.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QSignalSpy xdgSurfaceCreatedSpy(server.globals.xdg_shell.get(),
                                    &Server::XdgShell::toplevelCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());

    // create surface
    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface != nullptr);
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Server::Surface*>();
    QVERIFY(serverSurface);

    // create shell surface
    std::unique_ptr<Client::XdgShellToplevel> xdgSurface(
        m_xdgShell->create_toplevel(surface.get()));
    QVERIFY(xdgSurface != nullptr);
    QVERIFY(xdgSurfaceCreatedSpy.wait());

    // verify base things
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<Server::XdgShellToplevel*>();
    QVERIFY(serverXdgSurface);
    QCOMPARE(serverXdgSurface->configurePending(), false);
    QVERIFY(serverXdgSurface->title().empty());
    QVERIFY(serverXdgSurface->appId().empty());
    QCOMPARE(serverXdgSurface->transientFor(), nullptr);
    QCOMPARE(serverXdgSurface->surface()->surface(), serverSurface);

    // now let's destroy it
    QSignalSpy destroyedSpy(serverXdgSurface, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    xdgSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

void XdgShellTest::testTitle()
{
    // this test verifies that we can change the title of a shell surface
    // first create surface
    SURFACE

    // should not have a title yet
    QVERIFY(serverXdgSurface->title().empty());

    // lets' change the title
    QSignalSpy titleChangedSpy(serverXdgSurface, &Server::XdgShellToplevel::titleChanged);
    QVERIFY(titleChangedSpy.isValid());
    xdgSurface->setTitle(QStringLiteral("foo"));
    QVERIFY(titleChangedSpy.wait());
    QCOMPARE(titleChangedSpy.count(), 1);
    QCOMPARE(titleChangedSpy.first().first().value<std::string>(), "foo");
    QCOMPARE(serverXdgSurface->title(), "foo");
}

void XdgShellTest::testWindowClass()
{
    // this test verifies that we can change the window class/app id of a shell surface
    // first create surface
    SURFACE

    // should not have a window class yet
    QVERIFY(serverXdgSurface->appId().empty());

    // let's change the window class
    QSignalSpy windowClassChanged(serverXdgSurface, &Server::XdgShellToplevel::appIdChanged);
    QVERIFY(windowClassChanged.isValid());
    xdgSurface->setAppId(QByteArrayLiteral("org.kde.xdgsurfacetest"));
    QVERIFY(windowClassChanged.wait());
    QCOMPARE(windowClassChanged.count(), 1);
    QCOMPARE(windowClassChanged.first().first().value<std::string>(), "org.kde.xdgsurfacetest");
    QCOMPARE(serverXdgSurface->appId(), "org.kde.xdgsurfacetest");
}

void XdgShellTest::testMaximize()
{
    // this test verifies that the maximize/unmaximize calls work
    SURFACE

    QSignalSpy maximizeRequestedSpy(serverXdgSurface, &Server::XdgShellToplevel::maximizedChanged);
    QVERIFY(maximizeRequestedSpy.isValid());

    xdgSurface->setMaximized(true);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.last().first().toBool(), true);

    xdgSurface->setMaximized(false);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 2);
    QCOMPARE(maximizeRequestedSpy.last().first().toBool(), false);
}

void XdgShellTest::testMinimize()
{
    // this test verifies that the minimize request is delivered
    SURFACE

    QSignalSpy minimizeRequestedSpy(serverXdgSurface, &Server::XdgShellToplevel::minimizeRequested);
    QVERIFY(minimizeRequestedSpy.isValid());

    xdgSurface->requestMinimize();
    QVERIFY(minimizeRequestedSpy.wait());
    QCOMPARE(minimizeRequestedSpy.count(), 1);
}

void XdgShellTest::testFullscreen()
{
    // this test verifies going to/from fullscreen
    QSignalSpy xdgSurfaceCreatedSpy(server.globals.xdg_shell.get(),
                                    &Server::XdgShell::toplevelCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellToplevel> xdgSurface(
        m_xdgShell->create_toplevel(surface.get()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<Server::XdgShellToplevel*>();
    QVERIFY(serverXdgSurface);

    QSignalSpy fullscreenSpy(serverXdgSurface, &Server::XdgShellToplevel::fullscreenChanged);
    QVERIFY(fullscreenSpy.isValid());

    // without an output
    xdgSurface->setFullscreen(true, nullptr);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 1);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QVERIFY(!fullscreenSpy.last().at(1).value<Server::Output*>());

    // unset
    xdgSurface->setFullscreen(false);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 2);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), false);
    QVERIFY(!fullscreenSpy.last().at(1).value<Server::Output*>());

    // with outputs
    xdgSurface->setFullscreen(true, m_output1);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 3);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QCOMPARE(fullscreenSpy.last().at(1).value<Server::Output*>(), server.output1);

    // now other output
    xdgSurface->setFullscreen(true, m_output2);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 4);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QCOMPARE(fullscreenSpy.last().at(1).value<Server::Output*>(), server.output2);
}

void XdgShellTest::testShowWindowMenu()
{
    // this test verifies that the show window menu request works
    SURFACE

    QSignalSpy windowMenuSpy(serverXdgSurface, &Server::XdgShellToplevel::windowMenuRequested);
    QVERIFY(windowMenuSpy.isValid());

    // TODO: the serial needs to be a proper one
    xdgSurface->requestShowWindowMenu(m_seat, 20, QPoint(30, 40));
    QVERIFY(windowMenuSpy.wait());
    QCOMPARE(windowMenuSpy.count(), 1);
    QCOMPARE(windowMenuSpy.first().at(0).value<Server::Seat*>(), server.seat);
    QCOMPARE(windowMenuSpy.first().at(1).value<quint32>(), 20u);
    QCOMPARE(windowMenuSpy.first().at(2).toPoint(), QPoint(30, 40));
}

void XdgShellTest::testMove()
{
    // this test verifies that the move request works
    SURFACE

    QSignalSpy moveSpy(serverXdgSurface, &Server::XdgShellToplevel::moveRequested);
    QVERIFY(moveSpy.isValid());

    // TODO: the serial needs to be a proper one
    xdgSurface->requestMove(m_seat, 50);
    QVERIFY(moveSpy.wait());
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(moveSpy.first().at(0).value<Server::Seat*>(), server.seat);
    QCOMPARE(moveSpy.first().at(1).value<quint32>(), 50u);
}

void XdgShellTest::testResize_data()
{
    QTest::addColumn<Qt::Edges>("edges");

    QTest::newRow("none") << Qt::Edges();
    QTest::newRow("top") << Qt::Edges(Qt::TopEdge);
    QTest::newRow("bottom") << Qt::Edges(Qt::BottomEdge);
    QTest::newRow("left") << Qt::Edges(Qt::LeftEdge);
    QTest::newRow("top left") << Qt::Edges(Qt::TopEdge | Qt::LeftEdge);
    QTest::newRow("bottom left") << Qt::Edges(Qt::BottomEdge | Qt::LeftEdge);
    QTest::newRow("right") << Qt::Edges(Qt::RightEdge);
    QTest::newRow("top right") << Qt::Edges(Qt::TopEdge | Qt::RightEdge);
    QTest::newRow("bottom right") << Qt::Edges(Qt::BottomEdge | Qt::RightEdge);
}

void XdgShellTest::testResize()
{
    // this test verifies that the resize request works
    SURFACE

    QSignalSpy resizeSpy(serverXdgSurface, &Server::XdgShellToplevel::resizeRequested);
    QVERIFY(resizeSpy.isValid());

    // TODO: the serial needs to be a proper one
    QFETCH(Qt::Edges, edges);
    xdgSurface->requestResize(m_seat, 60, edges);
    QVERIFY(resizeSpy.wait());
    QCOMPARE(resizeSpy.count(), 1);
    QCOMPARE(resizeSpy.first().at(0).value<Server::Seat*>(), server.seat);
    QCOMPARE(resizeSpy.first().at(1).value<quint32>(), 60u);
    QCOMPARE(resizeSpy.first().at(2).value<Qt::Edges>(), edges);
}

void XdgShellTest::testTransient()
{
    // this test verifies that setting the transient for works
    SURFACE
    std::unique_ptr<Client::Surface> surface2(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellToplevel> xdgSurface2(
        m_xdgShell->create_toplevel(surface2.get()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface2 = xdgSurfaceCreatedSpy.last().first().value<Server::XdgShellToplevel*>();
    QVERIFY(serverXdgSurface2);

    QCOMPARE(serverXdgSurface->transientFor(), nullptr);
    QCOMPARE(serverXdgSurface2->transientFor(), nullptr);

    // now make xdsgSurface2 a transient for xdgSurface
    QSignalSpy transientForSpy(serverXdgSurface2, &Server::XdgShellToplevel::transientForChanged);
    QVERIFY(transientForSpy.isValid());
    xdgSurface2->setTransientFor(xdgSurface.get());

    QVERIFY(transientForSpy.wait());
    QCOMPARE(transientForSpy.count(), 1);
    QCOMPARE(serverXdgSurface2->transientFor(), serverXdgSurface);
    QCOMPARE(serverXdgSurface->transientFor(), nullptr);

    // unset the transient for
    xdgSurface2->setTransientFor(nullptr);
    QVERIFY(transientForSpy.wait());
    QCOMPARE(transientForSpy.count(), 2);
    QVERIFY(!serverXdgSurface2->transientFor());
    QVERIFY(!serverXdgSurface->transientFor());
}

void XdgShellTest::testPing()
{
    // This test verifies that a ping request is sent to the client.
    SURFACE

    QSignalSpy pingSpy(server.globals.xdg_shell.get(), &Server::XdgShell::pongReceived);
    QVERIFY(pingSpy.isValid());

    auto serial = server.globals.xdg_shell->ping(serverXdgSurface->client());
    QVERIFY(pingSpy.wait());
    QCOMPARE(pingSpy.count(), 1);
    QCOMPARE(pingSpy.takeFirst().at(0).value<quint32>(), serial);

    // Test of a ping failure. Disconnecting the connection thread to the queue will break the
    // connection and pings will timeout.
    disconnect(m_connection,
               &Client::ConnectionThread::eventsRead,
               m_queue,
               &Client::EventQueue::dispatch);
    server.globals.xdg_shell->ping(serverXdgSurface->client());
    QSignalSpy pingDelayedSpy(server.globals.xdg_shell.get(), &Server::XdgShell::pingDelayed);
    QVERIFY(pingDelayedSpy.wait());

    QSignalSpy pingTimeoutSpy(server.globals.xdg_shell.get(), &Server::XdgShell::pingTimeout);
    QVERIFY(pingTimeoutSpy.wait());
}

void XdgShellTest::testClose()
{
    // this test verifies that a close request is sent to the client
    SURFACE

    QSignalSpy closeSpy(xdgSurface.get(), &Client::XdgShellToplevel::closeRequested);
    QVERIFY(closeSpy.isValid());

    serverXdgSurface->close();
    QVERIFY(closeSpy.wait());
    QCOMPARE(closeSpy.count(), 1);

    QSignalSpy destroyedSpy(serverXdgSurface, &Server::XdgShellToplevel::resourceDestroyed);
    QVERIFY(destroyedSpy.isValid());
    xdgSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

void XdgShellTest::testConfigureStates_data()
{
    QTest::addColumn<Server::XdgShellSurface::States>("serverStates");
    QTest::addColumn<Client::XdgShellToplevel::States>("clientStates");

    auto const sa = Server::XdgShellSurface::States(Server::XdgShellSurface::State::Activated);
    auto const sm = Server::XdgShellSurface::States(Server::XdgShellSurface::State::Maximized);
    auto const sf = Server::XdgShellSurface::States(Server::XdgShellSurface::State::Fullscreen);
    auto const sr = Server::XdgShellSurface::States(Server::XdgShellSurface::State::Resizing);
    auto const stl = Server::XdgShellSurface::States(Server::XdgShellSurface::State::TiledLeft);
    auto const str = Server::XdgShellSurface::States(Server::XdgShellSurface::State::TiledRight);
    auto const stt = Server::XdgShellSurface::States(Server::XdgShellSurface::State::TiledTop);
    auto const stb = Server::XdgShellSurface::States(Server::XdgShellSurface::State::TiledBottom);

    auto const ca = Client::XdgShellToplevel::States(Client::XdgShellToplevel::State::Activated);
    auto const cm = Client::XdgShellToplevel::States(Client::XdgShellToplevel::State::Maximized);
    auto const cf = Client::XdgShellToplevel::States(Client::XdgShellToplevel::State::Fullscreen);
    auto const cr = Client::XdgShellToplevel::States(Client::XdgShellToplevel::State::Resizing);
    auto const ctl = Client::XdgShellToplevel::States(Client::XdgShellToplevel::State::TiledLeft);
    auto const ctr = Client::XdgShellToplevel::States(Client::XdgShellToplevel::State::TiledRight);
    auto const ctt = Client::XdgShellToplevel::States(Client::XdgShellToplevel::State::TiledTop);
    auto const ctb = Client::XdgShellToplevel::States(Client::XdgShellToplevel::State::TiledBottom);

    QTest::newRow("none") << Server::XdgShellSurface::States()
                          << Client::XdgShellToplevel::States();
    QTest::newRow("Active") << sa << ca;
    QTest::newRow("Maximize") << sm << cm;
    QTest::newRow("Fullscreen") << sf << cf;
    QTest::newRow("Resizing") << sr << cr;
    QTest::newRow("TiledLeft") << stl << ctl;
    QTest::newRow("TiledRight") << str << ctr;
    QTest::newRow("TiledTop") << stt << ctt;
    QTest::newRow("TiledBottom") << stb << ctb;

    QTest::newRow("Active/Maximize") << (sa | sm) << (ca | cm);
    QTest::newRow("Active/Fullscreen") << (sa | sf) << (ca | cf);
    QTest::newRow("Active/Resizing") << (sa | sr) << (ca | cr);
    QTest::newRow("Maximize/Fullscreen") << (sm | sf) << (cm | cf);
    QTest::newRow("Maximize/Resizing") << (sm | sr) << (cm | cr);
    QTest::newRow("Fullscreen/Resizing") << (sf | sr) << (cf | cr);
    QTest::newRow("TiledLeft/TiledRight") << (stl | str) << (ctl | ctr);
    QTest::newRow("TiledLeft/TiledTop") << (stl | stt) << (ctl | ctt);
    QTest::newRow("TiledLeft/TiledBottom") << (stl | stb) << (ctl | ctb);
    QTest::newRow("TiledRight/TiledTop") << (str | stt) << (ctr | ctt);
    QTest::newRow("TiledRight/TiledBottom") << (str | stb) << (ctr | ctb);
    QTest::newRow("TiledTop/TiledBottom") << (stt | stb) << (ctt | ctb);

    QTest::newRow("Active/Maximize/Fullscreen") << (sa | sm | sf) << (ca | cm | cf);
    QTest::newRow("Active/Maximize/Resizing") << (sa | sm | sr) << (ca | cm | cr);
    QTest::newRow("Maximize/Fullscreen|Resizing") << (sm | sf | sr) << (cm | cf | cr);

    QTest::newRow("Active/Maximize/Fullscreen/Resizing")
        << (sa | sm | sf | sr) << (ca | cm | cf | cr);
}

void XdgShellTest::testConfigureStates()
{
    // this test verifies that configure states works
    SURFACE

    QSignalSpy configureSpy(xdgSurface.get(), &Client::XdgShellToplevel::configureRequested);
    QVERIFY(configureSpy.isValid());

    QFETCH(Server::XdgShellSurface::States, serverStates);
    serverXdgSurface->configure(serverStates);
    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 1);
    QCOMPARE(configureSpy.first().at(0).toSize(), QSize(0, 0));
    QTEST(configureSpy.first().at(1).value<Client::XdgShellToplevel::States>(), "clientStates");
    QCOMPARE(configureSpy.first().at(2).value<quint32>(), server.display->serial());

    QSignalSpy ackSpy(serverXdgSurface, &Server::XdgShellToplevel::configureAcknowledged);
    QVERIFY(ackSpy.isValid());

    xdgSurface->ackConfigure(configureSpy.first().at(2).value<quint32>());
    QVERIFY(ackSpy.wait());
    QCOMPARE(ackSpy.count(), 1);
    QCOMPARE(ackSpy.first().first().value<quint32>(), configureSpy.first().at(2).value<quint32>());
}

void XdgShellTest::testConfigureMultipleAcks()
{
    // this test verifies that with multiple configure requests the last acknowledged one
    // acknowledges all
    SURFACE

    QSignalSpy configureSpy(xdgSurface.get(), &Client::XdgShellToplevel::configureRequested);
    QVERIFY(configureSpy.isValid());
    QSignalSpy sizeChangedSpy(xdgSurface.get(), &Client::XdgShellToplevel::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QSignalSpy ackSpy(serverXdgSurface, &Server::XdgShellToplevel::configureAcknowledged);
    QVERIFY(ackSpy.isValid());

    serverXdgSurface->configure(Server::XdgShellSurface::States(), QSize(10, 20));
    auto const serial1 = server.display->serial();
    serverXdgSurface->configure(Server::XdgShellSurface::States(), QSize(20, 30));
    auto const serial2 = server.display->serial();
    QVERIFY(serial1 != serial2);
    serverXdgSurface->configure(Server::XdgShellSurface::States(), QSize(30, 40));
    auto const serial3 = server.display->serial();
    QVERIFY(serial1 != serial3);
    QVERIFY(serial2 != serial3);

    QVERIFY(configureSpy.wait());
    QTRY_COMPARE(configureSpy.count(), 3);
    QVERIFY(!configureSpy.wait(100));

    QCOMPARE(configureSpy.at(0).at(0).toSize(), QSize(10, 20));
    QCOMPARE(configureSpy.at(0).at(1).value<Client::XdgShellToplevel::States>(),
             Client::XdgShellToplevel::States());
    QCOMPARE(configureSpy.at(0).at(2).value<quint32>(), serial1);
    QCOMPARE(configureSpy.at(1).at(0).toSize(), QSize(20, 30));
    QCOMPARE(configureSpy.at(1).at(1).value<Client::XdgShellToplevel::States>(),
             Client::XdgShellToplevel::States());
    QCOMPARE(configureSpy.at(1).at(2).value<quint32>(), serial2);
    QCOMPARE(configureSpy.at(2).at(0).toSize(), QSize(30, 40));
    QCOMPARE(configureSpy.at(2).at(1).value<Client::XdgShellToplevel::States>(),
             Client::XdgShellToplevel::States());
    QCOMPARE(configureSpy.at(2).at(2).value<quint32>(), serial3);
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(sizeChangedSpy.at(0).at(0).toSize(), QSize(10, 20));
    QCOMPARE(sizeChangedSpy.at(1).at(0).toSize(), QSize(20, 30));
    QCOMPARE(sizeChangedSpy.at(2).at(0).toSize(), QSize(30, 40));
    QCOMPARE(xdgSurface->size(), QSize(30, 40));

    xdgSurface->ackConfigure(serial3);
    QVERIFY(ackSpy.wait());
    QCOMPARE(ackSpy.count(), 3);
    QCOMPARE(ackSpy.at(0).first().value<quint32>(), serial1);
    QCOMPARE(ackSpy.at(1).first().value<quint32>(), serial2);
    QCOMPARE(ackSpy.at(2).first().value<quint32>(), serial3);

    // configure once more with a null size
    serverXdgSurface->configure(Server::XdgShellSurface::States());
    // should not change size
    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 4);
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(xdgSurface->size(), QSize(30, 40));
}

void XdgShellTest::testMaxSize()
{
    // this test verifies changing the window maxSize

    QSignalSpy xdgSurfaceCreatedSpy(server.globals.xdg_shell.get(),
                                    &Server::XdgShell::toplevelCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());

    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellToplevel> xdgSurface(
        m_xdgShell->create_toplevel(surface.get()));

    QVERIFY(xdgSurfaceCreatedSpy.wait());

    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<Server::XdgShellToplevel*>();
    QVERIFY(serverXdgSurface);

    QSignalSpy maxSizeSpy(serverXdgSurface, &Server::XdgShellToplevel::maxSizeChanged);
    QVERIFY(maxSizeSpy.isValid());

    xdgSurface->setMaxSize(QSize(100, 100));
    surface->commit(Client::Surface::CommitFlag::None);
    QVERIFY(maxSizeSpy.wait());
    QCOMPARE(maxSizeSpy.count(), 1);
    QCOMPARE(maxSizeSpy.last().at(0).value<QSize>(), QSize(100, 100));
    QCOMPARE(serverXdgSurface->maximumSize(), QSize(100, 100));

    xdgSurface->setMaxSize(QSize(200, 200));
    surface->commit(Client::Surface::CommitFlag::None);
    QVERIFY(maxSizeSpy.wait());
    QCOMPARE(maxSizeSpy.count(), 2);
    QCOMPARE(maxSizeSpy.last().at(0).value<QSize>(), QSize(200, 200));
    QCOMPARE(serverXdgSurface->maximumSize(), QSize(200, 200));
}

void XdgShellTest::testMinSize()
{
    // This test verifies changing the window minSize.
    QSignalSpy xdgSurfaceCreatedSpy(server.globals.xdg_shell.get(),
                                    &Server::XdgShell::toplevelCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellToplevel> xdgSurface(
        m_xdgShell->create_toplevel(surface.get()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<Server::XdgShellToplevel*>();
    QVERIFY(serverXdgSurface);

    QSignalSpy minSizeSpy(serverXdgSurface, &Server::XdgShellToplevel::minSizeChanged);
    QVERIFY(minSizeSpy.isValid());

    xdgSurface->setMinSize(QSize(200, 200));
    surface->commit(Client::Surface::CommitFlag::None);
    QVERIFY(minSizeSpy.wait());
    QCOMPARE(minSizeSpy.count(), 1);
    QCOMPARE(minSizeSpy.last().at(0).value<QSize>(), QSize(200, 200));
    QCOMPARE(serverXdgSurface->minimumSize(), QSize(200, 200));

    xdgSurface->setMinSize(QSize(100, 100));
    surface->commit(Client::Surface::CommitFlag::None);
    QVERIFY(minSizeSpy.wait());
    QCOMPARE(minSizeSpy.count(), 2);
    QCOMPARE(minSizeSpy.last().at(0).value<QSize>(), QSize(100, 100));
    QCOMPARE(serverXdgSurface->minimumSize(), QSize(100, 100));
}

void XdgShellTest::testPopup_data()
{
    QTest::addColumn<Client::xdg_shell_positioner_data>("positioner_data");

    Client::xdg_shell_positioner_data pos_data;
    pos_data.size = QSize(10, 10);
    pos_data.anchor.rect = QRect(100, 100, 50, 50);

    QTest::newRow("default") << pos_data;

    Client::xdg_shell_positioner_data pos_data2;
    pos_data.size = QSize(20, 20);
    pos_data.anchor.rect = QRect(101, 102, 51, 52);
    QTest::newRow("sizeAndAnchorRect") << pos_data2;

    pos_data.anchor.edge = Qt::TopEdge | Qt::RightEdge;
    QTest::newRow("anchorEdge") << pos_data;

    pos_data.gravity = Qt::BottomEdge;
    QTest::newRow("gravity") << pos_data;

    pos_data.gravity = Qt::TopEdge | Qt::RightEdge;
    QTest::newRow("gravity2") << pos_data;

    using constraints = Client::xdg_shell_constraint_adjustment;
    pos_data.constraint_adjustments = constraints::slide_x | constraints::flip_y;
    QTest::newRow("constraints") << pos_data;

    pos_data.constraint_adjustments = constraints::slide_x | constraints::slide_y
        | constraints::flip_x | constraints::flip_y | constraints::resize_x | constraints::resize_y;
    QTest::newRow("constraints2") << pos_data;

    pos_data.anchor.offset = QPoint(4, 5);
    QTest::newRow("offset") << pos_data;
}

void XdgShellTest::testPopup()
{
    QSignalSpy xdgTopLevelCreatedSpy(server.globals.xdg_shell.get(),
                                     &Server::XdgShell::toplevelCreated);
    QSignalSpy popupCreatedSpy(server.globals.xdg_shell.get(), &Server::XdgShell::popupCreated);

    std::unique_ptr<Client::Surface> parentSurface(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellToplevel> xdgParentSurface(
        m_xdgShell->create_toplevel(parentSurface.get()));

    QVERIFY(xdgTopLevelCreatedSpy.wait());
    auto serverXdgTopLevel
        = xdgTopLevelCreatedSpy.first().first().value<Server::XdgShellToplevel*>();

    QFETCH(Client::xdg_shell_positioner_data, positioner_data);

    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellPopup> xdgSurface(
        m_xdgShell->create_popup(surface.get(), xdgParentSurface.get(), positioner_data));
    QVERIFY(popupCreatedSpy.wait());
    auto serverXdgPopup = popupCreatedSpy.first().first().value<Server::XdgShellPopup*>();
    QVERIFY(serverXdgPopup);

    QCOMPARE(serverXdgPopup->get_positioner().size, positioner_data.size);
    QCOMPARE(serverXdgPopup->get_positioner().anchor.rect, positioner_data.anchor.rect);
    QCOMPARE(serverXdgPopup->get_positioner().anchor.edge, positioner_data.anchor.edge);
    QCOMPARE(serverXdgPopup->get_positioner().gravity, positioner_data.gravity);
    QCOMPARE(serverXdgPopup->get_positioner().anchor.offset, positioner_data.anchor.offset);

    // We have different enums for client and server, but they share the same values.
    QCOMPARE((int)serverXdgPopup->get_positioner().constraint_adjustments,
             (int)positioner_data.constraint_adjustments);

    QCOMPARE(serverXdgPopup->transientFor(), serverXdgTopLevel->surface());
}

// top level then toplevel
void XdgShellTest::testMultipleRoles1()
{
    // setting multiple roles on an xdg surface should fail
    QSignalSpy xdgSurfaceCreatedSpy(server.globals.xdg_shell.get(),
                                    &Server::XdgShell::toplevelCreated);
    QSignalSpy popupCreatedSpy(server.globals.xdg_shell.get(), &Server::XdgShell::popupCreated);

    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QVERIFY(popupCreatedSpy.isValid());

    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    // This is testing we work when a client does something stupid
    // we can't use Wrapland API here because by design that stops you from doing anything stupid

    qDebug() << (xdg_wm_base*)*m_xdgShell;
    auto xdgSurface = xdg_wm_base_get_xdg_surface(*m_xdgShell, *surface.get());

    // create a top level
    auto xdgTopLevel1 = xdg_surface_get_toplevel(xdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());

    // now try to create another top level for the same xdg surface. It should fail
    auto xdgTopLevel2 = xdg_surface_get_toplevel(xdgSurface);
    QVERIFY(!xdgSurfaceCreatedSpy.wait(10));

    xdg_toplevel_destroy(xdgTopLevel1);
    xdg_toplevel_destroy(xdgTopLevel2);
    xdg_surface_destroy(xdgSurface);
}

// toplevel then popup
void XdgShellTest::testMultipleRoles2()
{
    QSignalSpy xdgSurfaceCreatedSpy(server.globals.xdg_shell.get(),
                                    &Server::XdgShell::toplevelCreated);
    QSignalSpy popupCreatedSpy(server.globals.xdg_shell.get(), &Server::XdgShell::popupCreated);

    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QVERIFY(popupCreatedSpy.isValid());

    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Client::Surface> parentSurface(m_compositor->createSurface());

    auto parentXdgSurface = xdg_wm_base_get_xdg_surface(*m_xdgShell, *parentSurface.get());
    auto xdgTopLevelParent = xdg_surface_get_toplevel(parentXdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());

    auto xdgSurface = xdg_wm_base_get_xdg_surface(*m_xdgShell, *surface.get());
    // create a top level
    auto xdgTopLevel1 = xdg_surface_get_toplevel(xdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());

    // now try to create a popup on the same xdg surface. It should fail

    auto positioner = xdg_wm_base_create_positioner(*m_xdgShell);
    xdg_positioner_set_anchor_rect(positioner, 10, 10, 10, 10);
    xdg_positioner_set_size(positioner, 10, 100);

    auto xdgPopup2 = xdg_surface_get_popup(xdgSurface, parentXdgSurface, positioner);
    QVERIFY(!popupCreatedSpy.wait(10));

    xdg_positioner_destroy(positioner);
    xdg_toplevel_destroy(xdgTopLevel1);
    xdg_toplevel_destroy(xdgTopLevelParent);
    xdg_popup_destroy(xdgPopup2);
    xdg_surface_destroy(xdgSurface);
    xdg_surface_destroy(parentXdgSurface);
}

void XdgShellTest::test_role_after_buffer()
{
    QSignalSpy surface_spy(server.globals.compositor.get(), &Server::Compositor::surfaceCreated);
    QVERIFY(surface_spy.isValid());
    QSignalSpy xdg_toplevel_spy(server.globals.xdg_shell.get(), &Server::XdgShell::toplevelCreated);
    QVERIFY(xdg_toplevel_spy.isValid());

    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    auto xdg_surface = xdg_wm_base_get_xdg_surface(*m_xdgShell, *surface.get());

    QVERIFY(surface_spy.wait());
    auto server_surface = surface_spy.first().first().value<Server::Surface*>();

    QSignalSpy commit_spy(server_surface, &Server::Surface::committed);
    QVERIFY(commit_spy.isValid());

    // Now commit a buffer before getting the toplevel from the surface.
    auto img = QImage(QSize(300, 300), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto buffer = m_shmPool->createBuffer(img);

    surface->attachBuffer(buffer);
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(commit_spy.wait());

    auto xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    QVERIFY(!xdg_toplevel_spy.wait(100));

    xdg_toplevel_destroy(xdg_toplevel);
    xdg_surface_destroy(xdg_surface);
}

void XdgShellTest::testWindowGeometry()
{
    QSignalSpy toplevel_created_spy(server.globals.xdg_shell.get(),
                                    &Server::XdgShell::toplevelCreated);
    QVERIFY(toplevel_created_spy.isValid());

    std::unique_ptr<Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellToplevel> toplevel(m_xdgShell->create_toplevel(surface.get()));

    QVERIFY(toplevel_created_spy.wait());

    auto server_toplevel = toplevel_created_spy.first().first().value<Server::XdgShellToplevel*>();
    QVERIFY(server_toplevel);

    QSignalSpy window_geometry_changed_spy(server_toplevel->surface(),
                                           &Server::XdgShellSurface::window_geometry_changed);
    toplevel->setWindowGeometry(QRect(50, 50, 400, 400));
    surface->commit(Client::Surface::CommitFlag::None);
    QVERIFY(window_geometry_changed_spy.wait());

    // Window geometry is still invalid since surface has no size.
    QVERIFY(!server_toplevel->surface()->window_geometry().isValid());

    // Create a buffer for the surface.
    QSignalSpy toplevel_commit_spy(server_toplevel->surface()->surface(),
                                   &Server::Surface::committed);

    QImage img(QSize(500, 500), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto buffer = m_shmPool->createBuffer(img);

    // The x/y-coordinates of the attachement should not make a difference.
    surface->attachBuffer(buffer, QPoint(55, 55));
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(toplevel_commit_spy.wait());
    QCOMPARE(toplevel_commit_spy.count(), 1);
    QVERIFY(server_toplevel->surface()->surface()->isMapped());

    // Now the window geometry is the full size.
    QCOMPARE(server_toplevel->surface()->window_geometry(), QRect(50, 50, 400, 400));

    // Reduce the size of the surface.
    img = QImage(QSize(300, 300), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    buffer = m_shmPool->createBuffer(img);

    // The x/y-coordinates of the attachement should not make a difference.
    surface->attachBuffer(buffer, QPoint(55, 55));
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(toplevel_commit_spy.wait());
    QCOMPARE(toplevel_commit_spy.count(), 2);

    // Now the window geometry is clamped to the surface size.
    QCOMPARE(server_toplevel->surface()->window_geometry(), QRect(50, 50, 250, 250));

    // Add a popup to this toplevel.
    Client::xdg_shell_positioner_data positioner_data;
    positioner_data.size = QSize(10, 10);
    positioner_data.anchor.rect = QRect(100, 100, 50, 50);

    QSignalSpy popup_created_spy(server.globals.xdg_shell.get(), &Server::XdgShell::popupCreated);
    std::unique_ptr<Client::Surface> popup_surface(m_compositor->createSurface());
    std::unique_ptr<Client::XdgShellPopup> popup(
        m_xdgShell->create_popup(popup_surface.get(), toplevel.get(), positioner_data));
    QVERIFY(popup_created_spy.wait());
    auto server_popup = popup_created_spy.first().first().value<Server::XdgShellPopup*>();
    QVERIFY(server_popup);

    QSignalSpy popupWindowGeometryChangedSpy(server_popup->surface(),
                                             &Server::XdgShellSurface::window_geometry_changed);
    popup->setWindowGeometry(QRect(60, 60, 300, 300));
    popup_surface->commit(Client::Surface::CommitFlag::None);
    QVERIFY(popupWindowGeometryChangedSpy.wait());

    // Window geometry is still invalid since surface has no size.
    QVERIFY(!server_popup->surface()->window_geometry().isValid());

    // Create a buffer for the surface.
    QSignalSpy popup_commit_spy(server_popup->surface()->surface(), &Server::Surface::committed);
    img = QImage(QSize(300, 300), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    buffer = m_shmPool->createBuffer(img);

    // The x/y-coordinates of the attachement should not make a difference.
    popup_surface->attachBuffer(buffer, QPoint(55, 55));
    popup_surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(popup_commit_spy.wait());
    QCOMPARE(popup_commit_spy.count(), 1);

    // Window geometry is clamped to the surface size.
    QCOMPARE(server_popup->surface()->window_geometry(), QRect(60, 60, 240, 240));
}

QTEST_GUILESS_MAIN(XdgShellTest)
#include "xdg_shell.moc"
