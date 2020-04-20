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
#ifndef TEST_XDG_SHELL_H
#define TEST_XDG_SHELL_H

#include <QtTest>

#include "../../src/client/xdgshell.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/compositor.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/output.h"
#include "../../src/client/seat.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/surface.h"

#include "../../server/display.h"
#include "../../server/compositor.h"
#include "../../server/surface.h"
#include "../../server/output.h"

#include "../../src/server/seat_interface.h"
#include "../../src/server/xdgshell_interface.h"

Q_DECLARE_METATYPE(Qt::MouseButton)

class XdgShellTest : public QObject
{
    Q_OBJECT

protected:
    XdgShellTest(Wrapland::Server::XdgShellInterfaceVersion version);
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

protected:
    Wrapland::Server::XdgShellInterface *m_xdgShellInterface = nullptr;
    Wrapland::Client::Compositor *m_compositor = nullptr;
    Wrapland::Client::XdgShell *m_xdgShell = nullptr;
    Wrapland::Server::D_isplay *m_display = nullptr;
    Wrapland::Server::Compositor *m_serverCompositor = nullptr;
    Wrapland::Server::Output *m_o1Interface = nullptr;
    Wrapland::Server::Output *m_o2Interface = nullptr;
    Wrapland::Server::Seat *m_serverSeat = nullptr;
    Wrapland::Client::ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    Wrapland::Client::EventQueue *m_queue = nullptr;
    Wrapland::Client::ShmPool *m_shmPool = nullptr;
    Wrapland::Client::Output *m_output1 = nullptr;
    Wrapland::Client::Output *m_output2 = nullptr;
    Wrapland::Client::Seat *m_seat = nullptr;

private:
    Wrapland::Server::XdgShellInterfaceVersion m_version;
};

#define SURFACE \
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &Wrapland::Server::XdgShellInterface::surfaceCreated); \
    QVERIFY(xdgSurfaceCreatedSpy.isValid()); \
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface()); \
    std::unique_ptr<Wrapland::Client::XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.get())); \
    QCOMPARE(xdgSurface->size(), QSize()); \
    QVERIFY(xdgSurfaceCreatedSpy.wait()); \
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<Wrapland::Server::XdgShellSurfaceInterface*>(); \
    QVERIFY(serverXdgSurface);

#endif
