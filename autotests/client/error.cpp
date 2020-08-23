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
#include "../../src/client/xdgshell.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/plasma_shell.h"
#include "../../server/xdg_shell.h"

#include <wayland-client-protocol.h>

#include <errno.h> // For EPROTO

class ErrorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testMultipleShellSurfacesForSurface();
    void testMultiplePlasmaShellSurfacesForSurface();

private:
    Wrapland::Server::Display* m_display = nullptr;
    Wrapland::Server::Compositor* m_serverCompositor = nullptr;
    Wrapland::Server::XdgShell* m_serverXdgShell = nullptr;
    Wrapland::Server::PlasmaShell* m_psi = nullptr;

    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    QThread* m_thread = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::XdgShell* m_shell = nullptr;
    Wrapland::Client::PlasmaShell* m_plasmaShell = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-error-0");

void ErrorTest::init()
{
    m_display = new Wrapland::Server::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_display->createShm();
    m_serverCompositor = m_display->createCompositor(m_display);

    m_serverXdgShell = m_display->createXdgShell(m_display);
    m_psi = m_display->createPlasmaShell(m_display);

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
    m_queue->setup(m_connection);

    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor = registry.createCompositor(
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).name,
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).version,
        this);
    QVERIFY(m_compositor);
    m_shell = registry.createXdgShell(
        registry.interface(Wrapland::Client::Registry::Interface::XdgShellStable).name,
        registry.interface(Wrapland::Client::Registry::Interface::XdgShellStable).version,
        this);
    QVERIFY(m_shell);
    m_plasmaShell = registry.createPlasmaShell(
        registry.interface(Wrapland::Client::Registry::Interface::PlasmaShell).name,
        registry.interface(Wrapland::Client::Registry::Interface::PlasmaShell).version,
        this);
    QVERIFY(m_plasmaShell);
}

void ErrorTest::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_plasmaShell)
    CLEANUP(m_shell)
    CLEANUP(m_compositor)
    CLEANUP(m_queue)
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
    CLEANUP(m_psi)
    CLEANUP(m_serverXdgShell)
    CLEANUP(m_serverCompositor)
    CLEANUP(m_display)
#undef CLEANUP
}

void ErrorTest::testMultipleShellSurfacesForSurface()
{
    // this test verifies that creating two ShellSurfaces for the same Surface triggers a protocol
    // error
    QSignalSpy errorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::XdgShellSurface> shellSurface1(
        m_shell->createSurface(surface.get()));
    std::unique_ptr<Wrapland::Client::XdgShellSurface> shellSurface2(
        m_shell->createSurface(surface.get()));

    QCOMPARE(m_connection->error(), 0);
    QVERIFY(errorSpy.wait());
    QCOMPARE(m_connection->error(), EPROTO);
    QVERIFY(m_connection->hasProtocolError());
    QCOMPARE(m_connection->protocolError(), WL_DISPLAY_ERROR_INVALID_OBJECT);
}

void ErrorTest::testMultiplePlasmaShellSurfacesForSurface()
{
    // this test verifies that creating two ShellSurfaces for the same Surface triggers a protocol
    // error
    QSignalSpy errorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    // PlasmaShell is too smart and doesn't allow us to create a second PlasmaShellSurface
    // thus we need to cheat by creating a surface manually
    auto surface = wl_compositor_create_surface(*m_compositor);
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> shellSurface1(
        m_plasmaShell->createSurface(surface));
    std::unique_ptr<Wrapland::Client::PlasmaShellSurface> shellSurface2(
        m_plasmaShell->createSurface(surface));
    QCOMPARE(m_connection->error(), 0);
    QVERIFY(errorSpy.wait());
    QCOMPARE(m_connection->error(), EPROTO);
    QVERIFY(m_connection->hasProtocolError());
    QCOMPARE(m_connection->protocolError(), WL_DISPLAY_ERROR_INVALID_OBJECT);
    wl_surface_destroy(surface);
}

QTEST_GUILESS_MAIN(ErrorTest)
#include "error.moc"
