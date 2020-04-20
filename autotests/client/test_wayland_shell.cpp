/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "../../src/client/pointer.h"
#include "../../src/client/seat.h"
#include "../../src/client/shell.h"
#include "../../src/client/surface.h"
#include "../../src/client/registry.h"

#include "../../src/server/buffer_interface.h"
#include "../../src/server/clientconnection.h"
#include "../../src/server/shell_interface.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

#include <wayland-client-protocol.h>

Q_DECLARE_METATYPE(Qt::Edges)

class TestWaylandShell : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandShell(QObject *parent = nullptr);
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testCreateMultiple();
    void testFullscreen();
    void testMaximize();
    void testToplevel();
    void testTransient_data();
    void testTransient();
    void testTransientPopup();
    void testPing();
    void testTitle();
    void testWindowClass();
    void testDestroy();
    void testCast();
    void testMove();
    void testResize_data();
    void testResize();
    void testDisconnect();
    void testWhileDestroying();
    void testClientDisconnecting();

private:
    Wrapland::Server::D_isplay *m_display;
    Wrapland::Server::Compositor *m_serverCompositor;
    Wrapland::Server::ShellInterface *m_shellInterface;
    Wrapland::Server::Seat* m_serverSeat;
    Wrapland::Client::ConnectionThread *m_connection;
    Wrapland::Client::Compositor *m_compositor;
    Wrapland::Client::Shell *m_shell;
    Wrapland::Client::Seat *m_seat;
    Wrapland::Client::Pointer *m_pointer;
    Wrapland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-shell-0");

TestWaylandShell::TestWaylandShell(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverCompositor(nullptr)
    , m_shellInterface(nullptr)
    , m_serverSeat(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_shell(nullptr)
    , m_seat(nullptr)
    , m_pointer(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandShell::initTestCase()
{
    qRegisterMetaType<Qt::Edges>();
}

void TestWaylandShell::init()
{
    using namespace Wrapland::Server;
    delete m_display;
    m_display = new D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_serverCompositor = m_display->createCompositor(m_display);
    QVERIFY(m_serverCompositor);

    m_shellInterface = m_display->createShell(m_display);
    QVERIFY(m_shellInterface);
    m_shellInterface->create();
    QVERIFY(m_shellInterface->isValid());

    m_serverSeat = m_display->createSeat(m_display);
    QVERIFY(m_serverSeat);
    m_serverSeat->setHasPointer(true);

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
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

    using namespace Wrapland::Client;
    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QSignalSpy shellSpy(&registry, SIGNAL(shellAnnounced(quint32,quint32)));
    QSignalSpy seatAnnouncedSpy(&registry, &Registry::seatAnnounced);
    QVERIFY(seatAnnouncedSpy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor = new Wrapland::Client::Compositor(this);
    m_compositor->setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_compositor->isValid());

    if (shellSpy.isEmpty()) {
        QVERIFY(shellSpy.wait());
    }
    m_shell = registry.createShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shell->isValid());

    QVERIFY(!seatAnnouncedSpy.isEmpty());
    m_seat = registry.createSeat(seatAnnouncedSpy.first().first().value<quint32>(), seatAnnouncedSpy.first().last().value<quint32>(), this);
    QVERIFY(seatAnnouncedSpy.isValid());
    QSignalSpy hasPointerSpy(m_seat, &Wrapland::Client::Seat::hasPointerChanged);
    QVERIFY(hasPointerSpy.isValid());
    QVERIFY(hasPointerSpy.wait());
    QVERIFY(hasPointerSpy.first().first().toBool());
    m_pointer = m_seat->createPointer(m_seat);
    QVERIFY(m_pointer->isValid());
}

void TestWaylandShell::cleanup()
{
    if (m_pointer) {
        delete m_pointer;
        m_pointer = nullptr;
    }
    if (m_seat) {
        delete m_seat;
        m_seat = nullptr;
    }
    if (m_shell) {
        delete m_shell;
        m_shell = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
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

    delete m_serverSeat;
    m_serverSeat = nullptr;

    delete m_shellInterface;
    m_shellInterface = nullptr;

    delete m_serverCompositor;
    m_serverCompositor = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestWaylandShell::testCreateMultiple()
{
    std::unique_ptr<Wrapland::Client::Surface> s1(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> s2(m_compositor->createSurface());
    QVERIFY(s1 != nullptr);
    QVERIFY(s1->isValid());
    QVERIFY(s2 != nullptr);
    QVERIFY(s2->isValid());

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(Wrapland::Server::Wrapland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    std::unique_ptr<Wrapland::Client::ShellSurface> surface1(m_shell->createSurface(s1.get()));
    QVERIFY(surface1 != nullptr);
    QVERIFY(surface1->isValid());
    QVERIFY(!Wrapland::Client::ShellSurface::get(nullptr));
    QCOMPARE(Wrapland::Client::ShellSurface::get(*(surface1.get())), surface1.get());

    QVERIFY(serverSurfaceSpy.wait());
    QCOMPARE(serverSurfaceSpy.count(), 1);

    std::unique_ptr<Wrapland::Client::ShellSurface> surface2(m_shell->createSurface(s2.get()));
    QVERIFY(surface2 != nullptr);
    QVERIFY(surface2->isValid());
    QCOMPARE(Wrapland::Client::ShellSurface::get(*(surface2.get())), surface2.get());

    QVERIFY(serverSurfaceSpy.wait());
    QCOMPARE(serverSurfaceSpy.count(), 2);

    // try creating for one which already exist should not be possible
    std::unique_ptr<Wrapland::Client::ShellSurface> surface3(m_shell->createSurface(s2.get()));
    QVERIFY(surface3 != nullptr);
    QVERIFY(surface3->isValid());
    QCOMPARE(Wrapland::Client::ShellSurface::get(*(surface3.get())), surface3.get());

    QVERIFY(!serverSurfaceSpy.wait(100));
    QCOMPARE(serverSurfaceSpy.count(), 2);
}

void TestWaylandShell::testFullscreen()
{
    using namespace Wrapland::Server;
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);
    QSignalSpy sizeSpy(surface, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeSpy.isValid());
    QCOMPARE(surface->size(), QSize());

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(Wrapland::Server::Wrapland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->parentResource());
    QCOMPARE(serverSurface->shell(), m_shellInterface);

    QSignalSpy fullscreenSpy(serverSurface, SIGNAL(fullscreenChanged(bool)));
    QVERIFY(fullscreenSpy.isValid());

    QVERIFY(!serverSurface->isFullscreen());
    surface->setFullscreen();
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 1);
    QVERIFY(fullscreenSpy.first().first().toBool());
    QVERIFY(serverSurface->isFullscreen());
    serverSurface->requestSize(QSize(1024, 768));

    QVERIFY(sizeSpy.wait());
    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(sizeSpy.first().first().toSize(), QSize(1024, 768));
    QCOMPARE(surface->size(), QSize(1024, 768));

    // set back to toplevel
    fullscreenSpy.clear();
    wl_shell_surface_set_toplevel(*surface);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 1);
    QVERIFY(!fullscreenSpy.first().first().toBool());
    QVERIFY(!serverSurface->isFullscreen());
}

void TestWaylandShell::testMaximize()
{
    using namespace Wrapland::Server;
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);
    QSignalSpy sizeSpy(surface, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeSpy.isValid());
    QCOMPARE(surface->size(), QSize());

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(Wrapland::Server::Wrapland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->parentResource());

    QSignalSpy maximizedSpy(serverSurface, SIGNAL(maximizedChanged(bool)));
    QVERIFY(maximizedSpy.isValid());

    QVERIFY(!serverSurface->isMaximized());
    surface->setMaximized();
    QVERIFY(maximizedSpy.wait());
    QCOMPARE(maximizedSpy.count(), 1);
    QVERIFY(maximizedSpy.first().first().toBool());
    QVERIFY(serverSurface->isMaximized());
    serverSurface->requestSize(QSize(1024, 768));

    QVERIFY(sizeSpy.wait());
    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(sizeSpy.first().first().toSize(), QSize(1024, 768));
    QCOMPARE(surface->size(), QSize(1024, 768));

    // set back to toplevel
    maximizedSpy.clear();
    wl_shell_surface_set_toplevel(*surface);
    QVERIFY(maximizedSpy.wait());
    QCOMPARE(maximizedSpy.count(), 1);
    QVERIFY(!maximizedSpy.first().first().toBool());
    QVERIFY(!serverSurface->isMaximized());
}

void TestWaylandShell::testToplevel()
{
    using namespace Wrapland::Server;
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);
    QSignalSpy sizeSpy(surface, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeSpy.isValid());
    QCOMPARE(surface->size(), QSize());

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(Wrapland::Server::Wrapland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->parentResource());

    QSignalSpy toplevelSpy(serverSurface, SIGNAL(toplevelChanged(bool)));
    QVERIFY(toplevelSpy.isValid());

    surface->setFullscreen();
    QVERIFY(toplevelSpy.wait());
    QCOMPARE(toplevelSpy.count(), 1);
    QVERIFY(!toplevelSpy.first().first().toBool());
    toplevelSpy.clear();
    surface->setToplevel();
    QVERIFY(toplevelSpy.wait());
    QCOMPARE(toplevelSpy.count(), 1);
    QVERIFY(toplevelSpy.first().first().toBool());
    serverSurface->requestSize(QSize(1024, 768));

    QVERIFY(sizeSpy.wait());
    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(sizeSpy.first().first().toSize(), QSize(1024, 768));
    QCOMPARE(surface->size(), QSize(1024, 768));

    // set back to fullscreen
    toplevelSpy.clear();
    surface->setFullscreen();
    QVERIFY(toplevelSpy.wait());
    QCOMPARE(toplevelSpy.count(), 1);
    QVERIFY(!toplevelSpy.first().first().toBool());
}

void TestWaylandShell::testTransient_data()
{
    QTest::addColumn<bool>("keyboardFocus");

    QTest::newRow("focus") << true;
    QTest::newRow("no focus") << false;
}

void TestWaylandShell::testTransient()
{
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);

    QSignalSpy serverSurfaceSpy(m_shellInterface, &Wrapland::Server::ShellInterface::surfaceCreated);
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QSignalSpy acceptsKeyboardFocusChangedSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::acceptsKeyboardFocusChanged);
    QVERIFY(acceptsKeyboardFocusChangedSpy.isValid());
    QCOMPARE(serverSurface->isToplevel(), true);
    QCOMPARE(serverSurface->isPopup(), false);
    QCOMPARE(serverSurface->isTransient(), false);
    QCOMPARE(serverSurface->transientFor(), QPointer<Wrapland::Server::Surface>());
    QCOMPARE(serverSurface->transientOffset(), QPoint());
    QVERIFY(serverSurface->acceptsKeyboardFocus());
    QVERIFY(acceptsKeyboardFocusChangedSpy.isEmpty());

    QSignalSpy transientSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::transientChanged);
    QVERIFY(transientSpy.isValid());
    QSignalSpy transientOffsetSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::transientOffsetChanged);
    QVERIFY(transientOffsetSpy.isValid());
    QSignalSpy transientForChangedSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::transientForChanged);
    QVERIFY(transientForChangedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s2(m_compositor->createSurface());
    m_shell->createSurface(s2.get(), m_shell);
    serverSurfaceSpy.clear();
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface2 = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface2 != serverSurface);
    QVERIFY(serverSurface2);
    QVERIFY(serverSurface2->acceptsKeyboardFocus());

    QFETCH(bool, keyboardFocus);
    surface->setTransient(s2.get(), QPoint(10, 20), keyboardFocus ? Wrapland::Client::ShellSurface::TransientFlag::Default : Wrapland::Client::ShellSurface::TransientFlag::NoFocus);
    QVERIFY(transientSpy.wait());
    QCOMPARE(transientSpy.count(), 1);
    QCOMPARE(transientSpy.first().first().toBool(), true);
    QCOMPARE(transientOffsetSpy.count(), 1);
    QCOMPARE(transientOffsetSpy.first().first().toPoint(), QPoint(10, 20));
    QCOMPARE(transientForChangedSpy.count(), 1);
    QCOMPARE(serverSurface->isToplevel(), true);
    QCOMPARE(serverSurface->isPopup(), false);
    QCOMPARE(serverSurface->isTransient(), true);
    QCOMPARE(serverSurface->transientFor(), QPointer<Wrapland::Server::Surface>(serverSurface2->surface()));
    QCOMPARE(serverSurface->transientOffset(), QPoint(10, 20));
    QCOMPARE(serverSurface->acceptsKeyboardFocus(), keyboardFocus);
    QCOMPARE(acceptsKeyboardFocusChangedSpy.isEmpty(), keyboardFocus);
    QCOMPARE(acceptsKeyboardFocusChangedSpy.count(), keyboardFocus ? 0 : 1);

    QCOMPARE(serverSurface2->isToplevel(), true);
    QCOMPARE(serverSurface2->isPopup(), false);
    QCOMPARE(serverSurface2->isTransient(), false);
    QCOMPARE(serverSurface2->transientFor(), QPointer<Wrapland::Server::Surface>());
    QCOMPARE(serverSurface2->transientOffset(), QPoint());
    QVERIFY(serverSurface2->acceptsKeyboardFocus());
}

void TestWaylandShell::testTransientPopup()
{
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);

    QSignalSpy serverSurfaceSpy(m_shellInterface, &Wrapland::Server::ShellInterface::surfaceCreated);
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->isToplevel(), true);
    QCOMPARE(serverSurface->isPopup(), false);
    QCOMPARE(serverSurface->isTransient(), false);
    QCOMPARE(serverSurface->transientFor(), QPointer<Wrapland::Server::Surface>());
    QCOMPARE(serverSurface->transientOffset(), QPoint());
    QVERIFY(serverSurface->acceptsKeyboardFocus());

    QSignalSpy transientSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::transientChanged);
    QVERIFY(transientSpy.isValid());
    QSignalSpy transientOffsetSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::transientOffsetChanged);
    QVERIFY(transientOffsetSpy.isValid());
    QSignalSpy transientForChangedSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::transientForChanged);
    QVERIFY(transientForChangedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s2(m_compositor->createSurface());
    m_shell->createSurface(s2.get(), m_shell);
    serverSurfaceSpy.clear();
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface2 = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface2 != serverSurface);
    QVERIFY(serverSurface2);
    QVERIFY(serverSurface2->acceptsKeyboardFocus());

    // TODO: proper serial checking
    surface->setTransientPopup(s2.get(), m_seat, 1, QPoint(10, 20));
    QVERIFY(transientSpy.wait());
    QCOMPARE(transientSpy.count(), 1);
    QCOMPARE(transientSpy.first().first().toBool(), true);
    QCOMPARE(transientOffsetSpy.count(), 1);
    QCOMPARE(transientOffsetSpy.first().first().toPoint(), QPoint(10, 20));
    QCOMPARE(transientForChangedSpy.count(), 1);
    QCOMPARE(serverSurface->isToplevel(), false);
    QCOMPARE(serverSurface->isPopup(), true);
    QCOMPARE(serverSurface->isTransient(), true);
    QCOMPARE(serverSurface->transientFor(), QPointer<Wrapland::Server::Surface>(serverSurface2->surface()));
    QCOMPARE(serverSurface->transientOffset(), QPoint(10, 20));
    // TODO: honor the flag
    QCOMPARE(serverSurface->acceptsKeyboardFocus(), false);

    QCOMPARE(serverSurface2->isToplevel(), true);
    QCOMPARE(serverSurface2->isPopup(), false);
    QCOMPARE(serverSurface2->isTransient(), false);
    QCOMPARE(serverSurface2->transientFor(), QPointer<Wrapland::Server::Surface>());
    QCOMPARE(serverSurface2->transientOffset(), QPoint());

    // send popup done
    QSignalSpy popupDoneSpy(surface, &Wrapland::Client::ShellSurface::popupDone);
    QVERIFY(popupDoneSpy.isValid());
    serverSurface->popupDone();
    QVERIFY(popupDoneSpy.wait());
}

void TestWaylandShell::testPing()
{
    using namespace Wrapland::Server;
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);
    QSignalSpy pingSpy(surface, SIGNAL(pinged()));

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(Wrapland::Server::Wrapland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(!serverSurface->isPinged());

    QSignalSpy pingTimeoutSpy(serverSurface, SIGNAL(pingTimeout()));
    QVERIFY(pingTimeoutSpy.isValid());
    QSignalSpy pongSpy(serverSurface, SIGNAL(pongReceived()));
    QVERIFY(pongSpy.isValid());

    serverSurface->ping();
    QVERIFY(serverSurface->isPinged());
    QVERIFY(pingSpy.wait());
    wl_display_flush(m_connection->display());

    if (pongSpy.isEmpty()) {
        QVERIFY(pongSpy.wait());
    }
    QVERIFY(!pongSpy.isEmpty());
    QVERIFY(pingTimeoutSpy.isEmpty());

    // evil trick - timeout of zero will make it not get the pong
    serverSurface->setPingTimeout(0);
    pongSpy.clear();
    pingTimeoutSpy.clear();
    serverSurface->ping();
    QTest::qWait(100);
    if (pingTimeoutSpy.isEmpty()) {
        QVERIFY(pingTimeoutSpy.wait());
    }
    QCOMPARE(pingTimeoutSpy.count(), 1);
    QVERIFY(pongSpy.isEmpty());
}

void TestWaylandShell::testTitle()
{
    using namespace Wrapland::Server;
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(Wrapland::Server::Wrapland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy titleSpy(serverSurface, SIGNAL(titleChanged(QString)));
    QVERIFY(titleSpy.isValid());
    QString testTitle = QStringLiteral("fooBar");
    QVERIFY(serverSurface->title().isNull());

    surface->setTitle(testTitle);
    QVERIFY(titleSpy.wait());
    QCOMPARE(serverSurface->title(), testTitle);
    QCOMPARE(titleSpy.first().first().toString(), testTitle);
}

void TestWaylandShell::testWindowClass()
{
    using namespace Wrapland::Server;
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);

    QSignalSpy serverSurfaceSpy(m_shellInterface, SIGNAL(surfaceCreated(Wrapland::Server::Wrapland::Server::ShellSurfaceInterface*)));
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy windowClassSpy(serverSurface, SIGNAL(windowClassChanged(QByteArray)));
    QVERIFY(windowClassSpy.isValid());
    QByteArray testClass = QByteArrayLiteral("fooBar");
    QVERIFY(serverSurface->windowClass().isNull());

    surface->setWindowClass(testClass);
    QVERIFY(windowClassSpy.wait());
    QCOMPARE(serverSurface->windowClass(), testClass);
    QCOMPARE(windowClassSpy.first().first().toByteArray(), testClass);

    // try setting it to same should not trigger the signal
    surface->setWindowClass(testClass);
    QVERIFY(!windowClassSpy.wait(100));
}

void TestWaylandShell::testDestroy()
{
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);
    QVERIFY(surface->isValid());

    connect(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged, m_shell, &Wrapland::Client::Shell::release);
    connect(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged, m_pointer, &Wrapland::Client::Pointer::release);
    connect(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged, m_seat, &Wrapland::Client::Seat::release);
    connect(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged, m_compositor, &Wrapland::Client::Compositor::release);
    connect(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged, s.get(), &Wrapland::Client::Surface::release);
    connect(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged, m_queue, &Wrapland::Client::EventQueue::release);

    delete m_display;
    m_display = nullptr;
    m_serverCompositor = nullptr;
    m_shellInterface = nullptr;
    m_serverSeat = nullptr;
    QTRY_VERIFY(!m_connection->established());

    QTRY_VERIFY(!m_shell->isValid());
    QTRY_VERIFY(!surface->isValid());

    m_shell->release();
    surface->release();
}

void TestWaylandShell::testCast()
{
    Wrapland::Client::Registry registry;
    QSignalSpy shellSpy(&registry, SIGNAL(shellAnnounced(quint32,quint32)));
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(shellSpy.wait());

    Wrapland::Client::Shell s;
    auto wlShell = registry.bindShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>());
    m_queue->addProxy(wlShell);
    QVERIFY(wlShell);
    QVERIFY(!s.isValid());
    s.setup(wlShell);
    QVERIFY(s.isValid());
    QCOMPARE((wl_shell*)s, wlShell);

    const Wrapland::Client::Shell &s2(s);
    QCOMPARE((wl_shell*)s2, wlShell);
}

void TestWaylandShell::testMove()
{
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);

    QSignalSpy serverSurfaceSpy(m_shellInterface, &Wrapland::Server::ShellInterface::surfaceCreated);
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QSignalSpy moveRequestedSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::moveRequested);
    QVERIFY(moveRequestedSpy.isValid());

    QSignalSpy pointerButtonChangedSpy(m_pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(pointerButtonChangedSpy.isValid());

    m_serverSeat->setFocusedPointerSurface(serverSurface->surface());
    m_serverSeat->pointerButtonPressed(Qt::LeftButton);
    QVERIFY(pointerButtonChangedSpy.wait());

    surface->requestMove(m_seat, pointerButtonChangedSpy.first().first().value<quint32>());
    QVERIFY(moveRequestedSpy.wait());
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.first().at(0).value<Wrapland::Server::Seat*>(), m_serverSeat);
    QCOMPARE(moveRequestedSpy.first().at(1).value<quint32>(), m_serverSeat->pointerButtonSerial(Qt::LeftButton));
}

void TestWaylandShell::testResize_data()
{
    QTest::addColumn<Qt::Edges>("resizeEdge");
    QTest::addColumn<Qt::Edges>("expectedEdge");

    QTest::newRow("None") << Qt::Edges() << Qt::Edges();

    QTest::newRow("Top")    << Qt::Edges(Qt::TopEdge)    << Qt::Edges(Qt::TopEdge);
    QTest::newRow("Bottom") << Qt::Edges(Qt::BottomEdge) << Qt::Edges(Qt::BottomEdge);
    QTest::newRow("Left")   << Qt::Edges(Qt::LeftEdge)   << Qt::Edges(Qt::LeftEdge);
    QTest::newRow("Right")  << Qt::Edges(Qt::RightEdge)  << Qt::Edges(Qt::RightEdge);

    QTest::newRow("Top Left")     << Qt::Edges(Qt::TopEdge | Qt::LeftEdge)     << Qt::Edges(Qt::TopEdge | Qt::LeftEdge);
    QTest::newRow("Top Right")    << Qt::Edges(Qt::TopEdge | Qt::RightEdge)    << Qt::Edges(Qt::TopEdge | Qt::RightEdge);
    QTest::newRow("Bottom Left")  << Qt::Edges(Qt::BottomEdge | Qt::LeftEdge)  << Qt::Edges(Qt::BottomEdge | Qt::LeftEdge);
    QTest::newRow("Bottom Right") << Qt::Edges(Qt::BottomEdge | Qt::RightEdge) << Qt::Edges(Qt::BottomEdge | Qt::RightEdge);

    // invalid combinations
    QTest::newRow("Top Bottom") << Qt::Edges(Qt::TopEdge | Qt::BottomEdge) << Qt::Edges();
    QTest::newRow("Left Right") << Qt::Edges(Qt::RightEdge | Qt::LeftEdge) << Qt::Edges();
    QTest::newRow("Top Bottom Right")  << Qt::Edges(Qt::TopEdge | Qt::BottomEdge | Qt::RightEdge)  << Qt::Edges();
    QTest::newRow("Top Bottom Left")   << Qt::Edges(Qt::TopEdge | Qt::BottomEdge | Qt::LeftEdge)   << Qt::Edges();
    QTest::newRow("Left Right Top")    << Qt::Edges(Qt::RightEdge | Qt::LeftEdge | Qt::TopEdge)    << Qt::Edges();
    QTest::newRow("Left Right Bottom") << Qt::Edges(Qt::RightEdge | Qt::LeftEdge | Qt::BottomEdge) << Qt::Edges();
    QTest::newRow("All") << Qt::Edges(Qt::RightEdge | Qt::LeftEdge | Qt::BottomEdge | Qt::TopEdge) << Qt::Edges();
}

void TestWaylandShell::testResize()
{
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    auto surface = m_shell->createSurface(s.get(), m_shell);

    QSignalSpy serverSurfaceSpy(m_shellInterface, &Wrapland::Server::ShellInterface::surfaceCreated);
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);
    QSignalSpy resizeRequestedSpy(serverSurface, &Wrapland::Server::ShellSurfaceInterface::resizeRequested);
    QVERIFY(resizeRequestedSpy.isValid());

    QSignalSpy pointerButtonChangedSpy(m_pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(pointerButtonChangedSpy.isValid());

    m_serverSeat->setFocusedPointerSurface(serverSurface->surface());
    m_serverSeat->pointerButtonPressed(Qt::LeftButton);
    QVERIFY(pointerButtonChangedSpy.wait());

    QFETCH(Qt::Edges, resizeEdge);
    surface->requestResize(m_seat, pointerButtonChangedSpy.first().first().value<quint32>(), resizeEdge);
    QVERIFY(resizeRequestedSpy.wait());
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.first().at(0).value<Wrapland::Server::Seat*>(), m_serverSeat);
    QCOMPARE(resizeRequestedSpy.first().at(1).value<quint32>(), m_serverSeat->pointerButtonSerial(Qt::LeftButton));
    QTEST(resizeRequestedSpy.first().at(2).value<Qt::Edges>(), "expectedEdge");
}

void TestWaylandShell::testDisconnect()
{
    // this test verifies that the server side correctly tears down the resources when the client disconnects

    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    std::unique_ptr<Wrapland::Client::ShellSurface> surface(m_shell->createSurface(s.get(), m_shell));
    QSignalSpy serverSurfaceSpy(m_shellInterface, &Wrapland::Server::ShellInterface::surfaceCreated);
    QVERIFY(serverSurfaceSpy.isValid());
    QVERIFY(serverSurfaceSpy.wait());
    Wrapland::Server::ShellSurfaceInterface *serverSurface = serverSurfaceSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverSurface);

    // destroy client
    QSignalSpy clientDisconnectedSpy(serverSurface->client(), &Wrapland::Server::ClientConnection::disconnected);
    QVERIFY(clientDisconnectedSpy.isValid());
    QSignalSpy shellSurfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(shellSurfaceDestroyedSpy.isValid());

    s->release();
    surface->release();
    m_shell->release();
    m_compositor->release();
    m_pointer->release();
    m_seat->release();
    m_queue->release();

    QVERIFY(m_connection);
    m_connection->deleteLater();
    m_connection = nullptr;

    QVERIFY(clientDisconnectedSpy.wait());
    QCOMPARE(clientDisconnectedSpy.count(), 1);
    QCOMPARE(shellSurfaceDestroyedSpy.count(), 0);
    // it's already unbound, but the shell surface is not yet destroyed
    QVERIFY(!serverSurface->resource());
    // ensure we don't crash when accessing it in this state
    serverSurface->ping();
    serverSurface->requestSize(QSize(1, 2));
    QVERIFY(shellSurfaceDestroyedSpy.wait());
    QCOMPARE(shellSurfaceDestroyedSpy.count(), 1);
}

void TestWaylandShell::testWhileDestroying()
{
    // this test tries to hit a condition that a Surface gets created with an ID which was already
    // used for a previous Surface. For each Surface we try to create a ShellSurface.
    // Even if there was a Surface in the past with the same ID, it should create the ShellSurface
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // create ShellSurface
    QSignalSpy shellSurfaceCreatedSpy(m_shellInterface, &Wrapland::Server::ShellInterface::surfaceCreated);
    QVERIFY(shellSurfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::ShellSurface> ps(m_shell->createSurface(s.get()));
    QVERIFY(shellSurfaceCreatedSpy.wait());

    // now try to create more surfaces
    QSignalSpy clientErrorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(clientErrorSpy.isValid());
    for (int i = 0; i < 100; i++) {
        s.reset();
        ps.reset();
        s.reset(m_compositor->createSurface());
        ps.reset(m_shell->createSurface(s.get()));
        QVERIFY(surfaceCreatedSpy.wait());
    }
    QVERIFY(clientErrorSpy.isEmpty());
    QVERIFY(!clientErrorSpy.wait(100));
    QVERIFY(clientErrorSpy.isEmpty());
}

void TestWaylandShell::testClientDisconnecting()
{
    // this test tries to request a new surface size while the client is actually already destroyed
    // see BUG: 370232

    // create ShellSurface
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QSignalSpy shellSurfaceCreatedSpy(m_shellInterface, &Wrapland::Server::ShellInterface::surfaceCreated);
    QVERIFY(shellSurfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::ShellSurface> ps(m_shell->createSurface(s.get()));
    QVERIFY(shellSurfaceCreatedSpy.wait());

    auto serverShellSurface = shellSurfaceCreatedSpy.first().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverShellSurface);

    QSignalSpy shellSurfaceUnboundSpy(serverShellSurface, &Wrapland::Server::Resource::unbound);
    QVERIFY(shellSurfaceUnboundSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s2(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::ShellSurface> ps2(m_shell->createSurface(s2.get()));
    QVERIFY(shellSurfaceCreatedSpy.wait());
    auto serverShellSurface2 = shellSurfaceCreatedSpy.last().first().value<Wrapland::Server::ShellSurfaceInterface*>();
    QVERIFY(serverShellSurface2);

    connect(serverShellSurface, &Wrapland::Server::Resource::unbound, this,
        [serverShellSurface, serverShellSurface2] {
            serverShellSurface2->requestSize(QSize(100, 200));
        }
    );

    ps->release();
    s->release();
    ps2->release();
    s2->release();
    m_pointer->release();
    m_seat->release();
    m_shell->release();
    m_compositor->release();
    m_queue->release();

    m_connection->deleteLater();
    m_connection = nullptr;
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;

    QVERIFY(shellSurfaceUnboundSpy.wait());
}

QTEST_GUILESS_MAIN(TestWaylandShell)
#include "test_wayland_shell.moc"
