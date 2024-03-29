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
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/slide.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/region.h"
#include "../../server/slide.h"
#include "../../server/surface.h"

#include "../../tests/globals.h"

using namespace Wrapland::Client;

class TestSlide : public QObject
{
    Q_OBJECT
public:
    explicit TestSlide(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSurfaceDestroy();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::SlideManager* m_slideManager;
    Wrapland::Client::EventQueue* m_queue;
    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-wayland-slide-0"};

TestSlide::TestSlide(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestSlide::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    // Setup connection.
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

    Registry registry;
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QVERIFY(compositorSpy.isValid());

    QSignalSpy slideSpy(&registry, &Registry::slideAnnounced);
    QVERIFY(slideSpy.isValid());

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);

    server.globals.slide_manager
        = std::make_unique<Wrapland::Server::SlideManager>(server.display.get());

    QVERIFY(slideSpy.wait());
    m_slideManager = registry.createSlideManager(
        slideSpy.first().first().value<quint32>(), slideSpy.first().last().value<quint32>(), this);
}

void TestSlide::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_slideManager)
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

void TestSlide::testCreate()
{
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);

    auto slide = m_slideManager->createSlide(surface.get(), surface.get());
    slide->setLocation(Wrapland::Client::Slide::Location::Top);
    slide->setOffset(15);
    slide->commit();
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(commit_spy.wait());
    QCOMPARE(serverSurface->state().slide->location(), Wrapland::Server::Slide::Location::Top);
    QCOMPARE(serverSurface->state().slide->offset(), 15);

    // and destroy
    QSignalSpy destroyedSpy(serverSurface->state().slide, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete slide;
    QVERIFY(destroyedSpy.wait());
}

void TestSlide::testSurfaceDestroy()
{
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());

    std::unique_ptr<Slide> slide(m_slideManager->createSlide(surface.get()));
    slide->commit();
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    auto serverSlide = serverSurface->state().slide;
    QVERIFY(serverSlide);

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());
    QSignalSpy slideDestroyedSpy(serverSlide, &QObject::destroyed);
    QVERIFY(slideDestroyedSpy.isValid());
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(slideDestroyedSpy.isEmpty());
    // destroy the slide
    slide.reset();
    QVERIFY(slideDestroyedSpy.wait());
}

QTEST_GUILESS_MAIN(TestSlide)
#include "slide.moc"
