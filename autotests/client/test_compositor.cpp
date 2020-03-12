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

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/surface.h"
#include "../../src/client/registry.h"
#include "../../src/client/shm_pool.h"
#include "../../src/server/buffer_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/surface_interface.h"

class TestCompositor : public QObject
{
    Q_OBJECT
public:
    explicit TestCompositor(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testConnectionLoss();
    void testCast();

private:
    Wrapland::Server::Display *m_display;
    Wrapland::Server::CompositorInterface *m_compositorInterface;
    Wrapland::Client::ConnectionThread *m_connection;
    Wrapland::Client::Compositor *m_compositor;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-compositor-0");

TestCompositor::TestCompositor(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_thread(nullptr)
{
}

void TestCompositor::init()
{
    using namespace Wrapland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy establishedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(establishedSpy.wait());

    QSignalSpy clientConnectedSpy(m_display, &Display::clientConnected);
    QVERIFY(clientConnectedSpy.isValid());

    Wrapland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    // here we need a shm pool
    m_compositorInterface = m_display->createCompositor(m_display);
    QVERIFY(m_compositorInterface);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    QVERIFY(clientConnectedSpy.wait());
}

void TestCompositor::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
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

void TestCompositor::testConnectionLoss()
{
    using namespace Wrapland::Client;
    QVERIFY(m_compositor->isValid());

    QSignalSpy connectionSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectionSpy.isValid());
    delete m_display;
    m_display = nullptr;
    QTRY_COMPARE(connectionSpy.count(), 1);

    // The compositor pointer should still exist.
    QVERIFY(m_compositor->isValid());
}

void TestCompositor::testCast()
{
    using namespace Wrapland::Client;
    Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(compositorSpy.wait());

    Compositor c;
    auto wlComp = registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>());
    c.setup(wlComp);
    QCOMPARE((wl_compositor*)c, wlComp);

    const Compositor &c2(c);
    QCOMPARE((wl_compositor*)c2, wlComp);
}

QTEST_GUILESS_MAIN(TestCompositor)
#include "test_compositor.moc"
