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
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/subcompositor.h"
#include "../../src/server/display.h"
#include "../../src/server/subcompositor_interface.h"

class TestSubCompositor : public QObject
{
    Q_OBJECT
public:
    explicit TestSubCompositor(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testDestroy();
    void testCast();

private:
    Wrapland::Server::Display *m_display;
    Wrapland::Server::SubCompositorInterface *m_subcompositorInterface;
    Wrapland::Client::ConnectionThread *m_connection;
    Wrapland::Client::SubCompositor *m_subCompositor;
    Wrapland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-subcompositor-0");

TestSubCompositor::TestSubCompositor(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_subcompositorInterface(nullptr)
    , m_connection(nullptr)
    , m_subCompositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestSubCompositor::init()
{
    using namespace Wrapland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new Wrapland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Wrapland::Client::Registry registry;
    QSignalSpy subCompositorSpy(&registry, SIGNAL(subCompositorAnnounced(quint32,quint32)));
    QVERIFY(subCompositorSpy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_subcompositorInterface = m_display->createSubCompositor(m_display);
    QVERIFY(m_subcompositorInterface);
    m_subcompositorInterface->create();
    QVERIFY(m_subcompositorInterface->isValid());

    QVERIFY(subCompositorSpy.wait());
    m_subCompositor = registry.createSubCompositor(subCompositorSpy.first().first().value<quint32>(), subCompositorSpy.first().last().value<quint32>(), this);
}

void TestSubCompositor::cleanup()
{
    if (m_subCompositor) {
        delete m_subCompositor;
        m_subCompositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
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

void TestSubCompositor::testDestroy()
{
    using namespace Wrapland::Client;
    connect(m_connection, &ConnectionThread::establishedChanged, m_subCompositor, &SubCompositor::release);
    connect(m_connection, &ConnectionThread::establishedChanged, m_queue, &EventQueue::release);
    QVERIFY(m_subCompositor->isValid());

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!m_connection->established());

    // Now the pool should be destroyed.
    QTRY_VERIFY(!m_subCompositor->isValid());

    // Calling release again should not fail.
    m_subCompositor->release();
}

void TestSubCompositor::testCast()
{
    using namespace Wrapland::Client;
    Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy subCompositorSpy(&registry, SIGNAL(subCompositorAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(subCompositorSpy.wait());

    SubCompositor c;
    auto wlSubComp = registry.bindSubCompositor(subCompositorSpy.first().first().value<quint32>(), subCompositorSpy.first().last().value<quint32>());
    c.setup(wlSubComp);
    QCOMPARE((wl_subcompositor*)c, wlSubComp);

    const SubCompositor &c2(c);
    QCOMPARE((wl_subcompositor*)c2, wlSubComp);
}

QTEST_GUILESS_MAIN(TestSubCompositor)
#include "test_wayland_subcompositor.moc"
