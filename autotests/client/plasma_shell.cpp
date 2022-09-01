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
#include "../../src/client/plasmashell.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"

#include "../../server/client.h"
#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/plasma_shell_p.h"
#include "../../server/surface.h"

class TestPlasmaShell : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testRole_data();
    void testRole();
    void testPosition();
    void testSkipTaskbar();
    void testSkipSwitcher();
    void testPanelBehavior_data();
    void testPanelBehavior();
    void testAutoHidePanel();
    void testPanelTakesFocus();
    void test_open_under_cursor();
    void testDisconnect();
    void testWhileDestroying();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    QThread* m_thread = nullptr;
    Wrapland::Client::Registry* m_registry = nullptr;
    Wrapland::Client::PlasmaShell* m_plasmaShell = nullptr;
};

constexpr auto socket_name{"wrapland-test-wayland-plasma-shell-0"};

void TestPlasmaShell::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.compositor = server.display->createCompositor();
    server.globals.plasma_shell = server.display->createPlasmaShell();

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
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Wrapland::Client::Registry();
    QSignalSpy interfacesAnnouncedSpy(m_registry, &Wrapland::Client::Registry::interfaceAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    QVERIFY(interfacesAnnouncedSpy.wait());
#define CREATE(variable, factory, iface)                                                           \
    variable = m_registry->create##factory(                                                        \
        m_registry->interface(Wrapland::Client::Registry::Interface::iface).name,                  \
        m_registry->interface(Wrapland::Client::Registry::Interface::iface).version,               \
        this);                                                                                     \
    QVERIFY(variable);

    CREATE(m_compositor, Compositor, Compositor)
    CREATE(m_plasmaShell, PlasmaShell, PlasmaShell)

#undef CREATE
}

void TestPlasmaShell::cleanup()
{
#define DELETE(name)                                                                               \
    if (name) {                                                                                    \
        delete name;                                                                               \
        name = nullptr;                                                                            \
    }
    DELETE(m_plasmaShell)
    DELETE(m_compositor)
    DELETE(m_queue)
    DELETE(m_registry)
#undef DELETE
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

void TestPlasmaShell::testRole_data()
{
    QTest::addColumn<Wrapland::Client::PlasmaShellSurface::Role>("clientRole");
    QTest::addColumn<Wrapland::Server::PlasmaShellSurface::Role>("serverRole");

    QTest::newRow("desktop") << Wrapland::Client::PlasmaShellSurface::Role::Desktop
                             << Wrapland::Server::PlasmaShellSurface::Role::Desktop;
    QTest::newRow("osd") << Wrapland::Client::PlasmaShellSurface::Role::OnScreenDisplay
                         << Wrapland::Server::PlasmaShellSurface::Role::OnScreenDisplay;
    QTest::newRow("panel") << Wrapland::Client::PlasmaShellSurface::Role::Panel
                           << Wrapland::Server::PlasmaShellSurface::Role::Panel;
    QTest::newRow("notification") << Wrapland::Client::PlasmaShellSurface::Role::Notification
                                  << Wrapland::Server::PlasmaShellSurface::Role::Notification;
    QTest::newRow("tooltip") << Wrapland::Client::PlasmaShellSurface::Role::ToolTip
                             << Wrapland::Server::PlasmaShellSurface::Role::ToolTip;
    QTest::newRow("criticalnotification")
        << Wrapland::Client::PlasmaShellSurface::Role::CriticalNotification
        << Wrapland::Server::PlasmaShellSurface::Role::CriticalNotification;
    QTest::newRow("appletpopup") << Wrapland::Client::PlasmaShellSurface::Role::AppletPopup
                                 << Wrapland::Server::PlasmaShellSurface::Role::AppletPopup;
}

void TestPlasmaShell::testRole()
{
    // this test verifies that setting the role on a plasma shell surface works

    // first create signal spies
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());

    // create the surface
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    // no PlasmaShellSurface for the Surface yet yet
    QVERIFY(!Wrapland::Client::PlasmaShellSurface::get(s.get()));
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QCOMPARE(ps->role(), Wrapland::Client::PlasmaShellSurface::Role::Normal);
    // now we should have a PlasmaShellSurface for
    QCOMPARE(Wrapland::Client::PlasmaShellSurface::get(s.get()), ps.get());

    // try to create another PlasmaShellSurface for the same Surface, should return from cache
    QCOMPARE(m_plasmaShell->createSurface(s.get()), ps.get());

    // and get them on the server
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    QCOMPARE(surfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QCOMPARE(sps->surface(), surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>());
    QCOMPARE(sps->shell(), server.globals.plasma_shell.get());
    QCOMPARE(Wrapland::Server::PlasmaShellSurface::get(sps->resource()), sps);
    QVERIFY(!Wrapland::Server::PlasmaShellSurface::get(nullptr));

    // default role should be normal
    QCOMPARE(sps->role(), Wrapland::Server::PlasmaShellSurface::Role::Normal);

    // now change it
    QSignalSpy roleChangedSpy(sps, &Wrapland::Server::PlasmaShellSurface::roleChanged);
    QVERIFY(roleChangedSpy.isValid());
    QFETCH(Wrapland::Client::PlasmaShellSurface::Role, clientRole);
    ps->setRole(clientRole);
    QCOMPARE(ps->role(), clientRole);
    QVERIFY(roleChangedSpy.wait());
    QCOMPARE(roleChangedSpy.count(), 1);
    QTEST(sps->role(), "serverRole");

    // try changing again should not emit the signal
    ps->setRole(clientRole);
    QVERIFY(!roleChangedSpy.wait(100));

    // set role back to normal
    ps->setRole(Wrapland::Client::PlasmaShellSurface::Role::Normal);
    QCOMPARE(ps->role(), Wrapland::Client::PlasmaShellSurface::Role::Normal);
    QVERIFY(roleChangedSpy.wait());
    QCOMPARE(roleChangedSpy.count(), 2);
    QCOMPARE(sps->role(), Wrapland::Server::PlasmaShellSurface::Role::Normal);
}

void TestPlasmaShell::testPosition()
{
    // this test verifies that updating the position of a PlasmaShellSurface is properly passed to
    // the server
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QVERIFY(sps);
    QVERIFY(sps->surface());

    // default position should not be set
    QVERIFY(!sps->isPositionSet());
    QCOMPARE(sps->position(), QPoint());

    // now let's try to change the position
    QSignalSpy positionChangedSpy(sps, &Wrapland::Server::PlasmaShellSurface::positionChanged);
    QVERIFY(positionChangedSpy.isValid());
    ps->setPosition(QPoint(1, 2));
    QVERIFY(positionChangedSpy.wait());
    QCOMPARE(positionChangedSpy.count(), 1);
    QVERIFY(sps->isPositionSet());
    QCOMPARE(sps->position(), QPoint(1, 2));

    // let's try to set same position, should not trigger an update
    ps->setPosition(QPoint(1, 2));
    QVERIFY(!positionChangedSpy.wait(100));
    // different point should work, though
    ps->setPosition(QPoint(3, 4));
    QVERIFY(positionChangedSpy.wait());
    QCOMPARE(positionChangedSpy.count(), 2);
    QCOMPARE(sps->position(), QPoint(3, 4));
}

void TestPlasmaShell::testSkipTaskbar()
{
    // this test verifies that sip taskbar is properly passed to server
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QVERIFY(!sps->skipTaskbar());

    // now change
    QSignalSpy skipTaskbarChangedSpy(sps,
                                     &Wrapland::Server::PlasmaShellSurface::skipTaskbarChanged);
    QVERIFY(skipTaskbarChangedSpy.isValid());
    ps->setSkipTaskbar(true);
    QVERIFY(skipTaskbarChangedSpy.wait());
    QVERIFY(sps->skipTaskbar());
    // setting to same again should not emit the signal
    ps->setSkipTaskbar(true);
    QEXPECT_FAIL("", "Should not be emitted if not changed", Continue);
    QVERIFY(!skipTaskbarChangedSpy.wait(100));
    QVERIFY(sps->skipTaskbar());

    // setting to false should change again
    ps->setSkipTaskbar(false);
    QVERIFY(skipTaskbarChangedSpy.wait());
    QVERIFY(!sps->skipTaskbar());
}

void TestPlasmaShell::testSkipSwitcher()
{
    // this test verifies that Skip Switcher is properly passed to server
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QVERIFY(!sps->skipSwitcher());

    // now change
    QSignalSpy skipSwitcherChangedSpy(sps,
                                      &Wrapland::Server::PlasmaShellSurface::skipSwitcherChanged);
    QVERIFY(skipSwitcherChangedSpy.isValid());
    ps->setSkipSwitcher(true);
    QVERIFY(skipSwitcherChangedSpy.wait());
    QVERIFY(sps->skipSwitcher());
    // setting to same again should not emit the signal
    ps->setSkipSwitcher(true);
    QEXPECT_FAIL("", "Should not be emitted if not changed", Continue);
    QVERIFY(!skipSwitcherChangedSpy.wait(100));
    QVERIFY(sps->skipSwitcher());

    // setting to false should change again
    ps->setSkipSwitcher(false);
    QVERIFY(skipSwitcherChangedSpy.wait());
    QVERIFY(!sps->skipSwitcher());
}

void TestPlasmaShell::testPanelBehavior_data()
{
    QTest::addColumn<Wrapland::Client::PlasmaShellSurface::PanelBehavior>("client");
    QTest::addColumn<Wrapland::Server::PlasmaShellSurface::PanelBehavior>("server");

    QTest::newRow("autohide") << Wrapland::Client::PlasmaShellSurface::PanelBehavior::AutoHide
                              << Wrapland::Server::PlasmaShellSurface::PanelBehavior::AutoHide;
    QTest::newRow("can cover")
        << Wrapland::Client::PlasmaShellSurface::PanelBehavior::WindowsCanCover
        << Wrapland::Server::PlasmaShellSurface::PanelBehavior::WindowsCanCover;
    QTest::newRow("go below")
        << Wrapland::Client::PlasmaShellSurface::PanelBehavior::WindowsGoBelow
        << Wrapland::Server::PlasmaShellSurface::PanelBehavior::WindowsGoBelow;
}

void TestPlasmaShell::testPanelBehavior()
{
    // this test verifies that the panel behavior is properly passed to the server
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    ps->setRole(Wrapland::Client::PlasmaShellSurface::Role::Panel);
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QCOMPARE(sps->panelBehavior(),
             Wrapland::Server::PlasmaShellSurface::PanelBehavior::AlwaysVisible);

    // now change the behavior
    QSignalSpy behaviorChangedSpy(sps, &Wrapland::Server::PlasmaShellSurface::panelBehaviorChanged);
    QVERIFY(behaviorChangedSpy.isValid());
    QFETCH(Wrapland::Client::PlasmaShellSurface::PanelBehavior, client);
    ps->setPanelBehavior(client);
    QVERIFY(behaviorChangedSpy.wait());
    QTEST(sps->panelBehavior(), "server");

    // changing to same should not trigger the signal
    ps->setPanelBehavior(client);
    QVERIFY(!behaviorChangedSpy.wait(100));

    // but changing back to Always Visible should work
    ps->setPanelBehavior(Wrapland::Client::PlasmaShellSurface::PanelBehavior::AlwaysVisible);
    QVERIFY(behaviorChangedSpy.wait());
    QCOMPARE(sps->panelBehavior(),
             Wrapland::Server::PlasmaShellSurface::PanelBehavior::AlwaysVisible);
}

void TestPlasmaShell::testAutoHidePanel()
{
    // this test verifies that auto-hiding panels work correctly
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    ps->setRole(Wrapland::Client::PlasmaShellSurface::Role::Panel);
    ps->setPanelBehavior(Wrapland::Client::PlasmaShellSurface::PanelBehavior::AutoHide);
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    auto sps
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QVERIFY(sps);
    QCOMPARE(sps->panelBehavior(), Wrapland::Server::PlasmaShellSurface::PanelBehavior::AutoHide);

    QSignalSpy autoHideRequestedSpy(
        sps, &Wrapland::Server::PlasmaShellSurface::panelAutoHideHideRequested);
    QVERIFY(autoHideRequestedSpy.isValid());
    QSignalSpy autoHideShowRequestedSpy(
        sps, &Wrapland::Server::PlasmaShellSurface::panelAutoHideShowRequested);
    QVERIFY(autoHideShowRequestedSpy.isValid());
    ps->requestHideAutoHidingPanel();
    QVERIFY(autoHideRequestedSpy.wait());
    QCOMPARE(autoHideRequestedSpy.count(), 1);
    QCOMPARE(autoHideShowRequestedSpy.count(), 0);

    QSignalSpy panelShownSpy(ps.get(), &Wrapland::Client::PlasmaShellSurface::autoHidePanelShown);
    QVERIFY(panelShownSpy.isValid());
    QSignalSpy panelHiddenSpy(ps.get(), &Wrapland::Client::PlasmaShellSurface::autoHidePanelHidden);
    QVERIFY(panelHiddenSpy.isValid());

    sps->hideAutoHidingPanel();
    QVERIFY(panelHiddenSpy.wait());
    QCOMPARE(panelHiddenSpy.count(), 1);
    QCOMPARE(panelShownSpy.count(), 0);

    ps->requestShowAutoHidingPanel();
    QVERIFY(autoHideShowRequestedSpy.wait());
    QCOMPARE(autoHideRequestedSpy.count(), 1);
    QCOMPARE(autoHideShowRequestedSpy.count(), 1);

    sps->showAutoHidingPanel();
    QVERIFY(panelShownSpy.wait());
    QCOMPARE(panelHiddenSpy.count(), 1);
    QCOMPARE(panelShownSpy.count(), 1);

    // change panel type
    ps->setPanelBehavior(Wrapland::Client::PlasmaShellSurface::PanelBehavior::AlwaysVisible);
    // requesting auto hide should raise error
    QSignalSpy errorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    ps->requestHideAutoHidingPanel();
    QVERIFY(errorSpy.wait());
}

void TestPlasmaShell::testPanelTakesFocus()
{
    // this test verifies that whether a panel wants to take focus is passed through correctly
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);

    QVERIFY(plasmaSurfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    ps->setRole(Wrapland::Client::PlasmaShellSurface::Role::Panel);
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    auto sps
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QSignalSpy plasmaSurfaceTakesFocusSpy(
        sps, &Wrapland::Server::PlasmaShellSurface::panelTakesFocusChanged);

    QVERIFY(sps);
    QCOMPARE(sps->role(), Wrapland::Server::PlasmaShellSurface::Role::Panel);
    QCOMPARE(sps->panelTakesFocus(), false);

    ps->setPanelTakesFocus(true);
    m_connection->flush();
    QVERIFY(plasmaSurfaceTakesFocusSpy.wait());
    QCOMPARE(plasmaSurfaceTakesFocusSpy.count(), 1);
    QCOMPARE(sps->panelTakesFocus(), true);
    ps->setPanelTakesFocus(false);
    m_connection->flush();
    QVERIFY(plasmaSurfaceTakesFocusSpy.wait());
    QCOMPARE(plasmaSurfaceTakesFocusSpy.count(), 2);
    QCOMPARE(sps->panelTakesFocus(), false);
}

void TestPlasmaShell::test_open_under_cursor()
{
    // Verifies that whether a window wants to open under cursor is passed through
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());

    auto surface = std::unique_ptr<Wrapland::Client::Surface>(m_compositor->createSurface());
    auto pss = std::unique_ptr<Wrapland::Client::PlasmaShellSurface>(
        m_plasmaShell->createSurface(surface.get()));
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);

    auto spss
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QVERIFY(spss);

    QSignalSpy open_under_cursor_spy(
        spss, &Wrapland::Server::PlasmaShellSurface::open_under_cursor_requested);
    QVERIFY(open_under_cursor_spy.isValid());

    QVERIFY(!spss->open_under_cursor());

    pss->request_open_under_cursor();
    QVERIFY(open_under_cursor_spy.wait());
    QCOMPARE(open_under_cursor_spy.count(), 1);
    QVERIFY(spss->open_under_cursor());
}

void TestPlasmaShell::testDisconnect()
{
    // this test verifies that a disconnect cleans up
    QSignalSpy plasmaSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                       &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());
    // create the surface
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));

    // and get them on the server
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    auto sps
        = plasmaSurfaceCreatedSpy.first().first().value<Wrapland::Server::PlasmaShellSurface*>();
    QVERIFY(sps);

    // disconnect
    QSignalSpy clientDisconnectedSpy(sps->client(), &Wrapland::Server::Client::disconnected);
    QVERIFY(clientDisconnectedSpy.isValid());
    QSignalSpy surfaceDestroyedSpy(sps, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());

    QVERIFY(m_connection);
    s->release();
    ps->release();
    m_plasmaShell->release();
    m_compositor->release();
    m_registry->release();
    m_queue->release();

    QCOMPARE(surfaceDestroyedSpy.count(), 0);

    m_connection->deleteLater();
    m_connection = nullptr;

    QVERIFY(clientDisconnectedSpy.wait());
    QCOMPARE(clientDisconnectedSpy.count(), 1);
    QTRY_COMPARE(surfaceDestroyedSpy.count(), 1);
}

void TestPlasmaShell::testWhileDestroying()
{
    // this test tries to hit a condition that a Surface gets created with an ID which was already
    // used for a previous Surface. For each Surface we try to create a PlasmaShellSurface.
    // Even if there was a Surface in the past with the same ID, it should create the
    // PlasmaShellSurface
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // create ShellSurface
    QSignalSpy shellSurfaceCreatedSpy(server.globals.plasma_shell.get(),
                                      &Wrapland::Server::PlasmaShell::surfaceCreated);
    QVERIFY(shellSurfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.get()));
    QVERIFY(shellSurfaceCreatedSpy.wait());

    // now try to create more surfaces
    QSignalSpy clientErrorSpy(m_connection,
                              &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(clientErrorSpy.isValid());
    for (int i = 0; i < 100; i++) {
        s.reset();
        s.reset(m_compositor->createSurface());
        m_plasmaShell->createSurface(s.get(), this);
        QVERIFY(surfaceCreatedSpy.wait());
    }
    QVERIFY(clientErrorSpy.isEmpty());
    QVERIFY(!clientErrorSpy.wait(100));
    QVERIFY(clientErrorSpy.isEmpty());
}

QTEST_GUILESS_MAIN(TestPlasmaShell)
#include "plasma_shell.moc"
