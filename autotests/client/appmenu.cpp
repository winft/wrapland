/********************************************************************
Copyright 2017  David Edmundson <davidedmundson@kde.org>
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
#include <QtTest>

#include "../../src/client/appmenu.h"
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"

#include "../../server/appmenu.h"
#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/region.h"
#include "../../server/surface.h"

Q_DECLARE_METATYPE(Wrapland::Server::Appmenu::InterfaceAddress)

class TestAppmenu : public QObject
{
    Q_OBJECT
public:
    explicit TestAppmenu(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateAndSet();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::AppMenuManager* m_appmenuManager;
    Wrapland::Client::EventQueue* m_queue;
    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-wayland-appmenu-0"};

TestAppmenu::TestAppmenu(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestAppmenu::init()
{
    qRegisterMetaType<Wrapland::Server::Appmenu::InterfaceAddress>();
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

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

    Wrapland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, &Wrapland::Client::Registry::compositorAnnounced);
    QVERIFY(compositorSpy.isValid());

    QSignalSpy appmenuSpy(&registry, &Wrapland::Client::Registry::appMenuAnnounced);
    QVERIFY(appmenuSpy.isValid());

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    server.globals.compositor = server.display->createCompositor();

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);

    server.globals.appmenu_manager = server.display->createAppmenuManager();

    QVERIFY(appmenuSpy.wait());
    m_appmenuManager = registry.createAppMenuManager(appmenuSpy.first().first().value<quint32>(),
                                                     appmenuSpy.first().last().value<quint32>(),
                                                     this);
}

void TestAppmenu::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_appmenuManager)
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
#undef CLEANUP
    server = {};
}

void TestAppmenu::testCreateAndSet()
{
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QSignalSpy appmenuCreated(server.globals.appmenu_manager.get(),
                              &Wrapland::Server::AppmenuManager::appmenuCreated);

    QVERIFY(!server.globals.appmenu_manager->appmenuForSurface(serverSurface));

    auto appmenu = m_appmenuManager->create(surface.get(), surface.get());
    QVERIFY(appmenuCreated.wait());
    auto serverAppmenu = appmenuCreated.first().first().value<Wrapland::Server::Appmenu*>();
    QCOMPARE(server.globals.appmenu_manager->appmenuForSurface(serverSurface), serverAppmenu);

    QCOMPARE(serverAppmenu->address().serviceName, QString());
    QCOMPARE(serverAppmenu->address().objectPath, QString());

    QSignalSpy appMenuChangedSpy(serverAppmenu, &Wrapland::Server::Appmenu::addressChanged);

    appmenu->setAddress("net.somename", "/test/path");

    QVERIFY(appMenuChangedSpy.wait());
    QCOMPARE(serverAppmenu->address().serviceName, QString("net.somename"));
    QCOMPARE(serverAppmenu->address().objectPath, QString("/test/path"));

    // and destroy
    QSignalSpy destroyedSpy(serverAppmenu, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete appmenu;
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!server.globals.appmenu_manager->appmenuForSurface(serverSurface));
}

QTEST_GUILESS_MAIN(TestAppmenu)
#include "appmenu.moc"
