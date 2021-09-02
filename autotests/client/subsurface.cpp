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
#include "../../src/client/shm_pool.h"
#include "../../src/client/subcompositor.h"
#include "../../src/client/subsurface.h"
#include "../../src/client/surface.h"

#include "../../server/buffer.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/subcompositor.h"
#include "../../server/surface.h"

#include "../../tests/helpers.h"

#include <wayland-client.h>

class TestSubsurface : public QObject
{
    Q_OBJECT
public:
    explicit TestSubsurface(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testMode();
    void testPosition();
    void testPlaceAbove();
    void testPlaceBelow();
    void testDestroy();
    void testCast();
    void testSyncMode();
    void testDeSyncMode();
    void testMainSurfaceFromTree();
    void testRemoveSurface();
    void testMappingOfSurfaceTree();
    void testSurfaceAt();
    void testDestroyAttachedBuffer();
    void testDestroyParentSurface();

private:
    Wrapland::Server::Display* m_display;
    Wrapland::Server::Compositor* m_serverCompositor;
    Wrapland::Server::Subcompositor* m_serverSubcompositor;
    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::ShmPool* m_shm;
    Wrapland::Client::SubCompositor* m_subCompositor;
    Wrapland::Client::EventQueue* m_queue;
    QThread* m_thread;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-subsurface-0");

TestSubsurface::TestSubsurface(QObject* parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverCompositor(nullptr)
    , m_serverSubcompositor(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_shm(nullptr)
    , m_subCompositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestSubsurface::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    m_display = new Wrapland::Server::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_display->createShm();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(s_socketName);

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
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32, quint32)));
    QVERIFY(compositorSpy.isValid());
    QSignalSpy subCompositorSpy(&registry, SIGNAL(subCompositorAnnounced(quint32, quint32)));
    QVERIFY(subCompositorSpy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_serverCompositor = m_display->createCompositor(m_display);

    m_serverSubcompositor = m_display->createSubCompositor(m_display);
    QVERIFY(m_serverSubcompositor);

    QVERIFY(subCompositorSpy.wait());
    m_subCompositor
        = registry.createSubCompositor(subCompositorSpy.first().first().value<quint32>(),
                                       subCompositorSpy.first().last().value<quint32>(),
                                       this);

    if (compositorSpy.isEmpty()) {
        QVERIFY(compositorSpy.wait());
    }
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);

    m_shm = registry.createShmPool(
        registry.interface(Wrapland::Client::Registry::Interface::Shm).name,
        registry.interface(Wrapland::Client::Registry::Interface::Shm).version,
        this);
    QVERIFY(m_shm->isValid());
}

void TestSubsurface::cleanup()
{
    if (m_shm) {
        delete m_shm;
        m_shm = nullptr;
    }
    if (m_subCompositor) {
        delete m_subCompositor;
        m_subCompositor = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
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

void TestSubsurface::testCreate()
{
    QSignalSpy surfaceCreatedSpy(m_serverCompositor,
                                 SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    // create two Surfaces
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    surfaceCreatedSpy.clear();
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverParentSurface
        = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverParentSurface);

    QSignalSpy subsurfaceCreatedSpy(m_serverSubcompositor,
                                    SIGNAL(subsurfaceCreated(Wrapland::Server::Subsurface*)));
    QVERIFY(subsurfaceCreatedSpy.isValid());

    // create subsurface for surface of parent
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));

    QVERIFY(subsurfaceCreatedSpy.wait());

    auto serverSubsurface
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface);
    QVERIFY(serverSubsurface->parentSurface());
    QCOMPARE(serverSubsurface->parentSurface(), serverParentSurface);
    QCOMPARE(serverSubsurface->surface(), serverSurface);
    QCOMPARE(serverSurface->subsurface(), serverSubsurface);
    QCOMPARE(serverSubsurface->mainSurface(), serverParentSurface);

    // children are only added after committing the surface
    QCOMPARE(serverParentSurface->childSubsurfaces().size(), 0);

    // so let's commit the surface, to apply the stacking change
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());

    QCoreApplication::processEvents();
    QCOMPARE(serverParentSurface->childSubsurfaces().size(), 1);
    QCOMPARE(serverParentSurface->childSubsurfaces().front(), serverSubsurface);

    // and let's destroy it again
    QSignalSpy destroyedSpy(serverSubsurface, SIGNAL(destroyed(QObject*)));
    QVERIFY(destroyedSpy.isValid());
    subsurface.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(serverSurface->subsurface(), QPointer<Wrapland::Server::Subsurface>());

    // Applied immediately.
    QCOMPARE(serverParentSurface->childSubsurfaces().size(), 0);

    // Make sure committing on parent still works.
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverParentSurface->childSubsurfaces().size(), 0);
}

void TestSubsurface::testMode()
{
    // create two Surface
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());

    QSignalSpy subsurfaceCreatedSpy(m_serverSubcompositor,
                                    SIGNAL(subsurfaceCreated(Wrapland::Server::Subsurface*)));
    QVERIFY(subsurfaceCreatedSpy.isValid());

    // create the Subsurface for surface of parent
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceCreatedSpy.wait());
    auto serverSubsurface
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface);

    // both client and server subsurface should be in synchronized mode
    QCOMPARE(subsurface->mode(), Wrapland::Client::SubSurface::Mode::Synchronized);
    QCOMPARE(serverSubsurface->mode(), Wrapland::Server::Subsurface::Mode::Synchronized);

    // verify that we can change to desynchronized
    QSignalSpy modeChangedSpy(serverSubsurface,
                              SIGNAL(modeChanged(Wrapland::Server::Subsurface::Mode)));
    QVERIFY(modeChangedSpy.isValid());

    subsurface->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);
    QCOMPARE(subsurface->mode(), Wrapland::Client::SubSurface::Mode::Desynchronized);

    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.first().first().value<Wrapland::Server::Subsurface::Mode>(),
             Wrapland::Server::Subsurface::Mode::Desynchronized);
    QCOMPARE(serverSubsurface->mode(), Wrapland::Server::Subsurface::Mode::Desynchronized);

    // setting the same again won't change
    subsurface->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);
    QCOMPARE(subsurface->mode(), Wrapland::Client::SubSurface::Mode::Desynchronized);
    // not testing the signal, we do that after changing to synchronized

    // and change back to synchronized
    subsurface->setMode(Wrapland::Client::SubSurface::Mode::Synchronized);
    QCOMPARE(subsurface->mode(), Wrapland::Client::SubSurface::Mode::Synchronized);

    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 2);
    QCOMPARE(modeChangedSpy.first().first().value<Wrapland::Server::Subsurface::Mode>(),
             Wrapland::Server::Subsurface::Mode::Desynchronized);
    QCOMPARE(modeChangedSpy.last().first().value<Wrapland::Server::Subsurface::Mode>(),
             Wrapland::Server::Subsurface::Mode::Synchronized);
    QCOMPARE(serverSubsurface->mode(), Wrapland::Server::Subsurface::Mode::Synchronized);
}

void TestSubsurface::testPosition()
{
    // create two Surface
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());

    QSignalSpy subsurfaceCreatedSpy(m_serverSubcompositor,
                                    SIGNAL(subsurfaceCreated(Wrapland::Server::Subsurface*)));
    QVERIFY(subsurfaceCreatedSpy.isValid());

    // create the Subsurface for surface of parent
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceCreatedSpy.wait());
    auto serverSubsurface
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface);

    // create a signalspy
    QSignalSpy subsurfaceTreeChanged(serverSubsurface->parentSurface(),
                                     &Wrapland::Server::Surface::subsurfaceTreeChanged);
    QVERIFY(subsurfaceTreeChanged.isValid());

    // both client and server should have a default position
    QCOMPARE(subsurface->position(), QPoint());
    QCOMPARE(serverSubsurface->position(), QPoint());

    QSignalSpy positionChangedSpy(serverSubsurface, SIGNAL(positionChanged(QPoint)));
    QVERIFY(positionChangedSpy.isValid());

    // changing the position should not trigger a direct update on server side
    subsurface->setPosition(QPoint(10, 20));
    QCOMPARE(subsurface->position(), QPoint(10, 20));
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface->position(), QPoint());
    // changing once more
    subsurface->setPosition(QPoint(20, 30));
    QCOMPARE(subsurface->position(), QPoint(20, 30));
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface->position(), QPoint());

    // committing the parent surface should update the position
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    QCOMPARE(subsurfaceTreeChanged.count(), 0);
    QVERIFY(positionChangedSpy.wait());
    QCOMPARE(positionChangedSpy.count(), 1);
    QCOMPARE(positionChangedSpy.first().first().toPoint(), QPoint(20, 30));
    QCOMPARE(serverSubsurface->position(), QPoint(20, 30));
    QCOMPARE(subsurfaceTreeChanged.count(), 1);
}

void TestSubsurface::testPlaceAbove()
{
    // Create needed Surfaces (one parent, three children).
    std::unique_ptr<Wrapland::Client::Surface> surface1(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> surface2(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> surface3(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());

    QSignalSpy subsurfaceCreatedSpy(m_serverSubcompositor,
                                    &Wrapland::Server::Subcompositor::subsurfaceCreated);
    QVERIFY(subsurfaceCreatedSpy.isValid());

    // Create the Subsurfaces for surface of parent.
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface1(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface1.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceCreatedSpy.wait());

    auto serverSubsurface1
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface1);

    subsurfaceCreatedSpy.clear();
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface2(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface2.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceCreatedSpy.wait());

    auto serverSubsurface2
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface2);

    subsurfaceCreatedSpy.clear();
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface3(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface3.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceCreatedSpy.wait());

    auto serverSubsurface3
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface3);
    subsurfaceCreatedSpy.clear();

    // So far the stacking order should still be empty.
    QVERIFY(serverSubsurface1->parentSurface()->childSubsurfaces().empty());

    // Committing the parent should create the stacking order.
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);

    // Ensure it's processed on server side.
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface1);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface3);

    // Raising subsurface1 should place it to top of stack.
    subsurface1->raise();

    // Ensure it's processed on server side.
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    // But as long as parent is not committed it shouldn't change on server side.
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface1);

    // After commit it's changed.
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface1);

    // Try placing 3 above 1, should result in 2, 1, 3.
    subsurface3->placeAbove(QPointer<Wrapland::Client::SubSurface>(subsurface1.get()));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface1);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface3);

    // try placing 3 above 2, should result in 2, 3, 1
    subsurface3->placeAbove(QPointer<Wrapland::Client::SubSurface>(subsurface2.get()));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface1);

    // try placing 1 above 3 - shouldn't change
    subsurface1->placeAbove(QPointer<Wrapland::Client::SubSurface>(subsurface3.get()));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface1);

    // and 2 above 3 - > 3, 2, 1
    subsurface2->placeAbove(QPointer<Wrapland::Client::SubSurface>(subsurface3.get()));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface1);
}

void TestSubsurface::testPlaceBelow()
{
    using namespace Wrapland::Server;
    // create needed Surfaces (one parent, three client
    std::unique_ptr<Wrapland::Client::Surface> surface1(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> surface2(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> surface3(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());

    QSignalSpy subsurfaceCreatedSpy(m_serverSubcompositor,
                                    SIGNAL(subsurfaceCreated(Wrapland::Server::Subsurface*)));
    QVERIFY(subsurfaceCreatedSpy.isValid());

    // create the Subsurfaces for surface of parent
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface1(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface1.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceCreatedSpy.wait());
    auto serverSubsurface1
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface1);
    subsurfaceCreatedSpy.clear();
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface2(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface2.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceCreatedSpy.wait());
    auto serverSubsurface2
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface2);
    subsurfaceCreatedSpy.clear();
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface3(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface3.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceCreatedSpy.wait());
    auto serverSubsurface3
        = subsurfaceCreatedSpy.first().first().value<Wrapland::Server::Subsurface*>();
    QVERIFY(serverSubsurface3);
    subsurfaceCreatedSpy.clear();

    // so far the stacking order should still be empty
    QVERIFY(serverSubsurface1->parentSurface()->childSubsurfaces().empty());

    // committing the parent should create the stacking order
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface1);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface3);

    // lowering subsurface3 should place it to the bottom of stack
    subsurface3->lower();
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    // but as long as parent is not committed it shouldn't change on server side
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface1);
    // after commit it's changed
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface1);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface2);

    // place 1 below 3 -> 1, 3, 2
    subsurface1->placeBelow(QPointer<Wrapland::Client::SubSurface>(subsurface3.get()));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface1);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface2);

    // 2 below 3 -> 1, 2, 3
    subsurface2->placeBelow(QPointer<Wrapland::Client::SubSurface>(subsurface3.get()));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface1);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface3);

    // 1 below 2 -> shouldn't change
    subsurface1->placeBelow(QPointer<Wrapland::Client::SubSurface>(subsurface2.get()));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface1);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface2);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface3);

    // and 3 below 1 -> 3, 1, 2
    subsurface3->placeBelow(QPointer<Wrapland::Client::SubSurface>(subsurface1.get()));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().size(), 3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(0), serverSubsurface3);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(1), serverSubsurface1);
    QCOMPARE(serverSubsurface1->parentSurface()->childSubsurfaces().at(2), serverSubsurface2);
}

void TestSubsurface::testDestroy()
{
    // create two Surfaces
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());
    // create subsurface for surface of parent
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));

    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_compositor,
            &Wrapland::Client::Compositor::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_subCompositor,
            &Wrapland::Client::SubCompositor::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_shm,
            &Wrapland::Client::ShmPool::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_queue,
            &Wrapland::Client::EventQueue::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            surface.get(),
            &Wrapland::Client::Surface::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            parent.get(),
            &Wrapland::Client::Surface::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            subsurface.get(),
            &Wrapland::Client::SubSurface::release);
    QVERIFY(subsurface->isValid());

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!m_connection->established());

    // Now the pool should be destroyed.
    QTRY_VERIFY(!subsurface->isValid());

    // Calling destroy again should not fail.
    subsurface->release();
}

void TestSubsurface::testCast()
{
    // Create two Surfaces.
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());

    // Create subsurface for surface with parent.
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));

    QCOMPARE(Wrapland::Client::SubSurface::get(*(subsurface.get())),
             QPointer<Wrapland::Client::SubSurface>(subsurface.get()));
}

void TestSubsurface::testSyncMode()
{
    // this test verifies that state is only applied when the parent surface commits its pending
    // state

    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childSurface);

    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentSurface = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(parentSurface);
    QSignalSpy subsurfaceTreeChangedSpy(parentSurface,
                                        &Wrapland::Server::Surface::subsurfaceTreeChanged);
    QVERIFY(subsurfaceTreeChangedSpy.isValid());
    // create subsurface for surface of parent
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceTreeChangedSpy.wait());
    QCOMPARE(subsurfaceTreeChangedSpy.count(), 1);

    // let's damage the child surface
    QSignalSpy child_commit_spy(childSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(child_commit_spy.isValid());

    QImage image(QSize(200, 200), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 200, 200));
    surface->commit();

    // state should be applied when the parent surface's state gets applied
    QVERIFY(!child_commit_spy.wait(100));
    QVERIFY(!childSurface->buffer());

    QVERIFY(!childSurface->isMapped());
    QVERIFY(!parentSurface->isMapped());

    QImage image2(QSize(400, 400), QImage::Format_ARGB32_Premultiplied);
    image2.fill(Qt::red);
    parent->attachBuffer(m_shm->createBuffer(image2));
    parent->damage(QRect(0, 0, 400, 400));
    parent->commit();

    QVERIFY(child_commit_spy.wait());
    QCOMPARE(child_commit_spy.count(), 1);
    QCOMPARE(subsurfaceTreeChangedSpy.count(), 2);
    QCOMPARE(childSurface->buffer()->shmImage()->createQImage(), image);
    QCOMPARE(parentSurface->buffer()->shmImage()->createQImage(), image2);
    QVERIFY(childSurface->isMapped());
    QVERIFY(parentSurface->isMapped());

    // sending frame rendered to parent should also send it to child
    QSignalSpy frameRenderedSpy(surface.get(), &Wrapland::Client::Surface::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    parentSurface->frameRendered(100);
    QVERIFY(frameRenderedSpy.wait());
}

void TestSubsurface::testDeSyncMode()
{
    // this test verifies that state gets applied immediately in desync mode

    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childSurface);

    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentSurface = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(parentSurface);
    QSignalSpy subsurfaceTreeChangedSpy(parentSurface,
                                        &Wrapland::Server::Surface::subsurfaceTreeChanged);
    QVERIFY(subsurfaceTreeChangedSpy.isValid());
    // create subsurface for surface of parent
    std::unique_ptr<Wrapland::Client::SubSurface> subsurface(
        m_subCompositor->createSubSurface(QPointer<Wrapland::Client::Surface>(surface.get()),
                                          QPointer<Wrapland::Client::Surface>(parent.get())));
    QVERIFY(subsurfaceTreeChangedSpy.wait());
    QCOMPARE(subsurfaceTreeChangedSpy.count(), 1);

    // let's damage the child surface
    QSignalSpy child_commit_spy(childSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(child_commit_spy.isValid());

    QImage image(QSize(200, 200), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 200, 200));
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    // state should be applied when the parent surface's state gets applied or when the subsurface
    // switches to desync
    QVERIFY(!child_commit_spy.wait(100));
    QVERIFY(!childSurface->isMapped());
    QVERIFY(!parentSurface->isMapped());

    // setting to desync should apply the state directly
    QVERIFY(child_commit_spy.isEmpty());
    subsurface->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);

    QVERIFY(child_commit_spy.count() || child_commit_spy.wait());
    QCOMPARE(child_commit_spy.count(), 1);
    QCOMPARE(subsurfaceTreeChangedSpy.count(), 1);
    QCOMPARE(childSurface->buffer()->shmImage()->createQImage(), image);
    QVERIFY(!childSurface->isMapped());
    QVERIFY(!parentSurface->isMapped());

    // and damaging again, should directly be applied
    image.fill(Qt::red);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 200, 200));
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(child_commit_spy.wait());
    QCOMPARE(child_commit_spy.count(), 2);
    QCOMPARE(subsurfaceTreeChangedSpy.count(), 1);
    QCOMPARE(childSurface->buffer()->shmImage()->createQImage(), image);
}

void TestSubsurface::testMainSurfaceFromTree()
{
    // this test verifies that in a tree of surfaces every surface has the same main surface
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto parentServerSurface = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(parentServerSurface);
    std::unique_ptr<Wrapland::Client::Surface> childLevel1Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto childLevel1ServerSurface
        = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childLevel1ServerSurface);
    std::unique_ptr<Wrapland::Client::Surface> childLevel2Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto childLevel2ServerSurface
        = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childLevel2ServerSurface);
    std::unique_ptr<Wrapland::Client::Surface> childLevel3Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto childLevel3ServerSurface
        = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childLevel3ServerSurface);

    QSignalSpy subsurfaceTreeChangedSpy(parentServerSurface,
                                        &Wrapland::Server::Surface::subsurfaceTreeChanged);
    QVERIFY(subsurfaceTreeChangedSpy.isValid());

    auto* sub1 = m_subCompositor->createSubSurface(childLevel1Surface.get(), parentSurface.get());
    auto* sub2
        = m_subCompositor->createSubSurface(childLevel2Surface.get(), childLevel1Surface.get());
    auto* sub3
        = m_subCompositor->createSubSurface(childLevel3Surface.get(), childLevel2Surface.get());

    childLevel2Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    childLevel1Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    parentSurface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(subsurfaceTreeChangedSpy.wait());

    QCOMPARE(parentServerSurface->childSubsurfaces().size(), 1);

    auto child = parentServerSurface->childSubsurfaces().front();
    QCOMPARE(child->parentSurface(), parentServerSurface);
    QCOMPARE(child->mainSurface(), parentServerSurface);
    QCOMPARE(child->surface()->childSubsurfaces().size(), 1);

    auto child2 = child->surface()->childSubsurfaces().front();
    QCOMPARE(child2->parentSurface(), child->surface());
    QCOMPARE(child2->mainSurface(), parentServerSurface);
    QCOMPARE(child2->surface()->childSubsurfaces().size(), 1);

    auto child3 = child2->surface()->childSubsurfaces().front();
    QCOMPARE(child3->parentSurface(), child2->surface());
    QCOMPARE(child3->mainSurface(), parentServerSurface);
    QCOMPARE(child3->surface()->childSubsurfaces().size(), 0);

    delete sub1;
    delete sub2;
    delete sub3;
}

void TestSubsurface::testRemoveSurface()
{
    // this test verifies that removing the surface also removes the sub-surface from the parent
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;

    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentServerSurface = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(parentServerSurface);
    std::unique_ptr<Wrapland::Client::Surface> childSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childServerSurface = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childServerSurface);

    QSignalSpy subsurfaceTreeChangedSpy(parentServerSurface,
                                        &Wrapland::Server::Surface::subsurfaceTreeChanged);
    QVERIFY(subsurfaceTreeChangedSpy.isValid());

    std::unique_ptr<Wrapland::Client::SubSurface> sub(
        m_subCompositor->createSubSurface(childSurface.get(), parentSurface.get()));
    parentSurface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(subsurfaceTreeChangedSpy.wait());

    QCOMPARE(parentServerSurface->childSubsurfaces().size(), 1);

    // destroy surface, takes place immediately
    childSurface.reset();
    QVERIFY(subsurfaceTreeChangedSpy.wait());
    QCOMPARE(parentServerSurface->childSubsurfaces().size(), 0);
}

void TestSubsurface::testMappingOfSurfaceTree()
{
    // this test verifies mapping and unmapping of a sub-surface tree
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentServerSurface = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(parentServerSurface);
    std::unique_ptr<Wrapland::Client::Surface> childLevel1Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel1ServerSurface
        = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childLevel1ServerSurface);
    std::unique_ptr<Wrapland::Client::Surface> childLevel2Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel2ServerSurface
        = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childLevel2ServerSurface);
    std::unique_ptr<Wrapland::Client::Surface> childLevel3Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel3ServerSurface
        = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(childLevel3ServerSurface);

    QSignalSpy subsurfaceTreeChangedSpy(parentServerSurface,
                                        &Wrapland::Server::Surface::subsurfaceTreeChanged);
    QVERIFY(subsurfaceTreeChangedSpy.isValid());

    auto subsurfaceLevel1
        = m_subCompositor->createSubSurface(childLevel1Surface.get(), parentSurface.get());
    auto subsurfaceLevel2
        = m_subCompositor->createSubSurface(childLevel2Surface.get(), childLevel1Surface.get());
    auto subsurfaceLevel3
        = m_subCompositor->createSubSurface(childLevel3Surface.get(), childLevel2Surface.get());

    childLevel3Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    childLevel2Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    childLevel1Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    parentSurface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(subsurfaceTreeChangedSpy.wait());

    QCOMPARE(parentServerSurface->childSubsurfaces().size(), 1);
    auto child = parentServerSurface->childSubsurfaces().front();
    QCOMPARE(child->surface()->childSubsurfaces().size(), 1);
    auto child2 = child->surface()->childSubsurfaces().front();
    QCOMPARE(child2->surface()->childSubsurfaces().size(), 1);
    auto child3 = child2->surface()->childSubsurfaces().front();
    QCOMPARE(child3->parentSurface(), child2->surface());
    QCOMPARE(child3->mainSurface(), parentServerSurface);
    QCOMPARE(child3->surface()->childSubsurfaces().size(), 0);

    // So far no surface is mapped.
    QVERIFY(!parentServerSurface->isMapped());
    QVERIFY(!child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());

    // First set all subsurfaces to desync to simplify.
    subsurfaceLevel1->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);
    subsurfaceLevel2->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);
    subsurfaceLevel3->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);

    QImage image(QSize(200, 200), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);

    // Attach a buffer to the first child. Should not map.
    QSignalSpy child1_commit_spy(child->surface(), &Wrapland::Server::Surface::committed);
    QVERIFY(child1_commit_spy.isValid());
    childLevel1Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel1Surface->damage(QRect(0, 0, 200, 200));
    childLevel1Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(child1_commit_spy.wait());
    QVERIFY(child->surface()->buffer());
    QVERIFY(!child->surface()->isMapped());

    // Attach a buffer to the third child. Should not map.
    QSignalSpy child3_commit_spy(child3->surface(), &Wrapland::Server::Surface::committed);
    QVERIFY(child3_commit_spy.isValid());
    childLevel3Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel3Surface->damage(QRect(0, 0, 200, 200));
    childLevel3Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(child3_commit_spy.wait());
    QVERIFY(child3->surface()->buffer());
    QVERIFY(!child3->surface()->isMapped());

    // Map the top level.
    QSignalSpy commit_spy(parentServerSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());
    parentSurface->attachBuffer(m_shm->createBuffer(image));
    parentSurface->damage(QRect(0, 0, 200, 200));
    parentSurface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(parentServerSurface->isMapped());

    // First child should now be mapped automatically too but not second or third one.
    QVERIFY(child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());

    // Now map the second level. This should automatically also map the thrid one.
    QSignalSpy child2_commit_spy(child2->surface(), &Wrapland::Server::Surface::committed);
    QVERIFY(child2_commit_spy.isValid());
    childLevel2Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel2Surface->damage(QRect(0, 0, 200, 200));
    childLevel2Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(child2_commit_spy.wait());
    QVERIFY(parentServerSurface->isMapped());

    // Everything is mapped now.
    QVERIFY(parentServerSurface->isMapped());
    QVERIFY(child->surface()->isMapped());
    QVERIFY(child2->surface()->isMapped());
    QVERIFY(child3->surface()->isMapped());

    // Unmapping a parent should unmap the complete tree.
    QSignalSpy unmappedSpy(child2->surface(), &Wrapland::Server::Surface::unmapped);
    QVERIFY(unmappedSpy.isValid());
    childLevel2Surface->attachBuffer(Wrapland::Client::Buffer::Ptr());
    childLevel2Surface->damage(QRect(0, 0, 200, 200));
    childLevel2Surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(unmappedSpy.wait());

    QVERIFY(parentServerSurface->isMapped());
    QVERIFY(child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());

    delete subsurfaceLevel1;
    delete subsurfaceLevel2;
    delete subsurfaceLevel3;
}

void TestSubsurface::testSurfaceAt()
{
    // This test verifies that the correct surface is picked in a subsurface tree.
    //
    //     -----------------------------
    //    |                             |
    //    |      grandchild1(50x50)     |
    //    |                             |
    //    |                             |
    //    |                             |-----------------------------------------
    //    |                             |                          |              |
    //    |                             |                          |              |
    //    |                             |                          |              |
    //    |                             |                          |              |
    //     -----------------------------                           |              |
    //                   |                                         |              |
    //                   |              child1(75x75)              |              |
    //                   |                                         |              |
    //                   |                                         |              |
    //                   |                             ------------------------------------------
    //                   |                            |                                          |
    //                   |                            |              child2(75x75)               |
    //                   |                            |                                          |
    //                   |----------------------------|             -----------------------------|
    //                   |                            |            |              |              |
    //                   |                            |            |    input1    | grandhchild2 |
    //                   |       parent(100x100)      |            |    (25x25)   |   (50x50)    |
    //                   |                            |            |              |              |
    //                    ----------------------------|            |--------------+--------------|
    //                                                |            |              |              |
    //                                                |            |              |    input2    |
    //                                                |            |              |    (25x25)   |
    //                                                |            |              |              |
    //                                                 ------------------------------------------
    //

    QImage parentimage(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    parentimage.fill(Qt::red);
    QImage childImage(QSize(75, 75), QImage::Format_ARGB32_Premultiplied);
    childImage.fill(Qt::green);
    QImage grandchildImage(QSize(50, 50), QImage::Format_ARGB32_Premultiplied);
    childImage.fill(Qt::blue);

    // First create a parent surface and map it.
    QSignalSpy serverSurfaceCreated(m_serverCompositor,
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());
    parent->attachBuffer(m_shm->createBuffer(parentimage));
    parent->damage(QRect(0, 0, 100, 100));
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceCreated.wait());
    auto serverParent = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();

    // Create two child subsurfaces, just added to the parent this is to simulate the behavior of
    // QtWayland (might have changed in the meantime).
    std::unique_ptr<Wrapland::Client::Surface> child1(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverChild1 = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverChild1);

    std::unique_ptr<Wrapland::Client::Surface> child2(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverChild2 = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverChild2);

    // Create the subsurfaces for the children.
    std::unique_ptr<Wrapland::Client::SubSurface> child1Sub(
        m_subCompositor->createSubSurface(child1.get(), parent.get()));
    child1Sub->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);

    child1->attachBuffer(m_shm->createBuffer(childImage));
    child1->damage(QRect(0, 0, 75, 75));
    child1->commit(Wrapland::Client::Surface::CommitFlag::None);

    // Note: child2Sub is initially above child1Sub since it is added after.
    std::unique_ptr<Wrapland::Client::SubSurface> child2Sub(
        m_subCompositor->createSubSurface(child2.get(), parent.get()));
    child2Sub->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);
    child2Sub->setPosition(QPoint(50, 50));

    child2->attachBuffer(m_shm->createBuffer(childImage));
    child2->damage(QRect(0, 0, 75, 75));
    child2->commit(Wrapland::Client::Surface::CommitFlag::None);

    // Each of the children gets a child.
    std::unique_ptr<Wrapland::Client::Surface> grandchild1(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverGrandchild1
        = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();

    std::unique_ptr<Wrapland::Client::Surface> grandchild2(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverGrandchild2
        = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();

    // Create subsurfaces for the grandchildren.
    std::unique_ptr<Wrapland::Client::SubSurface> grandchild1Sub(
        m_subCompositor->createSubSurface(grandchild1.get(), child1.get()));
    grandchild1Sub->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);
    grandchild1Sub->setPosition(QPoint(-25, -25));

    std::unique_ptr<Wrapland::Client::SubSurface> grandchild2Sub(
        m_subCompositor->createSubSurface(grandchild2.get(), child2.get()));
    grandchild2Sub->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);
    grandchild2Sub->setPosition(QPoint(25, 25));

    // Now let's render both grandchildren.
    grandchild1->attachBuffer(m_shm->createBuffer(grandchildImage));
    grandchild1->damage(QRect(0, 0, 50, 50));
    grandchild1->commit(Wrapland::Client::Surface::CommitFlag::None);

    // Second grandchild's input region is subdivided into quadrants, with input mask on the top
    // left and bottom right.
    QRegion region;
    region += QRect(0, 0, 25, 25);
    region += QRect(25, 25, 25, 25);
    grandchild2->setInputRegion(m_compositor->createRegion(region).get());

    grandchild2->attachBuffer(m_shm->createBuffer(grandchildImage));
    grandchild2->damage(QRect(0, 0, 50, 50));
    grandchild2->commit(Wrapland::Client::Surface::CommitFlag::None);

    QSignalSpy treeChangedSpy(serverParent, &Wrapland::Server::Surface::subsurfaceTreeChanged);
    QVERIFY(treeChangedSpy.isValid());
    QVERIFY(treeChangedSpy.wait());

    QCOMPARE(serverChild1->subsurface()->parentSurface(), serverParent);
    QCOMPARE(serverChild2->subsurface()->parentSurface(), serverParent);
    QCOMPARE(serverGrandchild1->subsurface()->parentSurface(), serverChild1);
    QCOMPARE(serverGrandchild2->subsurface()->parentSurface(), serverChild2);

    QVERIFY(serverChild1->isMapped());
    QVERIFY(serverChild2->isMapped());
    QVERIFY(serverGrandchild1->isMapped());
    QVERIFY(serverGrandchild2->isMapped());

    QSignalSpy commit_spy(serverParent, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());
    parent->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    namespace WST = Wrapland::Server::Test;

    // Now test some positions.
    // Around top-left.
    QVERIFY(!WST::surface_at(serverParent, QPointF(-26, -25)));
    QVERIFY(!WST::surface_at(serverParent, QPointF(-25, -26)));
    QCOMPARE(WST::surface_at(serverParent, QPointF(-25, -25)), serverGrandchild1);
    QCOMPARE(WST::surface_at(serverParent, QPointF(0, 0)), serverGrandchild1);

    // Around middle.
    QCOMPARE(WST::surface_at(serverParent, QPointF(49, 49)), serverChild1);
    QCOMPARE(WST::surface_at(serverParent, QPointF(49, 75)), serverChild1);
    QCOMPARE(WST::surface_at(serverParent, QPointF(75, 49)), serverChild1);
    QCOMPARE(WST::surface_at(serverParent, QPointF(76, 49)), serverParent);
    QCOMPARE(WST::surface_at(serverParent, QPointF(49, 76)), serverParent);
    QCOMPARE(WST::surface_at(serverParent, QPointF(49, 100)), serverParent);
    QCOMPARE(WST::surface_at(serverParent, QPointF(100, 49)), serverParent);
    QCOMPARE(WST::surface_at(serverParent, QPointF(50, 50)), serverChild2);
    QCOMPARE(WST::surface_at(serverParent, QPointF(100, 50)), serverChild2);
    QCOMPARE(WST::surface_at(serverParent, QPointF(50, 100)), serverChild2);
    QCOMPARE(WST::surface_at(serverParent, QPointF(75, 75)), serverGrandchild2);

    // Around bottom-right.
    QCOMPARE(WST::surface_at(serverParent, QPointF(100, 100)), serverGrandchild2);
    QCOMPARE(WST::surface_at(serverParent, QPointF(125, 100)), serverGrandchild2);
    QCOMPARE(WST::surface_at(serverParent, QPointF(100, 125)), serverGrandchild2);
    QVERIFY(!WST::surface_at(serverParent, QPointF(126, 125)));
    QVERIFY(!WST::surface_at(serverParent, QPointF(125, 126)));

    // Now test some input positions.
    // Around top-left.
    QVERIFY(!WST::input_surface_at(serverParent, QPointF(-26, -25)));
    QVERIFY(!WST::input_surface_at(serverParent, QPointF(-25, -26)));
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(-25, -25)), serverGrandchild1);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(0, 0)), serverGrandchild1);

    // Around middle.
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(49, 49)), serverChild1);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(49, 75)), serverChild1);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(75, 49)), serverChild1);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(76, 49)), serverParent);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(49, 76)), serverParent);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(49, 100)), serverParent);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(100, 49)), serverParent);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(50, 50)), serverChild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(100, 50)), serverChild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(50, 100)), serverChild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(75, 75)), serverGrandchild2);

    // Around bottom-right.
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(99, 99)), serverGrandchild2);

    // In Qt QRegions do not contain the right and bottom edge.
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(99, 100)), serverChild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(100, 99)), serverChild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(125, 100)), serverChild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(100, 125)), serverChild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(125, 125)), serverChild2);

    QCOMPARE(WST::input_surface_at(serverParent, QPointF(99, 101)), serverChild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(101, 99)), serverChild2);

    QCOMPARE(WST::input_surface_at(serverParent, QPointF(75, 75)), serverGrandchild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(100, 100)), serverGrandchild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(101, 100)), serverGrandchild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(100, 101)), serverGrandchild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(101, 101)), serverGrandchild2);
    QCOMPARE(WST::input_surface_at(serverParent, QPointF(124, 124)), serverGrandchild2);

    QVERIFY(!WST::input_surface_at(serverParent, QPointF(126, 125)));
    QVERIFY(!WST::input_surface_at(serverParent, QPointF(125, 126)));
}

void TestSubsurface::testDestroyAttachedBuffer()
{
    // this test verifies that destroying of a buffer attached to a sub-surface works
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    // create surface
    QSignalSpy serverSurfaceCreated(m_serverCompositor,
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    std::unique_ptr<Wrapland::Client::Surface> child(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverChildSurface
        = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();
    // create sub-surface
    auto* sub = m_subCompositor->createSubSurface(child.get(), parent.get());

    // let's damage this surface, will be in sub-surface pending state
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    child->attachBuffer(m_shm->createBuffer(image));
    child->damage(QRect(0, 0, 100, 100));
    child->commit(Wrapland::Client::Surface::CommitFlag::None);
    m_connection->flush();

    // Let's try to destroy it
    QSignalSpy destroySpy(serverChildSurface, &QObject::destroyed);
    QVERIFY(destroySpy.isValid());
    delete m_shm;
    m_shm = nullptr;
    child.reset();
    QVERIFY(destroySpy.wait());

    delete sub;
}

void TestSubsurface::testDestroyParentSurface()
{
    // this test verifies that destroying a parent surface does not create problems
    // see BUG 389231
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    // create surface
    QSignalSpy serverSurfaceCreated(m_serverCompositor,
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverParentSurface
        = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();
    std::unique_ptr<Wrapland::Client::Surface> child(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverChildSurface
        = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();
    std::unique_ptr<Wrapland::Client::Surface> grandChild(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverGrandChildSurface
        = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();
    // create sub-surface in desynchronized mode as Qt uses them
    auto sub1 = m_subCompositor->createSubSurface(child.get(), parent.get());
    sub1->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);
    auto sub2 = m_subCompositor->createSubSurface(grandChild.get(), child.get());
    sub2->setMode(Wrapland::Client::SubSurface::Mode::Desynchronized);

    // let's damage this surface
    // and at the same time delete the parent surface
    parent.reset();
    QSignalSpy parentDestroyedSpy(serverParentSurface, &QObject::destroyed);
    QVERIFY(parentDestroyedSpy.isValid());
    QVERIFY(parentDestroyedSpy.wait());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    grandChild->attachBuffer(m_shm->createBuffer(image));
    grandChild->damage(QRect(0, 0, 100, 100));
    grandChild->commit(Wrapland::Client::Surface::CommitFlag::None);
    QSignalSpy commit_spy(serverGrandChildSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QVERIFY(commit_spy.wait());

    // Let's try to destroy it
    QSignalSpy destroySpy(serverChildSurface, &QObject::destroyed);
    QVERIFY(destroySpy.isValid());
    child.reset();
    QVERIFY(destroySpy.wait());

    delete sub1;
    delete sub2;
}

QTEST_GUILESS_MAIN(TestSubsurface)
#include "subsurface.moc"
