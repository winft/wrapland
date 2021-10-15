/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2015  Marco Martin <mart@kde.org>

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
#include "../../src/client/contrast.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/contrast.h"
#include "../../server/display.h"
#include "../../server/region.h"
#include "../../server/surface.h"

#include <wayland-util.h>

class TestContrast : public QObject
{
    Q_OBJECT
public:
    explicit TestContrast(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSurfaceDestroy();

private:
    Wrapland::Server::Display* m_display;
    Wrapland::Server::Compositor* m_serverCompositor;
    Wrapland::Server::ContrastManager* m_contrastManagerInterface;
    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::ContrastManager* m_contrastManager;
    Wrapland::Client::EventQueue* m_queue;
    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-wayland-contrast-0"};

TestContrast::TestContrast(QObject* parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverCompositor(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestContrast::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();
    m_display = new Wrapland::Server::Display(this);
    m_display->set_socket_name(socket_name);
    m_display->start();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
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

    QSignalSpy contrastSpy(&registry, &Wrapland::Client::Registry::contrastAnnounced);
    QVERIFY(contrastSpy.isValid());

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_serverCompositor = m_display->createCompositor(m_display);

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);

    m_contrastManagerInterface = m_display->createContrastManager(m_display);

    QVERIFY(contrastSpy.wait());
    m_contrastManager = registry.createContrastManager(contrastSpy.first().first().value<quint32>(),
                                                       contrastSpy.first().last().value<quint32>(),
                                                       this);
}

void TestContrast::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_contrastManager)
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
    CLEANUP(m_serverCompositor)
    CLEANUP(m_contrastManagerInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestContrast::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_serverCompositor,
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();

    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());

    auto* contrast = m_contrastManager->createContrast(surface.get(), surface.get());
    contrast->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), contrast));

    contrast->setContrast(0.2);
    contrast->setIntensity(2.0);
    contrast->setSaturation(1.7);

    contrast->commit();
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::contrast);
    QCOMPARE(serverSurface->state().contrast->region(), QRegion(0, 0, 10, 20));
    QCOMPARE(wl_fixed_from_double(serverSurface->state().contrast->contrast()),
             wl_fixed_from_double(0.2));
    QCOMPARE(wl_fixed_from_double(serverSurface->state().contrast->intensity()),
             wl_fixed_from_double(2.0));
    QCOMPARE(wl_fixed_from_double(serverSurface->state().contrast->saturation()),
             wl_fixed_from_double(1.7));

    // And destroy.
    QSignalSpy destroyedSpy(serverSurface->state().contrast.data(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete contrast;
    QVERIFY(destroyedSpy.wait());
}

void TestContrast::testSurfaceDestroy()
{
    QSignalSpy serverSurfaceCreated(m_serverCompositor,
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());

    std::unique_ptr<Wrapland::Client::Contrast> contrast(
        m_contrastManager->createContrast(surface.get()));
    contrast->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), contrast.get()));
    contrast->commit();
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::contrast);
    QCOMPARE(serverSurface->state().contrast->region(), QRegion(0, 0, 10, 20));

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());
    QSignalSpy contrastDestroyedSpy(serverSurface->state().contrast.data(), &QObject::destroyed);
    QVERIFY(contrastDestroyedSpy.isValid());
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(contrastDestroyedSpy.isEmpty());
    // destroy the blur
    contrast.reset();
    QVERIFY(contrastDestroyedSpy.wait());
}

QTEST_GUILESS_MAIN(TestContrast)
#include "contrast.moc"
