/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2017  David Edmundson <davidedmundson@kde.org>

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

#include "test_xdg_shell.h"

XdgShellTest::XdgShellTest(Wrapland::Server::XdgShellInterfaceVersion version):
    m_version(version)
{}

static const QString s_socketName = QStringLiteral("wrapland-test-xdg_shell-0");

void XdgShellTest::init()
{
    Q_ASSERT(!m_display);
    m_display = new Wrapland::Server::D_isplay(this);
    m_display->setSocketName(s_socketName);

    m_display->start();

    m_display->createShm();

    m_o1Interface = m_display->createOutput(m_display);
    m_o1Interface->addMode(QSize(1024, 768));

    m_o2Interface = m_display->createOutput(m_display);
    m_o2Interface->addMode(QSize(1024, 768));

    m_serverSeat = m_display->createSeat(m_display);
    m_serverSeat->setHasKeyboard(true);
    m_serverSeat->setHasPointer(true);
    m_serverSeat->setHasTouch(true);

    m_serverCompositor = m_display->createCompositor(m_display);

    m_xdgShellInterface = m_display->createXdgShell(m_version, m_display);
    QCOMPARE(m_xdgShellInterface->interfaceVersion(), m_version);
    m_xdgShellInterface->create();

    // Setup connection.
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
    QSignalSpy interfaceAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfaceAnnounced);
    QVERIFY(interfaceAnnouncedSpy.isValid());
    QSignalSpy outputAnnouncedSpy(&registry, &Wrapland::Client::Registry::outputAnnounced);
    QVERIFY(outputAnnouncedSpy.isValid());

    auto shellAnnouncedSignal =  m_version == Wrapland::Server::XdgShellInterfaceVersion::UnstableV5 ? &Wrapland::Client::Registry::xdgShellUnstableV5Announced :
                                 m_version == Wrapland::Server::XdgShellInterfaceVersion::UnstableV6 ? &Wrapland::Client::Registry::xdgShellUnstableV6Announced :
                                 &Wrapland::Client::Registry::xdgShellStableAnnounced;

    QSignalSpy xdgShellAnnouncedSpy(&registry, shellAnnouncedSignal);
    QVERIFY(xdgShellAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    QCOMPARE(outputAnnouncedSpy.count(), 2);
    m_output1 = registry.createOutput(outputAnnouncedSpy.first().at(0).value<quint32>(), outputAnnouncedSpy.first().at(1).value<quint32>(), this);
    m_output2 = registry.createOutput(outputAnnouncedSpy.last().at(0).value<quint32>(), outputAnnouncedSpy.last().at(1).value<quint32>(), this);

    m_shmPool = registry.createShmPool(registry.interface(Wrapland::Client::Registry::Interface::Shm).name, registry.interface(Wrapland::Client::Registry::Interface::Shm).version, this);
    QVERIFY(m_shmPool);
    QVERIFY(m_shmPool->isValid());

    m_compositor = registry.createCompositor(registry.interface(Wrapland::Client::Registry::Interface::Compositor).name, registry.interface(Wrapland::Client::Registry::Interface::Compositor).version, this);
    QVERIFY(m_compositor);
    QVERIFY(m_compositor->isValid());

    m_seat = registry.createSeat(registry.interface(Wrapland::Client::Registry::Interface::Seat).name, registry.interface(Wrapland::Client::Registry::Interface::Seat).version, this);
    QVERIFY(m_seat);
    QVERIFY(m_seat->isValid());

    QCOMPARE(xdgShellAnnouncedSpy.count(), 1);

    Wrapland::Client::Registry::Interface iface;
    switch (m_version) {
    case Wrapland::Server::XdgShellInterfaceVersion::UnstableV5:
        iface = Wrapland::Client::Registry::Interface::XdgShellUnstableV5;
        break;
    case Wrapland::Server::XdgShellInterfaceVersion::UnstableV6:
        iface = Wrapland::Client::Registry::Interface::XdgShellUnstableV6;
        break;
    case Wrapland::Server::XdgShellInterfaceVersion::Stable:
        iface = Wrapland::Client::Registry::Interface::XdgShellStable;
        break;
    }

    m_xdgShell = registry.createXdgShell(registry.interface(iface).name,
                                         registry.interface(iface).version,
                                         this);
    QVERIFY(m_xdgShell);
    QVERIFY(m_xdgShell->isValid());
}

void XdgShellTest::cleanup()
{
#define CLEANUP(variable) \
        delete variable; \
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

    CLEANUP(m_serverCompositor)
    CLEANUP(m_xdgShellInterface)
    CLEANUP(m_o1Interface);
    CLEANUP(m_o2Interface);
    CLEANUP(m_serverSeat);
    CLEANUP(m_display)
#undef CLEANUP
}

void XdgShellTest::testCreateSurface()
{
    // this test verifies that we can create a surface
    // first created the signal spies for the server
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &Wrapland::Server::XdgShellInterface::surfaceCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());

    // create surface
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface != nullptr);
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // create shell surface
    std::unique_ptr<Wrapland::Client::XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.get()));
    QVERIFY(xdgSurface != nullptr);
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    // verify base things
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<Wrapland::Server::XdgShellSurfaceInterface*>();
    QVERIFY(serverXdgSurface);
    QCOMPARE(serverXdgSurface->isConfigurePending(), false);
    QCOMPARE(serverXdgSurface->title(), QString());
    QCOMPARE(serverXdgSurface->windowClass(), QByteArray());
    QCOMPARE(serverXdgSurface->isTransient(), false);
    QCOMPARE(serverXdgSurface->transientFor(), QPointer<Wrapland::Server::XdgShellSurfaceInterface>());
    QCOMPARE(serverXdgSurface->surface(), serverSurface);

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
    QCOMPARE(serverXdgSurface->title(), QString());

    // lets' change the title
    QSignalSpy titleChangedSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::titleChanged);
    QVERIFY(titleChangedSpy.isValid());
    xdgSurface->setTitle(QStringLiteral("foo"));
    QVERIFY(titleChangedSpy.wait());
    QCOMPARE(titleChangedSpy.count(), 1);
    QCOMPARE(titleChangedSpy.first().first().toString(), QStringLiteral("foo"));
    QCOMPARE(serverXdgSurface->title(), QStringLiteral("foo"));
}

void XdgShellTest::testWindowClass()
{
    // this test verifies that we can change the window class/app id of a shell surface
    // first create surface
    SURFACE

    // should not have a window class yet
    QCOMPARE(serverXdgSurface->windowClass(), QByteArray());

    // let's change the window class
    QSignalSpy windowClassChanged(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::windowClassChanged);
    QVERIFY(windowClassChanged.isValid());
    xdgSurface->setAppId(QByteArrayLiteral("org.kde.xdgsurfacetest"));
    QVERIFY(windowClassChanged.wait());
    QCOMPARE(windowClassChanged.count(), 1);
    QCOMPARE(windowClassChanged.first().first().toByteArray(), QByteArrayLiteral("org.kde.xdgsurfacetest"));
    QCOMPARE(serverXdgSurface->windowClass(), QByteArrayLiteral("org.kde.xdgsurfacetest"));
}

void XdgShellTest::testMaximize()
{
    // this test verifies that the maximize/unmaximize calls work
    SURFACE

    QSignalSpy maximizeRequestedSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::maximizedChanged);
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

    QSignalSpy minimizeRequestedSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::minimizeRequested);
    QVERIFY(minimizeRequestedSpy.isValid());

    xdgSurface->requestMinimize();
    QVERIFY(minimizeRequestedSpy.wait());
    QCOMPARE(minimizeRequestedSpy.count(), 1);
}

void XdgShellTest::testFullscreen()
{
    qRegisterMetaType<Wrapland::Server::Output*>();
    // this test verifies going to/from fullscreen
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &Wrapland::Server::XdgShellInterface::surfaceCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.get()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<Wrapland::Server::XdgShellSurfaceInterface*>();
    QVERIFY(serverXdgSurface);

    QSignalSpy fullscreenSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::fullscreenChanged);
    QVERIFY(fullscreenSpy.isValid());

    // without an output
    xdgSurface->setFullscreen(true, nullptr);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 1);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QVERIFY(!fullscreenSpy.last().at(1).value<Wrapland::Server::Output*>());

    // unset
    xdgSurface->setFullscreen(false);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 2);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), false);
    QVERIFY(!fullscreenSpy.last().at(1).value<Wrapland::Server::Output*>());

    // with outputs
    xdgSurface->setFullscreen(true, m_output1);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 3);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QCOMPARE(fullscreenSpy.last().at(1).value<Wrapland::Server::Output*>(), m_o1Interface);

    // now other output
    xdgSurface->setFullscreen(true, m_output2);
    QVERIFY(fullscreenSpy.wait());
    QCOMPARE(fullscreenSpy.count(), 4);
    QCOMPARE(fullscreenSpy.last().at(0).toBool(), true);
    QCOMPARE(fullscreenSpy.last().at(1).value<Wrapland::Server::Output*>(), m_o2Interface);
}

void XdgShellTest::testShowWindowMenu()
{
    qRegisterMetaType<Wrapland::Server::Seat*>();
    // this test verifies that the show window menu request works
    SURFACE

    QSignalSpy windowMenuSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::windowMenuRequested);
    QVERIFY(windowMenuSpy.isValid());

    // TODO: the serial needs to be a proper one
    xdgSurface->requestShowWindowMenu(m_seat, 20, QPoint(30, 40));
    QVERIFY(windowMenuSpy.wait());
    QCOMPARE(windowMenuSpy.count(), 1);
    QCOMPARE(windowMenuSpy.first().at(0).value<Wrapland::Server::Seat*>(), m_serverSeat);
    QCOMPARE(windowMenuSpy.first().at(1).value<quint32>(), 20u);
    QCOMPARE(windowMenuSpy.first().at(2).toPoint(), QPoint(30, 40));
}

void XdgShellTest::testMove()
{
    qRegisterMetaType<Wrapland::Server::Seat*>();
    // this test verifies that the move request works
    SURFACE

    QSignalSpy moveSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::moveRequested);
    QVERIFY(moveSpy.isValid());

    // TODO: the serial needs to be a proper one
    xdgSurface->requestMove(m_seat, 50);
    QVERIFY(moveSpy.wait());
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(moveSpy.first().at(0).value<Wrapland::Server::Seat*>(), m_serverSeat);
    QCOMPARE(moveSpy.first().at(1).value<quint32>(), 50u);
}

void XdgShellTest::testResize_data()
{
    QTest::addColumn<Qt::Edges>("edges");

    QTest::newRow("none")         << Qt::Edges();
    QTest::newRow("top")          << Qt::Edges(Qt::TopEdge);
    QTest::newRow("bottom")       << Qt::Edges(Qt::BottomEdge);
    QTest::newRow("left")         << Qt::Edges(Qt::LeftEdge);
    QTest::newRow("top left")     << Qt::Edges(Qt::TopEdge | Qt::LeftEdge);
    QTest::newRow("bottom left")  << Qt::Edges(Qt::BottomEdge | Qt::LeftEdge);
    QTest::newRow("right")        << Qt::Edges(Qt::RightEdge);
    QTest::newRow("top right")    << Qt::Edges(Qt::TopEdge | Qt::RightEdge);
    QTest::newRow("bottom right") << Qt::Edges(Qt::BottomEdge | Qt::RightEdge);
}

void XdgShellTest::testResize()
{
    qRegisterMetaType<Wrapland::Server::Seat*>();
    // this test verifies that the resize request works
    SURFACE

    QSignalSpy resizeSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::resizeRequested);
    QVERIFY(resizeSpy.isValid());

    // TODO: the serial needs to be a proper one
    QFETCH(Qt::Edges, edges);
    xdgSurface->requestResize(m_seat, 60, edges);
    QVERIFY(resizeSpy.wait());
    QCOMPARE(resizeSpy.count(), 1);
    QCOMPARE(resizeSpy.first().at(0).value<Wrapland::Server::Seat*>(), m_serverSeat);
    QCOMPARE(resizeSpy.first().at(1).value<quint32>(), 60u);
    QCOMPARE(resizeSpy.first().at(2).value<Qt::Edges>(), edges);
}

void XdgShellTest::testTransient()
{
    // this test verifies that setting the transient for works
    SURFACE
    std::unique_ptr<Wrapland::Client::Surface> surface2(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::XdgShellSurface> xdgSurface2(m_xdgShell->createSurface(surface2.get()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface2 = xdgSurfaceCreatedSpy.last().first().value<Wrapland::Server::XdgShellSurfaceInterface*>();
    QVERIFY(serverXdgSurface2);

    QVERIFY(!serverXdgSurface->isTransient());
    QVERIFY(!serverXdgSurface2->isTransient());

    // now make xdsgSurface2 a transient for xdgSurface
    QSignalSpy transientForSpy(serverXdgSurface2, &Wrapland::Server::XdgShellSurfaceInterface::transientForChanged);
    QVERIFY(transientForSpy.isValid());
    xdgSurface2->setTransientFor(xdgSurface.get());

    QVERIFY(transientForSpy.wait());
    QCOMPARE(transientForSpy.count(), 1);
    QVERIFY(serverXdgSurface2->isTransient());
    QCOMPARE(serverXdgSurface2->transientFor().data(), serverXdgSurface);
    QVERIFY(!serverXdgSurface->isTransient());

    // unset the transient for
    xdgSurface2->setTransientFor(nullptr);
    QVERIFY(transientForSpy.wait());
    QCOMPARE(transientForSpy.count(), 2);
    QVERIFY(!serverXdgSurface2->isTransient());
    QVERIFY(serverXdgSurface2->transientFor().isNull());
    QVERIFY(!serverXdgSurface->isTransient());
}

void XdgShellTest::testPing()
{
    // this test verifies that a ping request is sent to the client
    SURFACE

    QSignalSpy pingSpy(m_xdgShellInterface, &Wrapland::Server::XdgShellInterface::pongReceived);
    QVERIFY(pingSpy.isValid());

    quint32 serial = m_xdgShellInterface->ping(serverXdgSurface);
    QVERIFY(pingSpy.wait());
    QCOMPARE(pingSpy.count(), 1);
    QCOMPARE(pingSpy.takeFirst().at(0).value<quint32>(), serial);

    // test of a ping failure
    // disconnecting the connection thread to the queue will break the connection and pings will do a timeout
    disconnect(m_connection, &Wrapland::Client::ConnectionThread::eventsRead, m_queue, &Wrapland::Client::EventQueue::dispatch);
    m_xdgShellInterface->ping(serverXdgSurface);
    QSignalSpy pingDelayedSpy(m_xdgShellInterface, &Wrapland::Server::XdgShellInterface::pingDelayed);
    QVERIFY(pingDelayedSpy.wait());

    QSignalSpy pingTimeoutSpy(m_xdgShellInterface, &Wrapland::Server::XdgShellInterface::pingTimeout);
    QVERIFY(pingTimeoutSpy.wait());
}

void XdgShellTest::testClose()
{
    // this test verifies that a close request is sent to the client
    SURFACE

    QSignalSpy closeSpy(xdgSurface.get(), &Wrapland::Client::XdgShellSurface::closeRequested);
    QVERIFY(closeSpy.isValid());

    serverXdgSurface->close();
    QVERIFY(closeSpy.wait());
    QCOMPARE(closeSpy.count(), 1);

    QSignalSpy destroyedSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::destroyed);
    QVERIFY(destroyedSpy.isValid());
    xdgSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

void XdgShellTest::testConfigureStates_data()
{
    QTest::addColumn<Wrapland::Server::XdgShellSurfaceInterface::States>("serverStates");
    QTest::addColumn<Wrapland::Client::XdgShellSurface::States>("clientStates");

    const auto sa = Wrapland::Server::XdgShellSurfaceInterface::States(Wrapland::Server::XdgShellSurfaceInterface::State::Activated);
    const auto sm = Wrapland::Server::XdgShellSurfaceInterface::States(Wrapland::Server::XdgShellSurfaceInterface::State::Maximized);
    const auto sf = Wrapland::Server::XdgShellSurfaceInterface::States(Wrapland::Server::XdgShellSurfaceInterface::State::Fullscreen);
    const auto sr = Wrapland::Server::XdgShellSurfaceInterface::States(Wrapland::Server::XdgShellSurfaceInterface::State::Resizing);

    const auto ca = Wrapland::Client::XdgShellSurface::States(Wrapland::Client::XdgShellSurface::State::Activated);
    const auto cm = Wrapland::Client::XdgShellSurface::States(Wrapland::Client::XdgShellSurface::State::Maximized);
    const auto cf = Wrapland::Client::XdgShellSurface::States(Wrapland::Client::XdgShellSurface::State::Fullscreen);
    const auto cr = Wrapland::Client::XdgShellSurface::States(Wrapland::Client::XdgShellSurface::State::Resizing);

    QTest::newRow("none")       << Wrapland::Server::XdgShellSurfaceInterface::States()   << Wrapland::Client::XdgShellSurface::States();
    QTest::newRow("Active")     << sa << ca;
    QTest::newRow("Maximize")   << sm << cm;
    QTest::newRow("Fullscreen") << sf << cf;
    QTest::newRow("Resizing")   << sr << cr;

    QTest::newRow("Active/Maximize")       << (sa | sm) << (ca | cm);
    QTest::newRow("Active/Fullscreen")     << (sa | sf) << (ca | cf);
    QTest::newRow("Active/Resizing")       << (sa | sr) << (ca | cr);
    QTest::newRow("Maximize/Fullscreen")   << (sm | sf) << (cm | cf);
    QTest::newRow("Maximize/Resizing")     << (sm | sr) << (cm | cr);
    QTest::newRow("Fullscreen/Resizing")   << (sf | sr) << (cf | cr);

    QTest::newRow("Active/Maximize/Fullscreen")   << (sa | sm | sf) << (ca | cm | cf);
    QTest::newRow("Active/Maximize/Resizing")     << (sa | sm | sr) << (ca | cm | cr);
    QTest::newRow("Maximize/Fullscreen|Resizing") << (sm | sf | sr) << (cm | cf | cr);

    QTest::newRow("Active/Maximize/Fullscreen/Resizing")   << (sa | sm | sf | sr) << (ca | cm | cf | cr);
}

void XdgShellTest::testConfigureStates()
{
    qRegisterMetaType<Wrapland::Client::XdgShellSurface::States>();
    // this test verifies that configure states works
    SURFACE

    QSignalSpy configureSpy(xdgSurface.get(), &Wrapland::Client::XdgShellSurface::configureRequested);
    QVERIFY(configureSpy.isValid());

    QFETCH(Wrapland::Server::XdgShellSurfaceInterface::States, serverStates);
    serverXdgSurface->configure(serverStates);
    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 1);
    QCOMPARE(configureSpy.first().at(0).toSize(), QSize(0, 0));
    QTEST(configureSpy.first().at(1).value<Wrapland::Client::XdgShellSurface::States>(), "clientStates");
    QCOMPARE(configureSpy.first().at(2).value<quint32>(), m_display->serial());

    QSignalSpy ackSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::configureAcknowledged);
    QVERIFY(ackSpy.isValid());

    xdgSurface->ackConfigure(configureSpy.first().at(2).value<quint32>());
    QVERIFY(ackSpy.wait());
    QCOMPARE(ackSpy.count(), 1);
    QCOMPARE(ackSpy.first().first().value<quint32>(), configureSpy.first().at(2).value<quint32>());
}

void XdgShellTest::testConfigureMultipleAcks()
{
    qRegisterMetaType<Wrapland::Client::XdgShellSurface::States>();
    // this test verifies that with multiple configure requests the last acknowledged one acknowledges all
    SURFACE

    QSignalSpy configureSpy(xdgSurface.get(), &Wrapland::Client::XdgShellSurface::configureRequested);
    QVERIFY(configureSpy.isValid());
    QSignalSpy sizeChangedSpy(xdgSurface.get(), &Wrapland::Client::XdgShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QSignalSpy ackSpy(serverXdgSurface, &Wrapland::Server::XdgShellSurfaceInterface::configureAcknowledged);
    QVERIFY(ackSpy.isValid());

    serverXdgSurface->configure(Wrapland::Server::XdgShellSurfaceInterface::States(), QSize(10, 20));
    const quint32 serial1 = m_display->serial();
    serverXdgSurface->configure(Wrapland::Server::XdgShellSurfaceInterface::States(), QSize(20, 30));
    const quint32 serial2 = m_display->serial();
    QVERIFY(serial1 != serial2);
    serverXdgSurface->configure(Wrapland::Server::XdgShellSurfaceInterface::States(), QSize(30, 40));
    const quint32 serial3 = m_display->serial();
    QVERIFY(serial1 != serial3);
    QVERIFY(serial2 != serial3);

    QVERIFY(configureSpy.wait());
    QTRY_COMPARE(configureSpy.count(), 3);
    QVERIFY(!configureSpy.wait(100));

    QCOMPARE(configureSpy.at(0).at(0).toSize(), QSize(10, 20));
    QCOMPARE(configureSpy.at(0).at(1).value<Wrapland::Client::XdgShellSurface::States>(), Wrapland::Client::XdgShellSurface::States());
    QCOMPARE(configureSpy.at(0).at(2).value<quint32>(), serial1);
    QCOMPARE(configureSpy.at(1).at(0).toSize(), QSize(20, 30));
    QCOMPARE(configureSpy.at(1).at(1).value<Wrapland::Client::XdgShellSurface::States>(), Wrapland::Client::XdgShellSurface::States());
    QCOMPARE(configureSpy.at(1).at(2).value<quint32>(), serial2);
    QCOMPARE(configureSpy.at(2).at(0).toSize(), QSize(30, 40));
    QCOMPARE(configureSpy.at(2).at(1).value<Wrapland::Client::XdgShellSurface::States>(), Wrapland::Client::XdgShellSurface::States());
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
    serverXdgSurface->configure(Wrapland::Server::XdgShellSurfaceInterface::States());
    // should not change size
    QVERIFY(configureSpy.wait());
    QCOMPARE(configureSpy.count(), 4);
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(xdgSurface->size(), QSize(30, 40));
}
