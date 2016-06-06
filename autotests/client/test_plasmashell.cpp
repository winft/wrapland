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
// Qt
#include <QtTest/QtTest>
// KWayland
#include "../../src/client/connection_thread.h"
#include "../../src/client/compositor.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"
#include "../../src/client/plasmashell.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/plasmashell_interface.h"


using namespace KWayland::Client;
using namespace KWayland::Server;


class TestPlasmaShell : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();


    void testRole_data();
    void testRole();
    void testDisconnect();

private:
    Display *m_display = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    PlasmaShellInterface *m_plasmaShellInterface = nullptr;

    ConnectionThread *m_connection = nullptr;
    Compositor *m_compositor = nullptr;
    EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
    Registry *m_registry = nullptr;
    PlasmaShell *m_plasmaShell = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-plasma-shell-0");

void TestPlasmaShell::init()
{
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    m_display->createShm();

    m_plasmaShellInterface = m_display->createPlasmaShell(m_display);
    m_plasmaShellInterface->create();

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Registry();
    QSignalSpy interfacesAnnouncedSpy(m_registry, &Registry::interfaceAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    QVERIFY(interfacesAnnouncedSpy.wait());
#define CREATE(variable, factory, iface) \
    variable = m_registry->create##factory(m_registry->interface(Registry::Interface::iface).name, m_registry->interface(Registry::Interface::iface).version, this); \
    QVERIFY(variable);

    CREATE(m_compositor, Compositor, Compositor)
    CREATE(m_plasmaShell, PlasmaShell, PlasmaShell)

#undef CREATE

}

void TestPlasmaShell::cleanup()
{
#define DELETE(name) \
    if (name) { \
        delete name; \
        name = nullptr; \
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

    delete m_display;
    m_display = nullptr;
}

void TestPlasmaShell::testRole_data()
{
    QTest::addColumn<KWayland::Client::PlasmaShellSurface::Role>("clientRole");
    QTest::addColumn<KWayland::Server::PlasmaShellSurfaceInterface::Role>("serverRole");

    QTest::newRow("desktop") << PlasmaShellSurface::Role::Desktop << PlasmaShellSurfaceInterface::Role::Desktop;
    QTest::newRow("osd") << PlasmaShellSurface::Role::OnScreenDisplay << PlasmaShellSurfaceInterface::Role::OnScreenDisplay;
    QTest::newRow("panel") << PlasmaShellSurface::Role::Panel << PlasmaShellSurfaceInterface::Role::Panel;
}

void TestPlasmaShell::testRole()
{
    // this test verifies that setting the role on a plasma shell surface works

    // first create signal spies
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());

    // create the surface
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QScopedPointer<PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.data()));

    // and get them on the server
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    QCOMPARE(surfaceCreatedSpy.count(), 1);

    // verify that we got a plasma shell surface
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface*>();
    QVERIFY(sps);
    QVERIFY(sps->surface());
    QCOMPARE(sps->surface(), surfaceCreatedSpy.first().first().value<SurfaceInterface*>());

    // default role should be normal
    QCOMPARE(sps->role(), PlasmaShellSurfaceInterface::Role::Normal);

    // now change it
    QSignalSpy roleChangedSpy(sps, &PlasmaShellSurfaceInterface::roleChanged);
    QVERIFY(roleChangedSpy.isValid());
    QFETCH(PlasmaShellSurface::Role, clientRole);
    ps->setRole(clientRole);
    QVERIFY(roleChangedSpy.wait());
    QCOMPARE(roleChangedSpy.count(), 1);
    QTEST(sps->role(), "serverRole");
}

void TestPlasmaShell::testDisconnect()
{
    // this test verifies that a disconnect cleans up
    QSignalSpy plasmaSurfaceCreatedSpy(m_plasmaShellInterface, &PlasmaShellInterface::surfaceCreated);
    QVERIFY(plasmaSurfaceCreatedSpy.isValid());
    // create the surface
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QScopedPointer<PlasmaShellSurface> ps(m_plasmaShell->createSurface(s.data()));

    // and get them on the server
    QVERIFY(plasmaSurfaceCreatedSpy.wait());
    QCOMPARE(plasmaSurfaceCreatedSpy.count(), 1);
    auto sps = plasmaSurfaceCreatedSpy.first().first().value<PlasmaShellSurfaceInterface*>();
    QVERIFY(sps);

    // disconnect
    QSignalSpy clientDisconnectedSpy(sps->client(), &ClientConnection::disconnected);
    QVERIFY(clientDisconnectedSpy.isValid());
    QSignalSpy surfaceDestroyedSpy(sps, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    QVERIFY(clientDisconnectedSpy.wait());
    QCOMPARE(clientDisconnectedSpy.count(), 1);
    QCOMPARE(surfaceDestroyedSpy.count(), 0);
    QVERIFY(surfaceDestroyedSpy.wait());
    QCOMPARE(surfaceDestroyedSpy.count(), 1);

    s->destroy();
    ps->destroy();
    m_plasmaShell->destroy();
    m_compositor->destroy();
    m_registry->destroy();
    m_queue->destroy();
}

QTEST_GUILESS_MAIN(TestPlasmaShell)
#include "test_plasmashell.moc"