/********************************************************************
Copyright 2015 Marco Martin <mart@kde.org>

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
#include "../../src/client/plasmawindowmanagement.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/plasma_window.h"
#include "../../server/region.h"
#include "../../server/surface.h"

#include <wayland-plasma-window-management-client-protocol.h>

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

typedef void (Srv::PlasmaWindow::*ServerWindowSignal)();
Q_DECLARE_METATYPE(ServerWindowSignal)

typedef void (Srv::PlasmaWindow::*ServerWindowBooleanSignal)(bool);
Q_DECLARE_METATYPE(ServerWindowBooleanSignal)

typedef void (Wrapland::Client::PlasmaWindow::*ClientWindowVoidSetter)();
Q_DECLARE_METATYPE(ClientWindowVoidSetter)

class TestWindowManagement : public QObject
{
    Q_OBJECT
public:
    explicit TestWindowManagement(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testWindowTitle();
    void testMinimizedGeometry();

    void testUseAfterUnmap();
    void testCreateAfterUnmap();
    void testActiveWindowOnUnmapped();

    void testServerDelete();
    void testDeleteActiveWindow();

    void testRequests_data();
    void testRequests();

    void testRequestsBoolean_data();
    void testRequestsBoolean();

    void testKeepAbove();
    void testKeepBelow();
    void testShowingDesktop();

    void testRequestShowingDesktop_data();
    void testRequestShowingDesktop();

    void testParentWindow();
    void testGeometry();
    void testIcon();
    void testPid();
    void testApplicationMenu();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;

        Srv::PlasmaWindow* plasma_window{nullptr};
        Srv::Surface* surface{nullptr};
    } server;

    Clt::Surface* m_surface = nullptr;
    Clt::ConnectionThread* m_connection;
    Clt::Compositor* m_compositor;
    Clt::EventQueue* m_queue;
    Clt::PlasmaWindowManagement* m_windowManagement;
    Clt::PlasmaWindow* m_window;
    QThread* m_thread;
    Clt::Registry* m_registry;
};

constexpr auto socket_name{"wrapland-test-wayland-windowmanagement-0"};

TestWindowManagement::TestWindowManagement(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestWindowManagement::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();
    qRegisterMetaType<Srv::PlasmaWindowManager::ShowingDesktopState>("ShowingDesktopState");

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    // Setup connection.
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Clt::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Clt::Registry(this);
    QSignalSpy compositorSpy(m_registry, &Clt::Registry::compositorAnnounced);
    QVERIFY(compositorSpy.isValid());

    QSignalSpy windowManagementSpy(m_registry, &Clt::Registry::plasmaWindowManagementAnnounced);
    QVERIFY(windowManagementSpy.isValid());

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    server.globals.compositor = server.display->createCompositor();

    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(),
                                                compositorSpy.first().last().value<quint32>(),
                                                this);

    server.globals.plasma_window_manager = server.display->createPlasmaWindowManager();

    QVERIFY(windowManagementSpy.wait());
    m_windowManagement = m_registry->createPlasmaWindowManagement(
        windowManagementSpy.first().first().value<quint32>(),
        windowManagementSpy.first().last().value<quint32>(),
        this);

    QSignalSpy windowSpy(m_windowManagement, &Clt::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowSpy.isValid());
    server.plasma_window = server.globals.plasma_window_manager->createWindow(this);
    server.plasma_window->setPid(1337);

    QVERIFY(windowSpy.wait());
    m_window = windowSpy.first().first().value<Clt::PlasmaWindow*>();

    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    m_surface = m_compositor->createSurface(this);
    QVERIFY(m_surface);

    QVERIFY(serverSurfaceCreated.wait());
    server.surface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(server.surface);
}

void TestWindowManagement::cleanup()
{

    if (m_surface) {
        delete m_surface;
        m_surface = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_windowManagement) {
        delete m_windowManagement;
        m_windowManagement = nullptr;
    }
    if (m_registry) {
        delete m_registry;
        m_registry = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        if (m_connection) {
            m_connection->flush();
            m_connection->deleteLater();
        }
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    m_connection = nullptr;

    server = {};
}

void TestWindowManagement::testWindowTitle()
{
    server.plasma_window->setTitle(QStringLiteral("Test Title"));

    QSignalSpy titleSpy(m_window, &Clt::PlasmaWindow::titleChanged);
    QVERIFY(titleSpy.isValid());

    QVERIFY(titleSpy.wait());

    QCOMPARE(m_window->title(), QString::fromUtf8("Test Title"));
}

void TestWindowManagement::testMinimizedGeometry()
{
    m_window->setMinimizedGeometry(m_surface, QRect(5, 10, 100, 200));

    QSignalSpy geometrySpy(server.plasma_window, &Srv::PlasmaWindow::minimizedGeometriesChanged);
    QVERIFY(geometrySpy.isValid());

    QVERIFY(geometrySpy.wait());
    QCOMPARE(server.plasma_window->minimizedGeometries().values().first(), QRect(5, 10, 100, 200));

    m_window->unsetMinimizedGeometry(m_surface);
    QVERIFY(geometrySpy.wait());
    QVERIFY(server.plasma_window->minimizedGeometries().isEmpty());
}

void TestWindowManagement::testUseAfterUnmap()
{
    // This test verifies that when the client uses a window after it's unmapped things don't break.
    QSignalSpy unmappedSpy(m_window, &Clt::PlasmaWindow::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy destroyedSpy(m_window, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    QSignalSpy serverDestroyedSpy(server.plasma_window, &QObject::destroyed);
    QVERIFY(serverDestroyedSpy.isValid());

    server.plasma_window->unmap();
    server.plasma_window = nullptr;
    QCOMPARE(serverDestroyedSpy.count(), 1);
    m_window->requestClose();
    QVERIFY(unmappedSpy.wait());
    QVERIFY(destroyedSpy.wait());
    m_window = nullptr;
}

void TestWindowManagement::testCreateAfterUnmap()
{
    // This test verifies that we don't get a protocol error on client side when creating an already
    // unmapped window.
    QCOMPARE(m_windowManagement->children().count(), 1);

    // Create and unmap in one go.
    // Client will first handle the create, the unmap will be sent once the server side is bound.
    auto serverWindow = server.globals.plasma_window_manager->createWindow(this);
    serverWindow->unmap();

    QCOMPARE(server.globals.plasma_window_manager->children().count(), 0);
    QCoreApplication::instance()->processEvents();
    QCoreApplication::instance()->processEvents(QEventLoop::WaitForMoreEvents);
    QTRY_COMPARE(m_windowManagement->children().count(), 2);

    auto window = dynamic_cast<Clt::PlasmaWindow*>(m_windowManagement->children().last());
    QVERIFY(window);

    // Now this is not yet on the server, on the server it will be after next roundtrip
    // which we can trigger by waiting for destroy of the newly created window.
    // Why destroy? Because we will get the unmap which triggers a destroy.
    QSignalSpy clientDestroyedSpy(window, &QObject::destroyed);
    QVERIFY(clientDestroyedSpy.isValid());
    QVERIFY(clientDestroyedSpy.wait());
    QCOMPARE(m_windowManagement->children().count(), 1);

    // The server side created a helper PlasmaWindow but it was only temporary so there is only
    // to check for memory leaks with a sanitizer.
}

void TestWindowManagement::testActiveWindowOnUnmapped()
{
    // This test verifies that unmapping the active window changes the active window.
    // first make the window active
    QVERIFY(!m_windowManagement->activeWindow());
    QVERIFY(!m_window->isActive());
    QSignalSpy activeWindowChangedSpy(m_windowManagement,
                                      &Clt::PlasmaWindowManagement::activeWindowChanged);
    QVERIFY(activeWindowChangedSpy.isValid());

    server.plasma_window->setActive(true);
    QVERIFY(activeWindowChangedSpy.wait());
    QCOMPARE(m_windowManagement->activeWindow(), m_window);
    QVERIFY(m_window->isActive());

    // Now unmap should change the active window.
    QSignalSpy destroyedSpy(m_window, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    QSignalSpy serverDestroyedSpy(server.plasma_window, &QObject::destroyed);
    QVERIFY(serverDestroyedSpy.isValid());

    server.globals.plasma_window_manager->unmapWindow(server.plasma_window);
    server.plasma_window = nullptr;
    QCOMPARE(serverDestroyedSpy.count(), 1);
    QVERIFY(activeWindowChangedSpy.wait());
    QVERIFY(!m_windowManagement->activeWindow());
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(destroyedSpy.count(), 1);

    m_window = nullptr;
}

void TestWindowManagement::testServerDelete()
{
    QSignalSpy unmappedSpy(m_window, &Clt::PlasmaWindow::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy destroyedSpy(m_window, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());

    delete server.plasma_window;
    server.plasma_window = nullptr;

    QVERIFY(unmappedSpy.wait());
    QVERIFY(destroyedSpy.wait());
    m_window = nullptr;
}

void TestWindowManagement::testDeleteActiveWindow()
{
    // This test verifies that deleting the active window on client side changes the active window
    // first make the window active
    QVERIFY(!m_windowManagement->activeWindow());
    QVERIFY(!m_window->isActive());
    QSignalSpy activeWindowChangedSpy(m_windowManagement,
                                      &Clt::PlasmaWindowManagement::activeWindowChanged);
    QVERIFY(activeWindowChangedSpy.isValid());

    server.plasma_window->setActive(true);
    QVERIFY(activeWindowChangedSpy.wait());
    QCOMPARE(activeWindowChangedSpy.count(), 1);
    QCOMPARE(m_windowManagement->activeWindow(), m_window);
    QVERIFY(m_window->isActive());

    // Delete the client side window - that's semantically kind of wrong, but shouldn't make our
    // code crash.
    delete m_window;
    m_window = nullptr;
    QCOMPARE(activeWindowChangedSpy.count(), 2);
    QVERIFY(!m_windowManagement->activeWindow());
}

void TestWindowManagement::testRequests_data()
{
    QTest::addColumn<ServerWindowSignal>("changedSignal");
    QTest::addColumn<ClientWindowVoidSetter>("requester");

    QTest::newRow("close") << &Srv::PlasmaWindow::closeRequested
                           << &Clt::PlasmaWindow::requestClose;
    QTest::newRow("move") << &Srv::PlasmaWindow::moveRequested << &Clt::PlasmaWindow::requestMove;
    QTest::newRow("resize") << &Srv::PlasmaWindow::resizeRequested
                            << &Clt::PlasmaWindow::requestResize;
}

void TestWindowManagement::testRequests()
{
    // This test case verifies all the different requests on a PlasmaWindow
    QFETCH(ServerWindowSignal, changedSignal);
    QSignalSpy requestSpy(server.plasma_window, changedSignal);
    QVERIFY(requestSpy.isValid());

    QFETCH(ClientWindowVoidSetter, requester);
    (m_window->*(requester))();
    QVERIFY(requestSpy.wait());
}

void TestWindowManagement::testRequestsBoolean_data()
{
    QTest::addColumn<ServerWindowBooleanSignal>("changedSignal");
    QTest::addColumn<int>("flag");

    QTest::newRow("activate") << &Srv::PlasmaWindow::activeRequested
                              << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
    QTest::newRow("minimized") << &Srv::PlasmaWindow::minimizedRequested
                               << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
    QTest::newRow("maximized") << &Srv::PlasmaWindow::maximizedRequested
                               << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED);
    QTest::newRow("fullscreen") << &Srv::PlasmaWindow::fullscreenRequested
                                << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN);
    QTest::newRow("keepAbove") << &Srv::PlasmaWindow::keepAboveRequested
                               << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE);
    QTest::newRow("keepBelow") << &Srv::PlasmaWindow::keepBelowRequested
                               << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW);
    QTest::newRow("demandsAttention")
        << &Srv::PlasmaWindow::demandsAttentionRequested
        << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION);
    QTest::newRow("closeable") << &Srv::PlasmaWindow::closeableRequested
                               << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE);
    QTest::newRow("minimizable") << &Srv::PlasmaWindow::minimizeableRequested
                                 << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE);
    QTest::newRow("maximizable") << &Srv::PlasmaWindow::maximizeableRequested
                                 << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE);
    QTest::newRow("fullscreenable") << &Srv::PlasmaWindow::fullscreenableRequested
                                    << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE);
    QTest::newRow("skiptaskbar") << &Srv::PlasmaWindow::skipTaskbarRequested
                                 << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR);
    QTest::newRow("skipSwitcher") << &Srv::PlasmaWindow::skipSwitcherRequested
                                  << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER);
    QTest::newRow("shadeable") << &Srv::PlasmaWindow::shadeableRequested
                               << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE);
    QTest::newRow("shaded") << &Srv::PlasmaWindow::shadedRequested
                            << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED);
    QTest::newRow("movable") << &Srv::PlasmaWindow::movableRequested
                             << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE);
    QTest::newRow("resizable") << &Srv::PlasmaWindow::resizableRequested
                               << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE);
    QTest::newRow("virtualDesktopChangeable")
        << &Srv::PlasmaWindow::virtualDesktopChangeableRequested
        << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE);
}

void TestWindowManagement::testRequestsBoolean()
{
    // This test case verifies all the different requests on a PlasmaWindow.
    QFETCH(ServerWindowBooleanSignal, changedSignal);
    QSignalSpy requestSpy(server.plasma_window, changedSignal);
    QVERIFY(requestSpy.isValid());
    QFETCH(int, flag);

    org_kde_plasma_window_set_state(*m_window, flag, flag);
    QVERIFY(requestSpy.wait());
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.first().first().toBool(), true);

    org_kde_plasma_window_set_state(*m_window, flag, 0);
    QVERIFY(requestSpy.wait());
    QCOMPARE(requestSpy.count(), 2);
    QCOMPARE(requestSpy.last().first().toBool(), false);
}

void TestWindowManagement::testShowingDesktop()
{
    // This test verifies setting the showing desktop state.
    QVERIFY(!m_windowManagement->isShowingDesktop());
    QSignalSpy showingDesktopChangedSpy(m_windowManagement,
                                        &Clt::PlasmaWindowManagement::showingDesktopChanged);
    QVERIFY(showingDesktopChangedSpy.isValid());
    server.globals.plasma_window_manager->setShowingDesktopState(
        Srv::PlasmaWindowManager::ShowingDesktopState::Enabled);
    QVERIFY(showingDesktopChangedSpy.wait());
    QCOMPARE(showingDesktopChangedSpy.count(), 1);
    QCOMPARE(showingDesktopChangedSpy.first().first().toBool(), true);
    QVERIFY(m_windowManagement->isShowingDesktop());

    // Setting to same should not change.
    server.globals.plasma_window_manager->setShowingDesktopState(
        Srv::PlasmaWindowManager::ShowingDesktopState::Enabled);
    QVERIFY(!showingDesktopChangedSpy.wait(100));
    QCOMPARE(showingDesktopChangedSpy.count(), 1);

    // Setting to other state should change.
    server.globals.plasma_window_manager->setShowingDesktopState(
        Srv::PlasmaWindowManager::ShowingDesktopState::Disabled);
    QVERIFY(showingDesktopChangedSpy.wait());
    QCOMPARE(showingDesktopChangedSpy.count(), 2);
    QCOMPARE(showingDesktopChangedSpy.first().first().toBool(), true);
    QCOMPARE(showingDesktopChangedSpy.last().first().toBool(), false);
    QVERIFY(!m_windowManagement->isShowingDesktop());
}

void TestWindowManagement::testRequestShowingDesktop_data()
{
    QTest::addColumn<bool>("value");
    QTest::addColumn<Srv::PlasmaWindowManager::ShowingDesktopState>("expectedValue");

    QTest::newRow("enable") << true << Srv::PlasmaWindowManager::ShowingDesktopState::Enabled;
    QTest::newRow("disable") << false << Srv::PlasmaWindowManager::ShowingDesktopState::Disabled;
}

void TestWindowManagement::testRequestShowingDesktop()
{
    // This test verifies requesting show desktop state.
    QSignalSpy requestSpy(server.globals.plasma_window_manager.get(),
                          &Srv::PlasmaWindowManager::requestChangeShowingDesktop);
    QVERIFY(requestSpy.isValid());
    QFETCH(bool, value);

    m_windowManagement->setShowingDesktop(value);
    QVERIFY(requestSpy.wait());
    QCOMPARE(requestSpy.count(), 1);
    QTEST(requestSpy.first().first().value<Srv::PlasmaWindowManager::ShowingDesktopState>(),
          "expectedValue");
}

void TestWindowManagement::testKeepAbove()
{
    // This test verifies setting the keep above state.
    QVERIFY(!m_window->isKeepAbove());
    QSignalSpy keepAboveChangedSpy(m_window, &Clt::PlasmaWindow::keepAboveChanged);
    QVERIFY(keepAboveChangedSpy.isValid());
    server.plasma_window->setKeepAbove(true);
    QVERIFY(keepAboveChangedSpy.wait());
    QCOMPARE(keepAboveChangedSpy.count(), 1);
    QVERIFY(m_window->isKeepAbove());

    // Setting to same should not change.
    server.plasma_window->setKeepAbove(true);
    QVERIFY(!keepAboveChangedSpy.wait(100));
    QCOMPARE(keepAboveChangedSpy.count(), 1);

    // Setting to other state should change.
    server.plasma_window->setKeepAbove(false);
    QVERIFY(keepAboveChangedSpy.wait());
    QCOMPARE(keepAboveChangedSpy.count(), 2);
    QVERIFY(!m_window->isKeepAbove());
}

void TestWindowManagement::testKeepBelow()
{
    // This test verifies setting the keep below state.
    QVERIFY(!m_window->isKeepBelow());
    QSignalSpy keepBelowChangedSpy(m_window, &Clt::PlasmaWindow::keepBelowChanged);
    QVERIFY(keepBelowChangedSpy.isValid());

    server.plasma_window->setKeepBelow(true);
    QVERIFY(keepBelowChangedSpy.wait());
    QCOMPARE(keepBelowChangedSpy.count(), 1);
    QVERIFY(m_window->isKeepBelow());

    // Setting to same should not change.
    server.plasma_window->setKeepBelow(true);
    QVERIFY(!keepBelowChangedSpy.wait(100));
    QCOMPARE(keepBelowChangedSpy.count(), 1);

    // Setting to other state should change.
    server.plasma_window->setKeepBelow(false);
    QVERIFY(keepBelowChangedSpy.wait());
    QCOMPARE(keepBelowChangedSpy.count(), 2);
    QVERIFY(!m_window->isKeepBelow());
}

void TestWindowManagement::testParentWindow()
{
    // This test verifies the functionality of ParentWindows.
    QCOMPARE(m_windowManagement->windows().count(), 1);
    auto parentWindow = m_windowManagement->windows().first();
    QVERIFY(parentWindow);
    QVERIFY(parentWindow->parentWindow().isNull());

    // Now let's create a second window.
    QSignalSpy windowAddedSpy(m_windowManagement, &Clt::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowAddedSpy.isValid());
    auto serverTransient = server.globals.plasma_window_manager->createWindow(this);
    serverTransient->setParentWindow(server.plasma_window);
    QVERIFY(windowAddedSpy.wait());
    auto transient = windowAddedSpy.first().first().value<Clt::PlasmaWindow*>();
    QCOMPARE(transient->parentWindow().data(), parentWindow);

    // Let's unset the parent.
    QSignalSpy parentWindowChangedSpy(transient, &Clt::PlasmaWindow::parentWindowChanged);
    QVERIFY(parentWindowChangedSpy.isValid());
    serverTransient->setParentWindow(nullptr);
    QVERIFY(parentWindowChangedSpy.wait());
    QVERIFY(transient->parentWindow().isNull());

    // And set it again.
    serverTransient->setParentWindow(server.plasma_window);
    QVERIFY(parentWindowChangedSpy.wait());
    QCOMPARE(transient->parentWindow().data(), parentWindow);

    // Now let's try to unmap the parent.
    server.plasma_window->unmap();
    m_window = nullptr;
    server.plasma_window = nullptr;
    QVERIFY(parentWindowChangedSpy.wait());
    QVERIFY(transient->parentWindow().isNull());
}

void TestWindowManagement::testGeometry()
{
    QVERIFY(m_window);
    QCOMPARE(m_window->geometry(), QRect());
    QSignalSpy windowGeometryChangedSpy(m_window, &Clt::PlasmaWindow::geometryChanged);
    QVERIFY(windowGeometryChangedSpy.isValid());

    server.plasma_window->setGeometry(QRect(20, -10, 30, 40));
    QVERIFY(windowGeometryChangedSpy.wait());
    QCOMPARE(m_window->geometry(), QRect(20, -10, 30, 40));

    // Setting an empty geometry should not be sent to the client.
    server.plasma_window->setGeometry(QRect());
    QVERIFY(!windowGeometryChangedSpy.wait(10));

    // Setting to the geometry which the client still has should not trigger signal.
    server.plasma_window->setGeometry(QRect(20, -10, 30, 40));
    QVERIFY(!windowGeometryChangedSpy.wait(10));

    // Setting another geometry should work though.
    server.plasma_window->setGeometry(QRect(0, 0, 35, 45));
    QVERIFY(windowGeometryChangedSpy.wait());
    QCOMPARE(windowGeometryChangedSpy.count(), 2);
    QCOMPARE(m_window->geometry(), QRect(0, 0, 35, 45));

    // Let's bind a second PlasmaWindowManagement to verify the initial setting.
    std::unique_ptr<Clt::PlasmaWindowManagement> pm(m_registry->createPlasmaWindowManagement(
        m_registry->interface(Clt::Registry::Interface::PlasmaWindowManagement).name,
        m_registry->interface(Clt::Registry::Interface::PlasmaWindowManagement).version));

    QVERIFY(pm != nullptr);
    QSignalSpy windowAddedSpy(pm.get(), &Clt::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowAddedSpy.isValid());
    QVERIFY(windowAddedSpy.wait());

    auto window = pm->windows().first();
    QCOMPARE(window->geometry(), QRect(0, 0, 35, 45));
}

void TestWindowManagement::testIcon()
{
    // Disable the icon test for now. Our way of providing icons is fundamentally wrong and the
    // whole concept needs to be redone so it works on all setups and in particular in a CI setting.
    // See issue #8.
    QEXPECT_FAIL("", "Icon test currently disabled (see issue #8).", Abort);
    QVERIFY(false);
#if 0
    // Initially the server should send us an icon.
    QSignalSpy iconChangedSpy(m_window, &Clt::PlasmaWindow::iconChanged);
    QVERIFY(iconChangedSpy.isValid());
    QVERIFY(iconChangedSpy.wait());
    QCOMPARE(iconChangedSpy.count(), 1);
    if (!QIcon::hasThemeIcon(QStringLiteral("wayland"))) {
        QEXPECT_FAIL("", "no icon", Continue);
    }
    QCOMPARE(m_window->icon().name(), QStringLiteral("wayland"));

    // First goes from themed name to empty.
    server.plasma_window->setIcon(QIcon());
    QVERIFY(iconChangedSpy.wait());
    QCOMPARE(iconChangedSpy.count(), 2);
    if (!QIcon::hasThemeIcon(QStringLiteral("wayland"))) {
        QEXPECT_FAIL("", "no icon", Continue);
    }
    QCOMPARE(m_window->icon().name(), QStringLiteral("wayland"));

    // We can't use QPixmap in a QTEST_GUILESS_MAIN test. For that we would need QTEST_MAIN. But
    // when we do this QtWayland is fired up opening all kind of entry points for errors and memory
    // leaks.
    //
    // Because of that for now just disable the test on setting the QPixmap. The real fix is to
    // change the API such that not a QIcon is set but only a QImage and an alternative icon name.
    // Create an icon with a pixmap.
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::red);
    QImage image = pixmap.toImage();

    server.plasma_window->setIcon(pixmap);
    QVERIFY(iconChangedSpy.wait());
    QCOMPARE(iconChangedSpy.count(), 3);

    QVERIFY(!m_window->icon().isNull());
    const QImage cmp = m_window->icon().pixmap(32, 32).toImage();

    QCOMPARE(cmp.size(), image.size());

    // Image format might be different from QPixmap to QIcon transformation. So check only
    // the raw pixel data.
    for (int i = 0; i < image.width() * image.height(); i++) {
        QCOMPARE(image.constBits()[i], cmp.constBits()[i]);
    }

    // Let's set a themed icon.
    server.plasma_window->setIcon(QIcon::fromTheme(QStringLiteral("xorg")));
    QVERIFY(iconChangedSpy.wait());
    QCOMPARE(iconChangedSpy.count(), 3);

    if (!QIcon::hasThemeIcon(QStringLiteral("xorg"))) {
        QEXPECT_FAIL("", "no icon", Continue);
    }
    QCOMPARE(m_window->icon().name(), QStringLiteral("xorg"));
#endif
}

void TestWindowManagement::testPid()
{
    QVERIFY(m_window);
    QVERIFY(m_window->pid() == 1337);

    // Test server not setting a PID for whatever reason.
    std::unique_ptr<Srv::PlasmaWindow> newWindowInterface(
        server.globals.plasma_window_manager->createWindow(this));

    QSignalSpy windowSpy(m_windowManagement, &Clt::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowSpy.wait());

    std::unique_ptr<Clt::PlasmaWindow> newWindow(
        windowSpy.first().first().value<Clt::PlasmaWindow*>());
    QVERIFY(newWindow);
    QVERIFY(newWindow->pid() == 0);
}

void TestWindowManagement::testApplicationMenu()
{
    const auto serviceName = QStringLiteral("org.kde.foo");
    const auto objectPath = QStringLiteral("/org/kde/bar");

    QSignalSpy applicationMenuChangedSpy(m_window, &Clt::PlasmaWindow::applicationMenuChanged);
    QVERIFY(applicationMenuChangedSpy.isValid());

    server.plasma_window->setApplicationMenuPaths(serviceName, objectPath);

    QVERIFY(applicationMenuChangedSpy.wait());

    QCOMPARE(m_window->applicationMenuServiceName(), serviceName);
    QCOMPARE(m_window->applicationMenuObjectPath(), objectPath);
}

QTEST_GUILESS_MAIN(TestWindowManagement)
#include "plasma_window_management.moc"
