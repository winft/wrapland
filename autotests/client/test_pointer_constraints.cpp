/********************************************************************
Copyright © 2016 Martin Gräßlin <mgraesslin@kde.org>
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

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
// client
#include "../../src/client/connection_thread.h"
#include "../../src/client/compositor.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/pointer.h"
#include "../../src/client/pointerconstraints.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/display.h"
#include "../../server/pointer_constraints_v1.h"
#include "../../server/seat.h"


#include "../../src/server/compositor_interface.h"
#include "../../src/server/surface_interface.h"

using namespace Wrapland::Client;
using namespace Wrapland::Server;

Q_DECLARE_METATYPE(Wrapland::Client::PointerConstraints::LifeTime)
Q_DECLARE_METATYPE(Wrapland::Server::ConfinedPointerV1::LifeTime)
Q_DECLARE_METATYPE(Wrapland::Server::LockedPointerV1::LifeTime)

class TestPointerConstraints : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testLockPointer_data();
    void testLockPointer();

    void testConfinePointer_data();
    void testConfinePointer();
    void testAlreadyConstrained_data();
    void testAlreadyConstrained();

private:
    D_isplay* m_display = nullptr;
    CompositorInterface* m_compositorInterface = nullptr;
    Wrapland::Server::Seat* m_serverSeat = nullptr;
    PointerConstraintsV1* m_pointerConstraintsServer = nullptr;
    ConnectionThread* m_connection = nullptr;
    QThread* m_thread = nullptr;
    EventQueue* m_queue = nullptr;
    Compositor* m_compositor = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::Pointer* m_pointer = nullptr;
    Wrapland::Client::PointerConstraints* m_pointerConstraints = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-pointer_constraint-0");

void TestPointerConstraints::init()
{
    delete m_display;
    m_display = new D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    m_display->createShm();
    m_serverSeat = m_display->createSeat(m_display);
    m_serverSeat->setHasPointer(true);
    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    m_pointerConstraintsServer = m_display->createPointerConstraints(m_display);

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy interfaceAnnouncedSpy(&registry, &Registry::interfaceAnnounced);
    QVERIFY(interfaceAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name, registry.interface(Registry::Interface::Compositor).version, this);
    QVERIFY(m_compositor);
    QVERIFY(m_compositor->isValid());

    m_pointerConstraints = registry.createPointerConstraints(registry.interface(Registry::Interface::PointerConstraintsUnstableV1).name,
                                                             registry.interface(Registry::Interface::PointerConstraintsUnstableV1).version, this);
    QVERIFY(m_pointerConstraints);
    QVERIFY(m_pointerConstraints->isValid());

    m_seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name, registry.interface(Registry::Interface::Seat).version, this);
    QVERIFY(m_seat);
    QVERIFY(m_seat->isValid());
    QSignalSpy pointerChangedSpy(m_seat, &Wrapland::Client::Seat::hasPointerChanged);
    QVERIFY(pointerChangedSpy.isValid());
    QVERIFY(pointerChangedSpy.wait());
    m_pointer = m_seat->createPointer(this);
    QVERIFY(m_pointer);
}

void TestPointerConstraints::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_pointerConstraints)
    CLEANUP(m_pointer)
    CLEANUP(m_seat)
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

    CLEANUP(m_compositorInterface)
    CLEANUP(m_serverSeat);
    CLEANUP(m_pointerConstraintsServer)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestPointerConstraints::testLockPointer_data()
{
    QTest::addColumn<PointerConstraints::LifeTime>("clientLifeTime");
    QTest::addColumn<LockedPointerV1::LifeTime>("serverLifeTime");
    QTest::addColumn<bool>("hasConstraintAfterUnlock");
    QTest::addColumn<int>("pointerChangedCount");

    QTest::newRow("persistent") << PointerConstraints::LifeTime::Persistent << LockedPointerV1::LifeTime::Persistent << true << 1;
    QTest::newRow("oneshot") << PointerConstraints::LifeTime::OneShot << LockedPointerV1::LifeTime::OneShot << false << 2;
}

void TestPointerConstraints::testLockPointer()
{
    // this test verifies the basic interaction for lock pointer
    // first create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->lockedPointer().isNull());
    QVERIFY(serverSurface->confinedPointer().isNull());

    // now create the locked pointer
    QSignalSpy pointerConstraintsChangedSpy(serverSurface, &SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(pointerConstraintsChangedSpy.isValid());
    QFETCH(PointerConstraints::LifeTime, clientLifeTime);
    std::unique_ptr<LockedPointer> lockedPointer(m_pointerConstraints->lockPointer(surface.get(), m_pointer, nullptr, clientLifeTime));
    QSignalSpy lockedSpy(lockedPointer.get(), &LockedPointer::locked);
    QVERIFY(lockedSpy.isValid());
    QSignalSpy unlockedSpy(lockedPointer.get(), &LockedPointer::unlocked);
    QVERIFY(unlockedSpy.isValid());
    QVERIFY(lockedPointer->isValid());
    QVERIFY(pointerConstraintsChangedSpy.wait());

    auto serverLockedPointer = serverSurface->lockedPointer();
    QVERIFY(serverLockedPointer);
    QVERIFY(serverSurface->confinedPointer().isNull());

    QCOMPARE(serverLockedPointer->isLocked(), false);
    QCOMPARE(serverLockedPointer->region(), QRegion());
    QFETCH(LockedPointerV1::LifeTime, serverLifeTime);
    QCOMPARE(serverLockedPointer->lifeTime(), serverLifeTime);
    // setting to unlocked now should not trigger an unlocked spy
    serverLockedPointer->setLocked(false);
    QVERIFY(!unlockedSpy.wait(500));

    // try setting a region
    QSignalSpy destroyedSpy(serverLockedPointer.data(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    QSignalSpy regionChangedSpy(serverLockedPointer.data(), &LockedPointerV1::regionChanged);
    QVERIFY(regionChangedSpy.isValid());
    lockedPointer->setRegion(m_compositor->createRegion(QRegion(0, 5, 10, 20), m_compositor));
    // it's double buffered
    QVERIFY(!regionChangedSpy.wait(500));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverLockedPointer->region(), QRegion(0, 5, 10, 20));
    // and unset region again
    lockedPointer->setRegion(nullptr);
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverLockedPointer->region(), QRegion());

    // let's lock the surface
    QSignalSpy lockedChangedSpy(serverLockedPointer.data(), &LockedPointerV1::lockedChanged);
    QVERIFY(lockedChangedSpy.isValid());
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QSignalSpy pointerMotionSpy(m_pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());
    m_serverSeat->setPointerPos(QPoint(0, 1));
    QVERIFY(pointerMotionSpy.wait());

    serverLockedPointer->setLocked(true);
    QCOMPARE(serverLockedPointer->isLocked(), true);
    m_serverSeat->setPointerPos(QPoint(1, 1));
    QCOMPARE(lockedChangedSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QVERIFY(lockedSpy.isEmpty());
    QVERIFY(lockedSpy.wait());
    QVERIFY(unlockedSpy.isEmpty());

    const QPointF hint = QPointF(1.5, 0.5);
    QSignalSpy hintChangedSpy(serverLockedPointer.data(), &LockedPointerV1::cursorPositionHintChanged);
    lockedPointer->setCursorPositionHint(hint);
    QCOMPARE(serverLockedPointer->cursorPositionHint(), QPointF(-1., -1.));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(hintChangedSpy.wait());
    QCOMPARE(serverLockedPointer->cursorPositionHint(), hint);

    // and unlock again
    serverLockedPointer->setLocked(false);
    QCOMPARE(serverLockedPointer->isLocked(), false);
    QCOMPARE(serverLockedPointer->cursorPositionHint(), QPointF(-1., -1.));
    QCOMPARE(lockedChangedSpy.count(), 2);
    QTEST(!serverSurface->lockedPointer().isNull(), "hasConstraintAfterUnlock");
    QTEST(pointerConstraintsChangedSpy.count(), "pointerChangedCount");
    QVERIFY(unlockedSpy.wait());
    QCOMPARE(unlockedSpy.count(), 1);
    QCOMPARE(lockedSpy.count(), 1);

    // now motion should work again
    m_serverSeat->setPointerPos(QPoint(0, 1));
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.count(), 2);

    lockedPointer.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(pointerConstraintsChangedSpy.count(), 2);
}

void TestPointerConstraints::testConfinePointer_data()
{
    QTest::addColumn<PointerConstraints::LifeTime>("clientLifeTime");
    QTest::addColumn<ConfinedPointerV1::LifeTime>("serverLifeTime");
    QTest::addColumn<bool>("hasConstraintAfterUnlock");
    QTest::addColumn<int>("pointerChangedCount");

    QTest::newRow("persistent") << PointerConstraints::LifeTime::Persistent << ConfinedPointerV1::LifeTime::Persistent << true << 1;
    QTest::newRow("oneshot") << PointerConstraints::LifeTime::OneShot << ConfinedPointerV1::LifeTime::OneShot << false << 2;
}

void TestPointerConstraints::testConfinePointer()
{
    // this test verifies the basic interaction for confined pointer
    // first create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->lockedPointer().isNull());
    QVERIFY(serverSurface->confinedPointer().isNull());

    // now create the confined pointer
    QSignalSpy pointerConstraintsChangedSpy(serverSurface, &SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(pointerConstraintsChangedSpy.isValid());
    QFETCH(PointerConstraints::LifeTime, clientLifeTime);
    std::unique_ptr<ConfinedPointer> confinedPointer(m_pointerConstraints->confinePointer(surface.get(), m_pointer, nullptr, clientLifeTime));
    QSignalSpy confinedSpy(confinedPointer.get(), &ConfinedPointer::confined);
    QVERIFY(confinedSpy.isValid());
    QSignalSpy unconfinedSpy(confinedPointer.get(), &ConfinedPointer::unconfined);
    QVERIFY(unconfinedSpy.isValid());
    QVERIFY(confinedPointer->isValid());
    QVERIFY(pointerConstraintsChangedSpy.wait());

    auto serverConfinedPointer = serverSurface->confinedPointer();
    QVERIFY(serverConfinedPointer);
    QVERIFY(serverSurface->lockedPointer().isNull());

    QCOMPARE(serverConfinedPointer->isConfined(), false);
    QCOMPARE(serverConfinedPointer->region(), QRegion());
    QFETCH(ConfinedPointerV1::LifeTime, serverLifeTime);
    QCOMPARE(serverConfinedPointer->lifeTime(), serverLifeTime);
    // setting to unconfined now should not trigger an unconfined spy
    serverConfinedPointer->setConfined(false);
    QVERIFY(!unconfinedSpy.wait(500));

    // try setting a region
    QSignalSpy destroyedSpy(serverConfinedPointer.data(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    QSignalSpy regionChangedSpy(serverConfinedPointer.data(), &ConfinedPointerV1::regionChanged);
    QVERIFY(regionChangedSpy.isValid());
    confinedPointer->setRegion(m_compositor->createRegion(QRegion(0, 5, 10, 20), m_compositor));
    // it's double buffered
    QVERIFY(!regionChangedSpy.wait(500));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverConfinedPointer->region(), QRegion(0, 5, 10, 20));
    // and unset region again
    confinedPointer->setRegion(nullptr);
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverConfinedPointer->region(), QRegion());

    // let's confine the surface
    QSignalSpy confinedChangedSpy(serverConfinedPointer.data(), &ConfinedPointerV1::confinedChanged);
    QVERIFY(confinedChangedSpy.isValid());
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    serverConfinedPointer->setConfined(true);
    QCOMPARE(serverConfinedPointer->isConfined(), true);
    QCOMPARE(confinedChangedSpy.count(), 1);
    QVERIFY(confinedSpy.isEmpty());
    QVERIFY(confinedSpy.wait());
    QVERIFY(unconfinedSpy.isEmpty());

    // and unconfine again
    serverConfinedPointer->setConfined(false);
    QCOMPARE(serverConfinedPointer->isConfined(), false);
    QCOMPARE(confinedChangedSpy.count(), 2);
    QTEST(!serverSurface->confinedPointer().isNull(), "hasConstraintAfterUnlock");
    QTEST(pointerConstraintsChangedSpy.count(), "pointerChangedCount");
    QVERIFY(unconfinedSpy.wait());
    QCOMPARE(unconfinedSpy.count(), 1);
    QCOMPARE(confinedSpy.count(), 1);

    confinedPointer.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(pointerConstraintsChangedSpy.count(), 2);
}

enum class Constraint {
    Lock,
    Confine
};

Q_DECLARE_METATYPE(Constraint)

void TestPointerConstraints::testAlreadyConstrained_data()
{
    QTest::addColumn<Constraint>("firstConstraint");
    QTest::addColumn<Constraint>("secondConstraint");

    QTest::newRow("confine-confine") << Constraint::Confine << Constraint::Confine;
    QTest::newRow("lock-confine") << Constraint::Lock << Constraint::Confine;
    QTest::newRow("confine-lock") << Constraint::Confine << Constraint::Lock;
    QTest::newRow("lock-lock") << Constraint::Lock << Constraint::Lock;
}

void TestPointerConstraints::testAlreadyConstrained()
{
    // this test verifies that creating a pointer constraint for an already constrained surface triggers an error
    // first create a surface
    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QFETCH(Constraint, firstConstraint);
    std::unique_ptr<ConfinedPointer> confinedPointer;
    std::unique_ptr<LockedPointer> lockedPointer;
    switch (firstConstraint) {
    case Constraint::Lock:
        lockedPointer.reset(m_pointerConstraints->lockPointer(surface.get(), m_pointer, nullptr, PointerConstraints::LifeTime::OneShot));
        break;
    case Constraint::Confine:
        confinedPointer.reset(m_pointerConstraints->confinePointer(surface.get(), m_pointer, nullptr, PointerConstraints::LifeTime::OneShot));
        break;
    default:
        Q_UNREACHABLE();
    }
    QVERIFY(confinedPointer || lockedPointer);

    QSignalSpy connectionSpy(m_connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectionSpy.isValid());
    QFETCH(Constraint, secondConstraint);
    std::unique_ptr<ConfinedPointer> confinedPointer2;
    std::unique_ptr<LockedPointer> lockedPointer2;
    switch (secondConstraint) {
    case Constraint::Lock:
        lockedPointer2.reset(m_pointerConstraints->lockPointer(surface.get(), m_pointer, nullptr, PointerConstraints::LifeTime::OneShot));
        break;
    case Constraint::Confine:
        confinedPointer2.reset(m_pointerConstraints->confinePointer(surface.get(), m_pointer, nullptr, PointerConstraints::LifeTime::OneShot));
        break;
    default:
        Q_UNREACHABLE();
    }
    QVERIFY(connectionSpy.wait());
    QVERIFY(m_connection->protocolError());
    if (confinedPointer2) {
        confinedPointer2->release();
    }
    if (lockedPointer2) {
        lockedPointer2->release();
    }
    if (confinedPointer) {
        confinedPointer->release();
    }
    if (lockedPointer) {
        lockedPointer->release();
    }
    surface->release();
    m_compositor->release();
    m_pointerConstraints->release();
    m_pointer->release();
    m_seat->release();
    m_queue->release();
}

QTEST_GUILESS_MAIN(TestPointerConstraints)
#include "test_pointer_constraints.moc"
