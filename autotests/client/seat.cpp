/********************************************************************
Copyright © 2014  Martin Gräßlin <mgraesslin@kde.org>
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
#include "../../src/client/seat.h"

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/pointer.h"
#include "../../src/client/pointergestures.h"
#include "../../src/client/registry.h"
#include "../../src/client/relativepointer.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/subcompositor.h"
#include "../../src/client/subsurface.h"
#include "../../src/client/surface.h"
#include "../../src/client/touch.h"

#include "../../src/server/buffer_interface.h"
#include "../../src/server/clientconnection.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/datadevicemanager_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/subcompositor_interface.h"
#include "../../src/server/surface_interface.h"

#include "../../server/client.h"
#include "../../server/display.h"
#include "../../server/keyboard.h"
#include "../../server/pointer.h"
#include "../../server/pointer_gestures_v1.h"
#include "../../server/relative_pointer_v1.h"
#include "../../server/seat.h"

#include <QtTest>

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class TestSeat : public QObject
{
    Q_OBJECT
public:
    explicit TestSeat(QObject* parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void testName();

    void testCapabilities_data();
    void testCapabilities();
    void testPointer();

    void testPointerTransformation_data();
    void testPointerTransformation();
    void testPointerButton_data();
    void testPointerButton();

    void testPointerSubSurfaceTree();

    void testPointerSwipeGesture_data();
    void testPointerSwipeGesture();
    void testPointerPinchGesture_data();
    void testPointerPinchGesture();

    void testPointerAxis();
    void testKeyboardSubSurfaceTreeFromPointer();
    void testCursor();
    void testCursorDamage();
    void testKeyboard();
    void testCast();
    void testDestroy();
    void testSelection();
    void testSelectionNoDataSource();
    void testDataDeviceForKeyboardSurface();
    void testTouch();
    void testDisconnect();
    void testPointerEnterOnUnboundSurface();
    // TODO: add test for keymap

private:
    Srv::D_isplay* m_display;
    Srv::CompositorInterface* m_compositorInterface;
    Srv::Seat* m_serverSeat;
    Srv::SubCompositorInterface* m_subCompositorInterface;
    Srv::RelativePointerManagerV1* m_relativePointerManagerServer;
    Srv::PointerGesturesV1* m_pointerGesturesInterface;
    Clt::ConnectionThread* m_connection;
    Clt::Compositor* m_compositor;
    Clt::Seat* m_seat;
    Clt::ShmPool* m_shm;
    Clt::SubCompositor* m_subCompositor;
    Clt::RelativePointerManager* m_relativePointerManager;
    Clt::PointerGestures* m_pointerGestures;
    Clt::EventQueue* m_queue;
    QThread* m_thread;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-seat-0");

TestSeat::TestSeat(QObject* parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_serverSeat(nullptr)
    , m_subCompositorInterface(nullptr)
    , m_relativePointerManagerServer(nullptr)
    , m_pointerGesturesInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_seat(nullptr)
    , m_shm(nullptr)
    , m_subCompositor(nullptr)
    , m_relativePointerManager(nullptr)
    , m_pointerGestures(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
    qRegisterMetaType<Wrapland::Server::Keyboard*>();
    qRegisterMetaType<Wrapland::Server::Pointer*>();
}

void TestSeat::init()
{
    m_display = new Srv::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    m_display->createShm();

    m_compositorInterface = m_display->createCompositor(m_display);
    QVERIFY(m_compositorInterface);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    m_subCompositorInterface = m_display->createSubCompositor(m_display);
    QVERIFY(m_subCompositorInterface);
    m_subCompositorInterface->create();
    QVERIFY(m_subCompositorInterface->isValid());

    m_relativePointerManagerServer = m_display->createRelativePointerManager(m_display);
    QVERIFY(m_relativePointerManagerServer);

    m_pointerGesturesInterface = m_display->createPointerGestures(m_display);
    QVERIFY(m_pointerGesturesInterface);

    // Setup connection.
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Clt::EventQueue(this);
    m_queue->setup(m_connection);

    Clt::Registry registry;
    QSignalSpy compositorSpy(&registry, &Clt::Registry::compositorAnnounced);
    QSignalSpy seatSpy(&registry, &Clt::Registry::seatAnnounced);
    QSignalSpy shmSpy(&registry, &Clt::Registry::shmAnnounced);

    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(compositorSpy.wait());

    m_serverSeat = m_display->createSeat();
    QVERIFY(m_serverSeat);
    m_serverSeat->setName("seat0");

    QVERIFY(seatSpy.wait());

    m_compositor = new Clt::Compositor(this);
    m_compositor->setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(),
                                                compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_compositor->isValid());

    m_seat = registry.createSeat(
        seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QSignalSpy nameSpy(m_seat, &Clt::Seat::nameChanged);
    QVERIFY(nameSpy.wait());

    m_shm = new Clt::ShmPool(this);
    m_shm->setup(registry.bindShm(shmSpy.first().first().value<quint32>(),
                                  shmSpy.first().last().value<quint32>()));
    QVERIFY(m_shm->isValid());

    m_subCompositor = registry.createSubCompositor(
        registry.interface(Clt::Registry::Interface::SubCompositor).name,
        registry.interface(Clt::Registry::Interface::SubCompositor).version,
        this);
    QVERIFY(m_subCompositor->isValid());

    m_relativePointerManager = registry.createRelativePointerManager(
        registry.interface(Clt::Registry::Interface::RelativePointerManagerUnstableV1).name,
        registry.interface(Clt::Registry::Interface::RelativePointerManagerUnstableV1).version,
        this);
    QVERIFY(m_relativePointerManager->isValid());

    m_pointerGestures = registry.createPointerGestures(
        registry.interface(Clt::Registry::Interface::PointerGesturesUnstableV1).name,
        registry.interface(Clt::Registry::Interface::PointerGesturesUnstableV1).version,
        this);
    QVERIFY(m_pointerGestures->isValid());
}

void TestSeat::cleanup()
{
    delete m_pointerGestures;
    m_pointerGestures = nullptr;

    delete m_relativePointerManager;
    m_relativePointerManager = nullptr;

    delete m_subCompositor;
    m_subCompositor = nullptr;

    delete m_shm;
    m_shm = nullptr;

    delete m_seat;
    m_seat = nullptr;

    delete m_compositor;
    m_compositor = nullptr;

    delete m_queue;
    m_queue = nullptr;

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

    delete m_compositorInterface;
    m_compositorInterface = nullptr;

    delete m_serverSeat;
    m_serverSeat = nullptr;

    delete m_subCompositorInterface;
    m_subCompositorInterface = nullptr;

    delete m_relativePointerManagerServer;
    m_relativePointerManagerServer = nullptr;

    delete m_pointerGesturesInterface;
    m_pointerGesturesInterface = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestSeat::testName()
{
    // No name set yet.
    QCOMPARE(m_seat->name(), QStringLiteral("seat0"));

    QSignalSpy spy(m_seat, &Clt::Seat::nameChanged);
    QVERIFY(spy.isValid());

    const std::string name("foobar");
    m_serverSeat->setName(name);
    QVERIFY(spy.wait());
    QCOMPARE(m_seat->name(), QString::fromStdString(name));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QString::fromStdString(name));
}

void TestSeat::testCapabilities_data()
{
    QTest::addColumn<bool>("pointer");
    QTest::addColumn<bool>("keyboard");
    QTest::addColumn<bool>("touch");

    // clang-format off
    QTest::newRow("none")             << false << false << false;
    QTest::newRow("pointer")          << true  << false << false;
    QTest::newRow("keyboard")         << false << true  << false;
    QTest::newRow("touch")            << false << false << true;
    QTest::newRow("pointer/keyboard") << true  << true  << false;
    QTest::newRow("pointer/touch")    << true  << false << true;
    QTest::newRow("keyboard/touch")   << false << true  << true;
    QTest::newRow("all")              << true  << true  << true;
    // clang-format on
}

void TestSeat::testCapabilities()
{
    QVERIFY(!m_seat->hasPointer());
    QVERIFY(!m_seat->hasKeyboard());
    QVERIFY(!m_seat->hasTouch());

    QFETCH(bool, pointer);
    QFETCH(bool, keyboard);
    QFETCH(bool, touch);

    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    QSignalSpy keyboardSpy(m_seat, &Clt::Seat::hasKeyboardChanged);
    QVERIFY(keyboardSpy.isValid());
    QSignalSpy touchSpy(m_seat, &Clt::Seat::hasTouchChanged);
    QVERIFY(touchSpy.isValid());

    m_serverSeat->setHasPointer(pointer);
    m_serverSeat->setHasKeyboard(keyboard);
    m_serverSeat->setHasTouch(touch);

    // Do processing.
    QCOMPARE(pointerSpy.wait(200), pointer);
    QCOMPARE(pointerSpy.isEmpty(), !pointer);
    if (!pointerSpy.isEmpty()) {
        QCOMPARE(pointerSpy.first().first().toBool(), pointer);
    }

    if (keyboardSpy.isEmpty()) {
        QCOMPARE(keyboardSpy.wait(200), keyboard);
    }
    QCOMPARE(keyboardSpy.isEmpty(), !keyboard);
    if (!keyboardSpy.isEmpty()) {
        QCOMPARE(keyboardSpy.first().first().toBool(), keyboard);
    }

    if (touchSpy.isEmpty()) {
        QCOMPARE(touchSpy.wait(200), touch);
    }
    QCOMPARE(touchSpy.isEmpty(), !touch);
    if (!touchSpy.isEmpty()) {
        QCOMPARE(touchSpy.first().first().toBool(), touch);
    }

    QCOMPARE(m_seat->hasPointer(), pointer);
    QCOMPARE(m_seat->hasKeyboard(), keyboard);
    QCOMPARE(m_seat->hasTouch(), touch);
}

void TestSeat::testPointer()
{
    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    m_serverSeat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    auto s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());

    Srv::SurfaceInterface* serverSurface
        = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy focusedPointerChangedSpy(m_serverSeat, &Srv::Seat::focusedPointerChanged);
    QVERIFY(focusedPointerChangedSpy.isValid());

    m_serverSeat->setPointerPos(QPoint(20, 18));
    m_serverSeat->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    QCOMPARE(focusedPointerChangedSpy.count(), 1);
    QVERIFY(!focusedPointerChangedSpy.first().first().value<Srv::Pointer*>());

    // No pointer yet.
    QVERIFY(m_serverSeat->focusedPointerSurface());
    QVERIFY(!m_serverSeat->focusedPointer());

    auto p = m_seat->createPointer(m_seat);
    QSignalSpy frameSpy(p, &Clt::Pointer::frame);
    QVERIFY(frameSpy.isValid());
    const Clt::Pointer& cp = *p;
    QVERIFY(p->isValid());

    QScopedPointer<Clt::RelativePointer> relativePointer(
        m_relativePointerManager->createRelativePointer(p));
    QVERIFY(relativePointer->isValid());

    QSignalSpy pointerCreatedSpy(m_serverSeat, &Srv::Seat::pointerCreated);
    QVERIFY(pointerCreatedSpy.isValid());

    // Once the pointer is created it should be set as the focused pointer.
    QVERIFY(pointerCreatedSpy.wait());
    QVERIFY(m_serverSeat->focusedPointer());
    QCOMPARE(pointerCreatedSpy.first().first().value<Srv::Pointer*>(),
             m_serverSeat->focusedPointer());
    QCOMPARE(focusedPointerChangedSpy.count(), 2);
    QCOMPARE(focusedPointerChangedSpy.last().first().value<Srv::Pointer*>(),
             m_serverSeat->focusedPointer());
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 1);

    m_serverSeat->setFocusedPointerSurface(nullptr);
    QCOMPARE(focusedPointerChangedSpy.count(), 3);
    QVERIFY(!focusedPointerChangedSpy.last().first().value<Srv::Pointer*>());
    serverSurface->client()->flush();
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 2);

    QSignalSpy enteredSpy(p, &Clt::Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    QSignalSpy leftSpy(p, &Clt::Pointer::left);
    QVERIFY(leftSpy.isValid());

    QSignalSpy motionSpy(p, &Clt::Pointer::motion);
    QVERIFY(motionSpy.isValid());

    QSignalSpy axisSpy(p, &Clt::Pointer::axisChanged);
    QVERIFY(axisSpy.isValid());

    QSignalSpy buttonSpy(p, &Clt::Pointer::buttonStateChanged);
    QVERIFY(buttonSpy.isValid());

    QSignalSpy relativeMotionSpy(relativePointer.data(), &Clt::RelativePointer::relativeMotion);
    QVERIFY(relativeMotionSpy.isValid());

    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());
    m_serverSeat->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    QCOMPARE(m_serverSeat->focusedPointerSurface(), serverSurface);

    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(enteredSpy.first().last().toPoint(), QPoint(10, 3));
    QTRY_COMPARE(frameSpy.count(), 3);

    auto serverPointer = m_serverSeat->focusedPointer();
    QVERIFY(serverPointer);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);
    QCOMPARE(focusedPointerChangedSpy.count(), 4);
    QCOMPARE(focusedPointerChangedSpy.last().first().value<Srv::Pointer*>(), serverPointer);

    // Test motion.
    m_serverSeat->setTimestamp(1);
    m_serverSeat->setPointerPos(QPoint(10, 16));

    QVERIFY(motionSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 4);
    QCOMPARE(motionSpy.first().first().toPoint(), QPoint(0, 1));
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // Test relative motion.
    m_serverSeat->relativePointerMotion(QSizeF(1, 2), QSizeF(3, 4), quint64(-1));
    QVERIFY(relativeMotionSpy.wait());
    QCOMPARE(relativeMotionSpy.count(), 1);
    QTRY_COMPARE(frameSpy.count(), 5);
    QCOMPARE(relativeMotionSpy.first().at(0).toSizeF(), QSizeF(1, 2));
    QCOMPARE(relativeMotionSpy.first().at(1).toSizeF(), QSizeF(3, 4));
    QCOMPARE(relativeMotionSpy.first().at(2).value<quint64>(), quint64(-1));

    // Test axis.
    m_serverSeat->setTimestamp(2);
    m_serverSeat->pointerAxis(Qt::Horizontal, 10);
    QVERIFY(axisSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 6);
    m_serverSeat->setTimestamp(3);
    m_serverSeat->pointerAxis(Qt::Vertical, 20);

    QVERIFY(axisSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 7);
    QCOMPARE(axisSpy.first().at(0).value<quint32>(), quint32(2));
    QCOMPARE(axisSpy.first().at(1).value<Clt::Pointer::Axis>(), Clt::Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.first().at(2).value<qreal>(), qreal(10));

    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(3));
    QCOMPARE(axisSpy.last().at(1).value<Clt::Pointer::Axis>(), Clt::Pointer::Axis::Vertical);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), qreal(20));

    // Test button.
    m_serverSeat->setTimestamp(4);
    m_serverSeat->pointerButtonPressed(1);
    QVERIFY(buttonSpy.wait());
    QTRY_COMPARE(buttonSpy.count(), 1);
    QTRY_COMPARE(frameSpy.count(), 8);
    QCOMPARE(buttonSpy.at(0).at(0).value<quint32>(), m_display->serial());
    m_serverSeat->setTimestamp(5);
    m_serverSeat->pointerButtonPressed(2);

    QVERIFY(buttonSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 9);
    QCOMPARE(buttonSpy.at(1).at(0).value<quint32>(), m_display->serial());
    m_serverSeat->setTimestamp(6);
    m_serverSeat->pointerButtonReleased(2);

    QVERIFY(buttonSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 10);
    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), m_display->serial());
    m_serverSeat->setTimestamp(7);
    m_serverSeat->pointerButtonReleased(1);

    QVERIFY(buttonSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 11);
    QCOMPARE(buttonSpy.count(), 4);

    // Timestamp
    QCOMPARE(buttonSpy.at(0).at(1).value<quint32>(), quint32(4));
    // Button
    QCOMPARE(buttonSpy.at(0).at(2).value<quint32>(), quint32(1));
    QCOMPARE(buttonSpy.at(0).at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Pressed);

    // Timestamp
    QCOMPARE(buttonSpy.at(1).at(1).value<quint32>(), quint32(5));
    // Button
    QCOMPARE(buttonSpy.at(1).at(2).value<quint32>(), quint32(2));
    QCOMPARE(buttonSpy.at(1).at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Pressed);

    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), m_serverSeat->pointerButtonSerial(2));
    // Timestamp
    QCOMPARE(buttonSpy.at(2).at(1).value<quint32>(), quint32(6));
    // Button
    QCOMPARE(buttonSpy.at(2).at(2).value<quint32>(), quint32(2));
    QCOMPARE(buttonSpy.at(2).at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Released);

    QCOMPARE(buttonSpy.at(3).at(0).value<quint32>(), m_serverSeat->pointerButtonSerial(1));
    // Timestamp
    QCOMPARE(buttonSpy.at(3).at(1).value<quint32>(), quint32(7));
    // Button
    QCOMPARE(buttonSpy.at(3).at(2).value<quint32>(), quint32(1));
    QCOMPARE(buttonSpy.at(3).at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Released);

    // Leave the surface.
    m_serverSeat->setFocusedPointerSurface(nullptr);
    QCOMPARE(focusedPointerChangedSpy.count(), 5);
    QVERIFY(leftSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 12);
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial());
    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());

    // Now a relative motion should not be sent to the relative pointer.
    m_serverSeat->relativePointerMotion(QSizeF(1, 2), QSizeF(3, 4), quint64(-1));
    QVERIFY(!relativeMotionSpy.wait(200));

    // Enter it again.
    m_serverSeat->setFocusedPointerSurface(serverSurface, QPoint(0, 0));
    QCOMPARE(focusedPointerChangedSpy.count(), 6);
    QVERIFY(enteredSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 13);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    // Send another relative motion event.
    m_serverSeat->relativePointerMotion(QSizeF(4, 5), QSizeF(6, 7), quint64(1));
    QVERIFY(relativeMotionSpy.wait());
    QCOMPARE(relativeMotionSpy.count(), 2);
    QCOMPARE(relativeMotionSpy.last().at(0).toSizeF(), QSizeF(4, 5));
    QCOMPARE(relativeMotionSpy.last().at(1).toSizeF(), QSizeF(6, 7));
    QCOMPARE(relativeMotionSpy.last().at(2).value<quint64>(), quint64(1));

    // Destroy the focused pointer.
    QSignalSpy unboundSpy(serverPointer, &Srv::Pointer::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());

    delete p;
    QVERIFY(unboundSpy.wait());
    QCOMPARE(unboundSpy.count(), 1);

    // Now test that calling into the methods in Seat does not crash.
    // The focused pointer must be null now since it got destroyed.
    QCOMPARE(m_serverSeat->focusedPointer(), nullptr);
    // The focused surface is still the same since it does still exist and it was once set
    // and not changed since then.
    QCOMPARE(m_serverSeat->focusedPointerSurface(), serverSurface);

    m_serverSeat->setTimestamp(8);
    m_serverSeat->setPointerPos(QPoint(10, 15));
    m_serverSeat->setTimestamp(9);
    m_serverSeat->pointerButtonPressed(1);
    m_serverSeat->setTimestamp(10);
    m_serverSeat->pointerButtonReleased(1);
    m_serverSeat->setTimestamp(11);
    m_serverSeat->pointerAxis(Qt::Horizontal, 10);
    m_serverSeat->setTimestamp(12);
    m_serverSeat->pointerAxis(Qt::Vertical, 20);

    m_serverSeat->setFocusedPointerSurface(nullptr);
    QCOMPARE(focusedPointerChangedSpy.count(), 8);

    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QCOMPARE(focusedPointerChangedSpy.count(), 9);

    QCOMPARE(m_serverSeat->focusedPointerSurface(), serverSurface);
    QVERIFY(!m_serverSeat->focusedPointer());

    // Create a pointer again.
    p = m_seat->createPointer(m_seat);
    QVERIFY(focusedPointerChangedSpy.wait());
    QCOMPARE(focusedPointerChangedSpy.count(), 10);
    QCOMPARE(m_serverSeat->focusedPointerSurface(), serverSurface);
    serverPointer = m_serverSeat->focusedPointer();
    QVERIFY(serverPointer);

    QSignalSpy entered2Spy(p, &Clt::Pointer::entered);
    QVERIFY(entered2Spy.wait());
    QCOMPARE(p->enteredSurface(), s);
    QSignalSpy leftSpy2(p, &Clt::Pointer::left);
    QVERIFY(leftSpy2.isValid());
    delete s;
    QVERIFY(!p->enteredSurface());
    QVERIFY(leftSpy2.wait());
    QCOMPARE(focusedPointerChangedSpy.count(), 11);
    QVERIFY(!m_serverSeat->focusedPointerSurface());
    QVERIFY(!m_serverSeat->focusedPointer());
}

void TestSeat::testPointerTransformation_data()
{
    QTest::addColumn<QMatrix4x4>("enterTransformation");
    // Global position at 20/18.
    QTest::addColumn<QPointF>("expectedEnterPoint");
    // Global position at 10/16.
    QTest::addColumn<QPointF>("expectedMovePoint");

    QMatrix4x4 tm;
    tm.translate(-10, -15);
    QTest::newRow("translation") << tm << QPointF(10, 3) << QPointF(0, 1);
    QMatrix4x4 sm;
    sm.scale(2, 2);
    QTest::newRow("scale") << sm << QPointF(40, 36) << QPointF(20, 32);
    QMatrix4x4 rotate;
    rotate.rotate(90, 0, 0, 1);
    QTest::newRow("rotate") << rotate << QPointF(-18, 20) << QPointF(-16, 10);
}

void TestSeat::testPointerTransformation()
{
    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    m_serverSeat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    auto s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    m_serverSeat->setPointerPos(QPoint(20, 18));
    QFETCH(QMatrix4x4, enterTransformation);
    m_serverSeat->setFocusedPointerSurface(serverSurface, enterTransformation);
    QCOMPARE(m_serverSeat->focusedPointerSurfaceTransformation(), enterTransformation);

    // No pointer yet.
    QVERIFY(m_serverSeat->focusedPointerSurface());
    QVERIFY(!m_serverSeat->focusedPointer());

    auto p = m_seat->createPointer(m_seat);
    const Clt::Pointer& cp = *p;
    QVERIFY(p->isValid());
    QSignalSpy pointerCreatedSpy(m_serverSeat, &Srv::Seat::pointerCreated);
    QVERIFY(pointerCreatedSpy.isValid());

    // Once the pointer is created it should be set as the focused pointer.
    QVERIFY(pointerCreatedSpy.wait());
    QVERIFY(m_serverSeat->focusedPointer());
    QCOMPARE(pointerCreatedSpy.first().first().value<Srv::Pointer*>(),
             m_serverSeat->focusedPointer());

    m_serverSeat->setFocusedPointerSurface(nullptr);
    serverSurface->client()->flush();
    QTest::qWait(100);

    QSignalSpy enteredSpy(p, &Clt::Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    QSignalSpy leftSpy(p, &Clt::Pointer::left);
    QVERIFY(leftSpy.isValid());

    QSignalSpy motionSpy(p, &Clt::Pointer::motion);
    QVERIFY(motionSpy.isValid());

    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());
    m_serverSeat->setFocusedPointerSurface(serverSurface, enterTransformation);
    QCOMPARE(m_serverSeat->focusedPointerSurface(), serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial());
    QTEST(enteredSpy.first().last().toPointF(), "expectedEnterPoint");

    auto serverPointer = m_serverSeat->focusedPointer();
    QVERIFY(serverPointer);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    // Test motion.
    m_serverSeat->setTimestamp(1);
    m_serverSeat->setPointerPos(QPoint(10, 16));
    QVERIFY(motionSpy.wait());
    QTEST(motionSpy.first().first().toPointF(), "expectedMovePoint");
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // Leave the surface.
    m_serverSeat->setFocusedPointerSurface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial());
    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());

    // Enter it again.
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    delete s;
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    QVERIFY(!m_serverSeat->focusedPointerSurface());
}

Q_DECLARE_METATYPE(Qt::MouseButton)

void TestSeat::testPointerButton_data()
{
    QTest::addColumn<Qt::MouseButton>("qtButton");
    QTest::addColumn<quint32>("waylandButton");

    // clang-format off
    QTest::newRow("left")    << Qt::LeftButton    << quint32(BTN_LEFT);
    QTest::newRow("right")   << Qt::RightButton   << quint32(BTN_RIGHT);
    QTest::newRow("mid")     << Qt::MidButton     << quint32(BTN_MIDDLE);
    QTest::newRow("middle")  << Qt::MiddleButton  << quint32(BTN_MIDDLE);
    QTest::newRow("back")    << Qt::BackButton    << quint32(BTN_BACK);
    QTest::newRow("x1")      << Qt::XButton1      << quint32(BTN_BACK);
    QTest::newRow("extra1")  << Qt::ExtraButton1  << quint32(BTN_BACK);
    QTest::newRow("forward") << Qt::ForwardButton << quint32(BTN_FORWARD);
    QTest::newRow("x2")      << Qt::XButton2      << quint32(BTN_FORWARD);
    QTest::newRow("extra2")  << Qt::ExtraButton2  << quint32(BTN_FORWARD);
    QTest::newRow("task")    << Qt::TaskButton    << quint32(BTN_TASK);
    QTest::newRow("extra3")  << Qt::ExtraButton3  << quint32(BTN_TASK);
    QTest::newRow("extra4")  << Qt::ExtraButton4  << quint32(BTN_EXTRA);
    QTest::newRow("extra5")  << Qt::ExtraButton5  << quint32(BTN_SIDE);
    QTest::newRow("extra6")  << Qt::ExtraButton6  << quint32(0x118);
    QTest::newRow("extra7")  << Qt::ExtraButton7  << quint32(0x119);
    QTest::newRow("extra8")  << Qt::ExtraButton8  << quint32(0x11a);
    QTest::newRow("extra9")  << Qt::ExtraButton9  << quint32(0x11b);
    QTest::newRow("extra10") << Qt::ExtraButton10 << quint32(0x11c);
    QTest::newRow("extra11") << Qt::ExtraButton11 << quint32(0x11d);
    QTest::newRow("extra12") << Qt::ExtraButton12 << quint32(0x11e);
    QTest::newRow("extra13") << Qt::ExtraButton13 << quint32(0x11f);
    // clang-format on
}

void TestSeat::testPointerButton()
{
    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    m_serverSeat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QScopedPointer<Clt::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    QSignalSpy buttonChangedSpy(p.data(), &Clt::Pointer::buttonStateChanged);
    QVERIFY(buttonChangedSpy.isValid());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    m_serverSeat->setPointerPos(QPoint(20, 18));
    m_serverSeat->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    QVERIFY(m_serverSeat->focusedPointerSurface());
    QVERIFY(m_serverSeat->focusedPointer());

    QCoreApplication::processEvents();

    m_serverSeat->setFocusedPointerSurface(serverSurface, QPoint(10, 15));

    auto serverPointer = m_serverSeat->focusedPointer();
    QVERIFY(serverPointer);
    QFETCH(Qt::MouseButton, qtButton);
    QFETCH(quint32, waylandButton);

    quint32 msec = QDateTime::currentMSecsSinceEpoch();
    QCOMPARE(m_serverSeat->isPointerButtonPressed(waylandButton), false);
    QCOMPARE(m_serverSeat->isPointerButtonPressed(qtButton), false);
    m_serverSeat->setTimestamp(msec);
    m_serverSeat->pointerButtonPressed(qtButton);
    QCOMPARE(m_serverSeat->isPointerButtonPressed(waylandButton), true);
    QCOMPARE(m_serverSeat->isPointerButtonPressed(qtButton), true);

    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 1);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(),
             m_serverSeat->pointerButtonSerial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(),
             m_serverSeat->pointerButtonSerial(qtButton));
    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), msec);
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Pressed);

    msec = QDateTime::currentMSecsSinceEpoch();
    m_serverSeat->setTimestamp(QDateTime::currentMSecsSinceEpoch());
    m_serverSeat->pointerButtonReleased(qtButton);
    QCOMPARE(m_serverSeat->isPointerButtonPressed(waylandButton), false);
    QCOMPARE(m_serverSeat->isPointerButtonPressed(qtButton), false);

    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 2);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(),
             m_serverSeat->pointerButtonSerial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(),
             m_serverSeat->pointerButtonSerial(qtButton));

    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), msec);
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Released);
}

void TestSeat::testPointerSubSurfaceTree()
{
    // This test verifies that pointer motion on a surface with sub-surfaces sends motion
    // enter/leave to the sub-surface.

    // First create the pointer.
    QSignalSpy hasPointerChangedSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    m_serverSeat->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());

    // Create a sub surface tree.
    // Parent surface (100, 100) with one sub surface taking the half of it's size (50, 100)
    // which has two further children (50, 50) which are overlapping.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> parentSurface(m_compositor->createSurface());
    QScopedPointer<Clt::Surface> childSurface(m_compositor->createSurface());
    QScopedPointer<Clt::Surface> grandChild1Surface(m_compositor->createSurface());
    QScopedPointer<Clt::Surface> grandChild2Surface(m_compositor->createSurface());

    QScopedPointer<Clt::SubSurface> childSubSurface(
        m_subCompositor->createSubSurface(childSurface.data(), parentSurface.data()));
    QScopedPointer<Clt::SubSurface> grandChild1SubSurface(
        m_subCompositor->createSubSurface(grandChild1Surface.data(), childSurface.data()));
    QScopedPointer<Clt::SubSurface> grandChild2SubSurface(
        m_subCompositor->createSubSurface(grandChild2Surface.data(), childSurface.data()));

    grandChild2SubSurface->setPosition(QPoint(0, 25));

    // Let's map the surfaces.
    auto render = [this](Clt::Surface* s, const QSize& size) {
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        s->attachBuffer(m_shm->createBuffer(image));
        s->damage(QRect(QPoint(0, 0), size));
        s->commit(Clt::Surface::CommitFlag::None);
    };
    render(grandChild2Surface.data(), QSize(50, 50));
    render(grandChild1Surface.data(), QSize(50, 50));
    render(childSurface.data(), QSize(50, 100));
    render(parentSurface.data(), QSize(100, 100));

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface->isMapped());

    // Send in pointer events.
    QSignalSpy enteredSpy(pointer.data(), &Clt::Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer.data(), &Clt::Pointer::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy motionSpy(pointer.data(), &Clt::Pointer::motion);
    QVERIFY(motionSpy.isValid());

    // First to the grandChild2 in the overlapped area.
    quint32 timestamp = 1;
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setPointerPos(QPointF(25, 50));
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(motionSpy.count(), 0);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(25, 25));
    QCOMPARE(pointer->enteredSurface(), grandChild2Surface.data());

    // A motion on grandchild2.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setPointerPos(QPointF(25, 60));
    QVERIFY(motionSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(motionSpy.last().first().toPointF(), QPointF(25, 35));

    // Motion which changes to childSurface.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setPointerPos(QPointF(25, 80));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(25, 80));
    QCOMPARE(pointer->enteredSurface(), childSurface.data());

    // A leave for the whole surface.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setFocusedPointerSurface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 2);
    QCOMPARE(motionSpy.count(), 1);

    // A new enter on the main surface.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setPointerPos(QPointF(75, 50));
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 3);
    QCOMPARE(leftSpy.count(), 2);
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(75, 50));
    QCOMPARE(pointer->enteredSurface(), parentSurface.data());
}

void TestSeat::testPointerSwipeGesture_data()
{
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<int>("expectedEndCount");
    QTest::addColumn<int>("expectedCancelCount");

    QTest::newRow("end") << false << 1 << 0;
    QTest::newRow("cancel") << true << 0 << 1;
}

void TestSeat::testPointerSwipeGesture()
{
    // First create the pointer and pointer swipe gesture.
    QSignalSpy hasPointerChangedSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());

    m_serverSeat->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());
    QScopedPointer<Clt::PointerSwipeGesture> gesture(
        m_pointerGestures->createSwipeGesture(pointer.data()));
    QVERIFY(gesture);
    QVERIFY(gesture->isValid());
    QVERIFY(gesture->surface().isNull());
    QCOMPARE(gesture->fingerCount(), 0u);

    QSignalSpy startSpy(gesture.data(), &Clt::PointerSwipeGesture::started);
    QVERIFY(startSpy.isValid());
    QSignalSpy updateSpy(gesture.data(), &Clt::PointerSwipeGesture::updated);
    QVERIFY(updateSpy.isValid());
    QSignalSpy endSpy(gesture.data(), &Clt::PointerSwipeGesture::ended);
    QVERIFY(endSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &Clt::PointerSwipeGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    // Now create a surface.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedPointerSurface(), serverSurface);
    QVERIFY(m_serverSeat->focusedPointer());

    // Send in the start.
    quint32 timestamp = 1;
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->startPointerSwipeGesture(2);

    QVERIFY(startSpy.wait());
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(startSpy.first().at(1).value<quint32>(), 1u);
    QCOMPARE(gesture->fingerCount(), 2u);
    QCOMPARE(gesture->surface().data(), surface.data());

    // Another start should not be possible.
    m_serverSeat->startPointerSwipeGesture(2);
    QVERIFY(!startSpy.wait(200));

    // Send in some updates.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->updatePointerSwipeGesture(QSizeF(2, 3));

    QVERIFY(updateSpy.wait());
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->updatePointerSwipeGesture(QSizeF(4, 5));

    QVERIFY(updateSpy.wait());
    QCOMPARE(updateSpy.count(), 2);
    QCOMPARE(updateSpy.at(0).at(0).toSizeF(), QSizeF(2, 3));
    QCOMPARE(updateSpy.at(0).at(1).value<quint32>(), 2u);
    QCOMPARE(updateSpy.at(1).at(0).toSizeF(), QSizeF(4, 5));
    QCOMPARE(updateSpy.at(1).at(1).value<quint32>(), 3u);

    // Now end or cancel.
    QFETCH(bool, cancel);
    QSignalSpy* spy;

    m_serverSeat->setTimestamp(timestamp++);
    if (cancel) {
        m_serverSeat->cancelPointerSwipeGesture();
        spy = &cancelledSpy;
    } else {
        m_serverSeat->endPointerSwipeGesture();
        spy = &endSpy;
    }

    QVERIFY(spy->wait());
    QTEST(endSpy.count(), "expectedEndCount");
    QTEST(cancelledSpy.count(), "expectedCancelCount");

    QCOMPARE(spy->count(), 1);
    QCOMPARE(spy->first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(spy->first().at(1).value<quint32>(), 4u);

    QCOMPARE(gesture->fingerCount(), 0u);
    QVERIFY(gesture->surface().isNull());

    // Now a start should be possible again.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->startPointerSwipeGesture(2);
    QVERIFY(startSpy.wait());

    // Unsetting the focused pointer surface should not change anything.
    m_serverSeat->setFocusedPointerSurface(nullptr);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->updatePointerSwipeGesture(QSizeF(6, 7));
    QVERIFY(updateSpy.wait());
    // And end.
    m_serverSeat->setTimestamp(timestamp++);
    if (cancel) {
        m_serverSeat->cancelPointerSwipeGesture();
    } else {
        m_serverSeat->endPointerSwipeGesture();
    }
    QVERIFY(spy->wait());
}

void TestSeat::testPointerPinchGesture_data()
{
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<int>("expectedEndCount");
    QTest::addColumn<int>("expectedCancelCount");

    QTest::newRow("end") << false << 1 << 0;
    QTest::newRow("cancel") << true << 0 << 1;
}

void TestSeat::testPointerPinchGesture()
{
    // First create the pointer and pointer swipe gesture.
    QSignalSpy hasPointerChangedSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    m_serverSeat->setHasPointer(true);

    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());
    QScopedPointer<Clt::PointerPinchGesture> gesture(
        m_pointerGestures->createPinchGesture(pointer.data()));
    QVERIFY(gesture);
    QVERIFY(gesture->isValid());
    QVERIFY(gesture->surface().isNull());
    QCOMPARE(gesture->fingerCount(), 0u);

    QSignalSpy startSpy(gesture.data(), &Clt::PointerPinchGesture::started);
    QVERIFY(startSpy.isValid());
    QSignalSpy updateSpy(gesture.data(), &Clt::PointerPinchGesture::updated);
    QVERIFY(updateSpy.isValid());
    QSignalSpy endSpy(gesture.data(), &Clt::PointerPinchGesture::ended);
    QVERIFY(endSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &Clt::PointerPinchGesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    // Now create a surface.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedPointerSurface(), serverSurface);
    QVERIFY(m_serverSeat->focusedPointer());

    // Send in the start.
    quint32 timestamp = 1;
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->startPointerPinchGesture(3);

    QVERIFY(startSpy.wait());
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(startSpy.first().at(1).value<quint32>(), 1u);
    QCOMPARE(gesture->fingerCount(), 3u);
    QCOMPARE(gesture->surface().data(), surface.data());

    // Another start should not be possible.
    m_serverSeat->startPointerPinchGesture(3);
    QVERIFY(!startSpy.wait(200));

    // Send in some updates.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->updatePointerPinchGesture(QSizeF(2, 3), 2, 45);

    QVERIFY(updateSpy.wait());
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->updatePointerPinchGesture(QSizeF(4, 5), 1, 90);

    QVERIFY(updateSpy.wait());
    QCOMPARE(updateSpy.count(), 2);
    QCOMPARE(updateSpy.at(0).at(0).toSizeF(), QSizeF(2, 3));
    QCOMPARE(updateSpy.at(0).at(1).value<quint32>(), 2u);
    QCOMPARE(updateSpy.at(0).at(2).value<quint32>(), 45u);
    QCOMPARE(updateSpy.at(0).at(3).value<quint32>(), 2u);
    QCOMPARE(updateSpy.at(1).at(0).toSizeF(), QSizeF(4, 5));
    QCOMPARE(updateSpy.at(1).at(1).value<quint32>(), 1u);
    QCOMPARE(updateSpy.at(1).at(2).value<quint32>(), 90u);
    QCOMPARE(updateSpy.at(1).at(3).value<quint32>(), 3u);

    // Now end or cancel.
    QFETCH(bool, cancel);
    QSignalSpy* spy;

    m_serverSeat->setTimestamp(timestamp++);
    if (cancel) {
        m_serverSeat->cancelPointerPinchGesture();
        spy = &cancelledSpy;
    } else {
        m_serverSeat->endPointerPinchGesture();
        spy = &endSpy;
    }

    QVERIFY(spy->wait());
    QTEST(endSpy.count(), "expectedEndCount");
    QTEST(cancelledSpy.count(), "expectedCancelCount");
    QCOMPARE(spy->count(), 1);
    QCOMPARE(spy->first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(spy->first().at(1).value<quint32>(), 4u);

    QCOMPARE(gesture->fingerCount(), 0u);
    QVERIFY(gesture->surface().isNull());

    // Now a start should be possible again.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->startPointerPinchGesture(3);
    QVERIFY(startSpy.wait());

    // Unsetting the focused pointer surface should not change anything.
    m_serverSeat->setFocusedPointerSurface(nullptr);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->updatePointerPinchGesture(QSizeF(6, 7), 2, -45);

    QVERIFY(updateSpy.wait());

    // And end.
    m_serverSeat->setTimestamp(timestamp++);

    if (cancel) {
        m_serverSeat->cancelPointerPinchGesture();
    } else {
        m_serverSeat->endPointerPinchGesture();
    }

    QVERIFY(spy->wait());
}

void TestSeat::testPointerAxis()
{
    // First create the pointer.
    QSignalSpy hasPointerChangedSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    m_serverSeat->setHasPointer(true);

    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());
    QVERIFY(pointer);

    // Now create a surface.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedPointerSurface(), serverSurface);
    QVERIFY(m_serverSeat->focusedPointer());

    QSignalSpy frameSpy(pointer.data(), &Clt::Pointer::frame);
    QVERIFY(frameSpy.isValid());
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 1);

    // Let's scroll vertically.
    QSignalSpy axisSourceSpy(pointer.data(), &Clt::Pointer::axisSourceChanged);
    QVERIFY(axisSourceSpy.isValid());
    QSignalSpy axisSpy(pointer.data(), &Clt::Pointer::axisChanged);
    QVERIFY(axisSpy.isValid());
    QSignalSpy axisDiscreteSpy(pointer.data(), &Clt::Pointer::axisDiscreteChanged);
    QVERIFY(axisDiscreteSpy.isValid());
    QSignalSpy axisStoppedSpy(pointer.data(), &Clt::Pointer::axisStopped);
    QVERIFY(axisStoppedSpy.isValid());

    quint32 timestamp = 1;
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerAxisV5(Qt::Vertical, 10, 1, Srv::PointerAxisSource::Wheel);

    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 2);
    QCOMPARE(axisSourceSpy.count(), 1);

    QCOMPARE(axisSourceSpy.last().at(0).value<Clt::Pointer::AxisSource>(),
             Clt::Pointer::AxisSource::Wheel);
    QCOMPARE(axisDiscreteSpy.count(), 1);
    QCOMPARE(axisDiscreteSpy.last().at(0).value<Clt::Pointer::Axis>(),
             Clt::Pointer::Axis::Vertical);
    QCOMPARE(axisDiscreteSpy.last().at(1).value<qint32>(), 1);

    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(axisSpy.last().at(1).value<Clt::Pointer::Axis>(), Clt::Pointer::Axis::Vertical);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), 10.0);

    QCOMPARE(axisStoppedSpy.count(), 0);

    // Let's scroll using fingers.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerAxisV5(Qt::Horizontal, 42, 0, Srv::PointerAxisSource::Finger);

    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 3);

    QCOMPARE(axisSourceSpy.count(), 2);
    QCOMPARE(axisSourceSpy.last().at(0).value<Clt::Pointer::AxisSource>(),
             Clt::Pointer::AxisSource::Finger);

    QCOMPARE(axisDiscreteSpy.count(), 1);

    QCOMPARE(axisSpy.count(), 2);
    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(2));
    QCOMPARE(axisSpy.last().at(1).value<Clt::Pointer::Axis>(), Clt::Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), 42.0);

    QCOMPARE(axisStoppedSpy.count(), 0);

    // Lift the fingers off the device.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerAxisV5(Qt::Horizontal, 0, 0, Srv::PointerAxisSource::Finger);

    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 4);

    QCOMPARE(axisSourceSpy.count(), 3);
    QCOMPARE(axisSourceSpy.last().at(0).value<Clt::Pointer::AxisSource>(),
             Clt::Pointer::AxisSource::Finger);

    QCOMPARE(axisDiscreteSpy.count(), 1);
    QCOMPARE(axisSpy.count(), 2);

    QCOMPARE(axisStoppedSpy.count(), 1);
    QCOMPARE(axisStoppedSpy.last().at(0).value<quint32>(), 3);
    QCOMPARE(axisStoppedSpy.last().at(1).value<Clt::Pointer::Axis>(),
             Clt::Pointer::Axis::Horizontal);

    // If the device is unknown, no axis_source event should be sent.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerAxisV5(Qt::Horizontal, 42, 1, Srv::PointerAxisSource::Unknown);

    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 5);
    QCOMPARE(axisSourceSpy.count(), 3);

    QCOMPARE(axisDiscreteSpy.count(), 2);
    QCOMPARE(axisDiscreteSpy.last().at(0).value<Clt::Pointer::Axis>(),
             Clt::Pointer::Axis::Horizontal);
    QCOMPARE(axisDiscreteSpy.last().at(1).value<qint32>(), 1);

    QCOMPARE(axisSpy.count(), 3);
    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(4));
    QCOMPARE(axisSpy.last().at(1).value<Clt::Pointer::Axis>(), Clt::Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), 42.0);

    QCOMPARE(axisStoppedSpy.count(), 1);
}

void TestSeat::testKeyboardSubSurfaceTreeFromPointer()
{
    // This test verifies that when clicking on a sub-surface the keyboard focus passes to it.

    // First create the pointer.
    QSignalSpy hasPointerChangedSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    m_serverSeat->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());

    // And create keyboard.
    QSignalSpy hasKeyboardChangedSpy(m_seat, &Clt::Seat::hasKeyboardChanged);
    QVERIFY(hasKeyboardChangedSpy.isValid());
    m_serverSeat->setHasKeyboard(true);
    QVERIFY(hasKeyboardChangedSpy.wait());
    QScopedPointer<Clt::Keyboard> keyboard(m_seat->createKeyboard());

    // Create a sub surface tree.
    // Parent surface (100, 100) with one sub surface taking the half of it's size (50, 100)
    // which has two further children (50, 50) which are overlapping.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    QScopedPointer<Clt::Surface> parentSurface(m_compositor->createSurface());
    QScopedPointer<Clt::Surface> childSurface(m_compositor->createSurface());
    QScopedPointer<Clt::Surface> grandChild1Surface(m_compositor->createSurface());
    QScopedPointer<Clt::Surface> grandChild2Surface(m_compositor->createSurface());

    QScopedPointer<Clt::SubSurface> childSubSurface(
        m_subCompositor->createSubSurface(childSurface.data(), parentSurface.data()));
    QScopedPointer<Clt::SubSurface> grandChild1SubSurface(
        m_subCompositor->createSubSurface(grandChild1Surface.data(), childSurface.data()));
    QScopedPointer<Clt::SubSurface> grandChild2SubSurface(
        m_subCompositor->createSubSurface(grandChild2Surface.data(), childSurface.data()));

    grandChild2SubSurface->setPosition(QPoint(0, 25));

    // Let's map the surfaces.
    auto render = [this](Clt::Surface* s, const QSize& size) {
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        s->attachBuffer(m_shm->createBuffer(image));
        s->damage(QRect(QPoint(0, 0), size));
        s->commit(Clt::Surface::CommitFlag::None);
    };
    render(grandChild2Surface.data(), QSize(50, 50));
    render(grandChild1Surface.data(), QSize(50, 50));
    render(childSurface.data(), QSize(50, 100));
    render(parentSurface.data(), QSize(100, 100));

    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface->isMapped());

    // Pass keyboard focus to the main surface.
    QSignalSpy enterSpy(keyboard.data(), &Clt::Keyboard::entered);
    QVERIFY(enterSpy.isValid());
    QSignalSpy leftSpy(keyboard.data(), &Clt::Keyboard::left);
    QVERIFY(leftSpy.isValid());

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(enterSpy.wait());
    QCOMPARE(enterSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(keyboard->enteredSurface(), parentSurface.data());

    // Now pass also pointer focus to the surface.
    QSignalSpy pointerEnterSpy(pointer.data(), &Clt::Pointer::entered);
    QVERIFY(pointerEnterSpy.isValid());
    quint32 timestamp = 1;

    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setPointerPos(QPointF(25, 50));
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QVERIFY(pointerEnterSpy.wait());
    QCOMPARE(pointerEnterSpy.count(), 1);
    // Should not have affected the keyboard.
    QCOMPARE(enterSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);

    // Let's click.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerButtonPressed(Qt::LeftButton);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerButtonReleased(Qt::LeftButton);
    QVERIFY(enterSpy.wait());
    QCOMPARE(enterSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(keyboard->enteredSurface(), grandChild2Surface.data());

    // Click on same surface should not trigger another enter.
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerButtonPressed(Qt::LeftButton);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerButtonReleased(Qt::LeftButton);
    QVERIFY(!enterSpy.wait(200));
    QCOMPARE(enterSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(keyboard->enteredSurface(), grandChild2Surface.data());

    // Unfocus keyboard.
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(enterSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 2);
}

void TestSeat::testCursor()
{
    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    m_serverSeat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QScopedPointer<Clt::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    QSignalSpy enteredSpy(p.data(), &Clt::Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    m_serverSeat->setPointerPos(QPoint(20, 18));
    m_serverSeat->setFocusedPointerSurface(serverSurface, QPoint(10, 15));

    quint32 serial = m_display->serial();
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), serial);
    QVERIFY(m_serverSeat->focusedPointerSurface());
    QVERIFY(m_serverSeat->focusedPointer());
    QVERIFY(!m_serverSeat->focusedPointer()->cursor());

    QSignalSpy cursorChangedSpy(m_serverSeat->focusedPointer(), &Srv::Pointer::cursorChanged);
    QVERIFY(cursorChangedSpy.isValid());
    // Just remove the pointer.
    p->setCursor(nullptr);
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(cursorChangedSpy.count(), 1);
    auto cursor = m_serverSeat->focusedPointer()->cursor();
    QVERIFY(cursor);
    QVERIFY(!cursor->surface());
    QCOMPARE(cursor->hotspot(), QPoint());
    QCOMPARE(cursor->enteredSerial(), serial);
    QCOMPARE(cursor->pointer(), m_serverSeat->focusedPointer());

    QSignalSpy hotspotChangedSpy(cursor, &Srv::Cursor::hotspotChanged);
    QVERIFY(hotspotChangedSpy.isValid());
    QSignalSpy surfaceChangedSpy(cursor, &Srv::Cursor::surfaceChanged);
    QVERIFY(surfaceChangedSpy.isValid());
    QSignalSpy enteredSerialChangedSpy(cursor, &Srv::Cursor::enteredSerialChanged);
    QVERIFY(enteredSerialChangedSpy.isValid());
    QSignalSpy changedSpy(cursor, &Srv::Cursor::changed);
    QVERIFY(changedSpy.isValid());

    // Test changing hotspot.
    p->setCursor(nullptr, QPoint(1, 2));
    QVERIFY(hotspotChangedSpy.wait());
    QCOMPARE(hotspotChangedSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(cursorChangedSpy.count(), 2);
    QCOMPARE(cursor->hotspot(), QPoint(1, 2));
    QVERIFY(enteredSerialChangedSpy.isEmpty());
    QVERIFY(surfaceChangedSpy.isEmpty());

    // Set surface.
    auto cursorSurface = m_compositor->createSurface(m_compositor);
    QVERIFY(cursorSurface->isValid());
    p->setCursor(cursorSurface, QPoint(1, 2));
    QVERIFY(surfaceChangedSpy.wait());
    QCOMPARE(surfaceChangedSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(cursorChangedSpy.count(), 3);
    QVERIFY(enteredSerialChangedSpy.isEmpty());
    QCOMPARE(cursor->hotspot(), QPoint(1, 2));
    QVERIFY(cursor->surface());

    // And add an image to the surface.
    QImage img(QSize(10, 20), QImage::Format_RGB32);
    img.fill(Qt::red);
    cursorSurface->attachBuffer(m_shm->createBuffer(img));
    cursorSurface->damage(QRect(0, 0, 10, 20));
    cursorSurface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.count(), 3);
    QCOMPARE(cursorChangedSpy.count(), 4);
    QCOMPARE(surfaceChangedSpy.count(), 1);
    QCOMPARE(cursor->surface()->buffer()->data(), img);

    // And add another image to the surface.
    QImage blue(QSize(10, 20), QImage::Format_ARGB32_Premultiplied);
    blue.fill(Qt::blue);
    cursorSurface->attachBuffer(m_shm->createBuffer(blue));
    cursorSurface->damage(QRect(0, 0, 10, 20));
    cursorSurface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.count(), 4);
    QCOMPARE(cursorChangedSpy.count(), 5);
    QCOMPARE(cursor->surface()->buffer()->data(), blue);

    p->hideCursor();
    QVERIFY(surfaceChangedSpy.wait());
    QCOMPARE(changedSpy.count(), 5);
    QCOMPARE(cursorChangedSpy.count(), 6);
    QCOMPARE(surfaceChangedSpy.count(), 2);
    QVERIFY(!cursor->surface());
}

void TestSeat::testCursorDamage()
{
    // This test verifies that damaging a cursor surface triggers a cursor changed on the server.

    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    m_serverSeat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    // Create pointer.
    QScopedPointer<Clt::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    QSignalSpy enteredSpy(p.data(), &Clt::Pointer::entered);

    QVERIFY(enteredSpy.isValid());
    // Create surface.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    // Send enter to the surface.
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    QVERIFY(enteredSpy.wait());

    // Create a signal spy for the cursor changed signal.
    auto pointer = m_serverSeat->focusedPointer();
    QSignalSpy cursorChangedSpy(pointer, &Srv::Pointer::cursorChanged);
    QVERIFY(cursorChangedSpy.isValid());

    // Now let's set the cursor.
    auto* cursorSurface = m_compositor->createSurface(m_compositor);
    QVERIFY(cursorSurface);
    QImage red(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    red.fill(Qt::red);
    cursorSurface->attachBuffer(m_shm->createBuffer(red));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit(Clt::Surface::CommitFlag::None);
    p->setCursor(cursorSurface, QPoint(0, 0));
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(pointer->cursor()->surface()->buffer()->data(), red);

    // And damage the surface.
    QImage blue(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    blue.fill(Qt::blue);
    cursorSurface->attachBuffer(m_shm->createBuffer(blue));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(pointer->cursor()->surface()->buffer()->data(), blue);
}

void TestSeat::testKeyboard()
{
    QSignalSpy keyboardSpy(m_seat, &Clt::Seat::hasKeyboardChanged);
    QVERIFY(keyboardSpy.isValid());
    m_serverSeat->setHasKeyboard(true);
    QVERIFY(keyboardSpy.wait());

    // Create the surface.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    auto* s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);

    // No keyboard yet.
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);
    QVERIFY(!m_serverSeat->focusedKeyboard());

    auto* keyboard = m_seat->createKeyboard(m_seat);
    QSignalSpy repeatInfoSpy(keyboard, &Clt::Keyboard::keyRepeatChanged);
    QVERIFY(repeatInfoSpy.isValid());
    const Clt::Keyboard& ckeyboard = *keyboard;
    QVERIFY(keyboard->isValid());
    QCOMPARE(keyboard->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard->keyRepeatDelay(), 0);
    QCOMPARE(keyboard->keyRepeatRate(), 0);
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    auto serverKeyboard = m_serverSeat->focusedKeyboard();
    QVERIFY(serverKeyboard);

    // We should get the repeat info announced.
    QCOMPARE(repeatInfoSpy.count(), 1);
    QCOMPARE(keyboard->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard->keyRepeatDelay(), 0);
    QCOMPARE(keyboard->keyRepeatRate(), 0);

    // Let's change repeat in server.
    m_serverSeat->setKeyRepeatInfo(25, 660);
    m_serverSeat->focusedKeyboard()->client()->flush();
    QVERIFY(repeatInfoSpy.wait());
    QCOMPARE(repeatInfoSpy.count(), 2);
    QCOMPARE(keyboard->isKeyRepeatEnabled(), true);
    QCOMPARE(keyboard->keyRepeatRate(), 25);
    QCOMPARE(keyboard->keyRepeatDelay(), 660);

    m_serverSeat->setTimestamp(1);
    m_serverSeat->keyPressed(KEY_K);
    m_serverSeat->setTimestamp(2);
    m_serverSeat->keyPressed(KEY_D);
    m_serverSeat->setTimestamp(3);
    m_serverSeat->keyPressed(KEY_E);

    QSignalSpy modifierSpy(keyboard, &Clt::Keyboard::modifiersChanged);
    QVERIFY(modifierSpy.isValid());

    QSignalSpy enteredSpy(keyboard, &Clt::Keyboard::entered);
    QVERIFY(enteredSpy.isValid());
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboard()->focusedSurface(), serverSurface);

    // We get the modifiers sent after the enter.
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 1);
    QCOMPARE(modifierSpy.first().at(0).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(1).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(2).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(3).value<quint32>(), quint32(0));
    QCOMPARE(enteredSpy.count(), 1);

    // TODO: get through API
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial() - 1);

    QSignalSpy keyChangedSpy(keyboard, &Clt::Keyboard::keyChanged);
    QVERIFY(keyChangedSpy.isValid());

    m_serverSeat->setTimestamp(4);
    m_serverSeat->keyReleased(KEY_E);
    QVERIFY(keyChangedSpy.wait());
    m_serverSeat->setTimestamp(5);
    m_serverSeat->keyReleased(KEY_D);
    QVERIFY(keyChangedSpy.wait());
    m_serverSeat->setTimestamp(6);
    m_serverSeat->keyReleased(KEY_K);
    QVERIFY(keyChangedSpy.wait());
    m_serverSeat->setTimestamp(7);
    m_serverSeat->keyPressed(KEY_F1);
    QVERIFY(keyChangedSpy.wait());
    m_serverSeat->setTimestamp(8);
    m_serverSeat->keyReleased(KEY_F1);
    QVERIFY(keyChangedSpy.wait());

    QCOMPARE(keyChangedSpy.count(), 5);
    QCOMPARE(keyChangedSpy.at(0).at(0).value<quint32>(), quint32(KEY_E));
    QCOMPARE(keyChangedSpy.at(0).at(1).value<Clt::Keyboard::KeyState>(),
             Clt::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(0).at(2).value<quint32>(), quint32(4));
    QCOMPARE(keyChangedSpy.at(1).at(0).value<quint32>(), quint32(KEY_D));
    QCOMPARE(keyChangedSpy.at(1).at(1).value<Clt::Keyboard::KeyState>(),
             Clt::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(1).at(2).value<quint32>(), quint32(5));
    QCOMPARE(keyChangedSpy.at(2).at(0).value<quint32>(), quint32(KEY_K));
    QCOMPARE(keyChangedSpy.at(2).at(1).value<Clt::Keyboard::KeyState>(),
             Clt::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(2).at(2).value<quint32>(), quint32(6));
    QCOMPARE(keyChangedSpy.at(3).at(0).value<quint32>(), quint32(KEY_F1));
    QCOMPARE(keyChangedSpy.at(3).at(1).value<Clt::Keyboard::KeyState>(),
             Clt::Keyboard::KeyState::Pressed);
    QCOMPARE(keyChangedSpy.at(3).at(2).value<quint32>(), quint32(7));
    QCOMPARE(keyChangedSpy.at(4).at(0).value<quint32>(), quint32(KEY_F1));
    QCOMPARE(keyChangedSpy.at(4).at(1).value<Clt::Keyboard::KeyState>(),
             Clt::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(4).at(2).value<quint32>(), quint32(8));

    // Releasing a key which is already released should not set a key changed.
    m_serverSeat->keyReleased(KEY_F1);
    QVERIFY(!keyChangedSpy.wait(200));

    // Let's press it again.
    m_serverSeat->keyPressed(KEY_F1);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 6);

    // Press again should be ignored.
    m_serverSeat->keyPressed(KEY_F1);
    QVERIFY(!keyChangedSpy.wait(200));

    // And release.
    m_serverSeat->keyReleased(KEY_F1);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 7);

    m_serverSeat->updateKeyboardModifiers(1, 2, 3, 4);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 2);
    QCOMPARE(modifierSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(modifierSpy.last().at(1).value<quint32>(), quint32(2));
    QCOMPARE(modifierSpy.last().at(2).value<quint32>(), quint32(3));
    QCOMPARE(modifierSpy.last().at(3).value<quint32>(), quint32(4));

    QSignalSpy leftSpy(keyboard, &Clt::Keyboard::left);
    QVERIFY(leftSpy.isValid());
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    QVERIFY(!m_serverSeat->focusedKeyboardSurface());
    QVERIFY(!m_serverSeat->focusedKeyboard());
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);

    // TODO: get through API
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial() - 1);

    QVERIFY(!keyboard->enteredSurface());
    QVERIFY(!ckeyboard.enteredSurface());

    // Enter it again.
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboard()->focusedSurface(), serverSurface);
    QCOMPARE(enteredSpy.count(), 2);

    QCOMPARE(keyboard->enteredSurface(), s);
    QCOMPARE(ckeyboard.enteredSurface(), s);

    QSignalSpy serverSurfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(serverSurfaceDestroyedSpy.isValid());
    QCOMPARE(keyboard->enteredSurface(), s);
    delete s;
    QVERIFY(!keyboard->enteredSurface());

    QVERIFY(leftSpy.wait());
    QCOMPARE(serverSurfaceDestroyedSpy.count(), 1);
    QVERIFY(!m_serverSeat->focusedKeyboardSurface());
    QVERIFY(!m_serverSeat->focusedKeyboard());
    QVERIFY(!serverKeyboard->focusedSurface());

    // Let's create a Surface again.
    QScopedPointer<Clt::Surface> s2(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 2);
    serverSurface = surfaceCreatedSpy.last().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboard(), serverKeyboard);

    // Delete the Keyboard.
    QSignalSpy destroyedSpy(serverKeyboard, &Srv::Keyboard::destroyed);
    QVERIFY(destroyedSpy.isValid());

    delete keyboard;
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(destroyedSpy.count(), 1);

    // Verify that calling into the Keyboard related functionality doesn't crash.
    m_serverSeat->setTimestamp(9);
    m_serverSeat->keyPressed(KEY_F2);
    m_serverSeat->setTimestamp(10);
    m_serverSeat->keyReleased(KEY_F2);
    m_serverSeat->setKeyRepeatInfo(30, 560);
    m_serverSeat->setKeyRepeatInfo(25, 660);
    m_serverSeat->updateKeyboardModifiers(5, 6, 7, 8);
    m_serverSeat->setKeymap(open("/dev/null", O_RDONLY), 0);
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);
    QVERIFY(!m_serverSeat->focusedKeyboard());

    // Create a second Keyboard to verify that repeat info is announced properly.
    auto* keyboard2 = m_seat->createKeyboard(m_seat);
    QSignalSpy repeatInfoSpy2(keyboard2, &Clt::Keyboard::keyRepeatChanged);
    QVERIFY(repeatInfoSpy2.isValid());
    QVERIFY(keyboard2->isValid());

    QCOMPARE(keyboard2->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard2->keyRepeatDelay(), 0);
    QCOMPARE(keyboard2->keyRepeatRate(), 0);

    wl_display_flush(m_connection->display());

    QVERIFY(repeatInfoSpy2.wait());
    QCOMPARE(keyboard2->isKeyRepeatEnabled(), true);
    QCOMPARE(keyboard2->keyRepeatRate(), 25);
    QCOMPARE(keyboard2->keyRepeatDelay(), 660);
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);

    serverKeyboard = m_serverSeat->focusedKeyboard();

    QVERIFY(serverKeyboard);
    QSignalSpy keyboard2DestroyedSpy(serverKeyboard, &Srv::Keyboard::destroyed);
    QVERIFY(keyboard2DestroyedSpy.isValid());

    delete keyboard2;
    QVERIFY(keyboard2DestroyedSpy.wait());

    // This should have unset it on the server.
    QVERIFY(!m_serverSeat->focusedKeyboard());

    // But not the surface.
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);
}

void TestSeat::testCast()
{
    Clt::Registry registry;
    QSignalSpy seatSpy(&registry, &Clt::Registry::seatAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(seatSpy.wait());
    Clt::Seat s;
    QVERIFY(!s.isValid());
    auto wlSeat = registry.bindSeat(seatSpy.first().first().value<quint32>(),
                                    seatSpy.first().last().value<quint32>());
    QVERIFY(wlSeat);
    s.setup(wlSeat);
    QVERIFY(s.isValid());

    QCOMPARE((wl_seat*)s, wlSeat);
    const Clt::Seat& s2(s);
    QCOMPARE((wl_seat*)s2, wlSeat);
}

void TestSeat::testDestroy()
{

    QSignalSpy keyboardSpy(m_seat, &Clt::Seat::hasKeyboardChanged);
    QVERIFY(keyboardSpy.isValid());
    m_serverSeat->setHasKeyboard(true);
    QVERIFY(keyboardSpy.wait());
    auto* k = m_seat->createKeyboard(m_seat);
    QVERIFY(k->isValid());

    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    m_serverSeat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());
    auto* p = m_seat->createPointer(m_seat);
    QVERIFY(p->isValid());

    QSignalSpy touchSpy(m_seat, &Clt::Seat::hasTouchChanged);
    QVERIFY(touchSpy.isValid());
    m_serverSeat->setHasTouch(true);
    QVERIFY(touchSpy.wait());
    auto* t = m_seat->createTouch(m_seat);
    QVERIFY(t->isValid());

    delete m_compositor;
    m_compositor = nullptr;

    connect(m_connection, &Clt::ConnectionThread::establishedChanged, m_seat, &Clt::Seat::release);
    connect(
        m_connection, &Clt::ConnectionThread::establishedChanged, m_shm, &Clt::ShmPool::release);
    connect(m_connection,
            &Clt::ConnectionThread::establishedChanged,
            m_subCompositor,
            &Clt::SubCompositor::release);
    connect(m_connection,
            &Clt::ConnectionThread::establishedChanged,
            m_relativePointerManager,
            &Clt::RelativePointerManager::release);
    connect(m_connection,
            &Clt::ConnectionThread::establishedChanged,
            m_pointerGestures,
            &Clt::PointerGestures::release);
    connect(m_connection,
            &Clt::ConnectionThread::establishedChanged,
            m_queue,
            &Clt::EventQueue::release);

    QVERIFY(m_seat->isValid());

    QSignalSpy connectionDiedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectionDiedSpy.isValid());

    delete m_display;
    m_display = nullptr;
    m_compositorInterface = nullptr;
    m_serverSeat = nullptr;
    m_subCompositorInterface = nullptr;
    m_relativePointerManagerServer = nullptr;
    m_pointerGesturesInterface = nullptr;
    QTRY_COMPARE(connectionDiedSpy.count(), 1);

    // Now the seat should be destroyed.
    QTRY_VERIFY(!m_seat->isValid());
    QTRY_VERIFY(!k->isValid());
    QTRY_VERIFY(!p->isValid());
    QTRY_VERIFY(!t->isValid());

    // Calling release again should not fail.
    delete k;
    delete p;
    delete t;
}

void TestSeat::testSelection()
{
    QScopedPointer<Srv::DataDeviceManagerInterface> ddmi(m_display->createDataDeviceManager());
    ddmi->create();

    Clt::Registry registry;
    QSignalSpy dataDeviceManagerSpy(&registry, &Clt::Registry::dataDeviceManagerAnnounced);
    QVERIFY(dataDeviceManagerSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(dataDeviceManagerSpy.wait());
    QScopedPointer<Clt::DataDeviceManager> ddm(
        registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(),
                                         dataDeviceManagerSpy.first().last().value<quint32>()));
    QVERIFY(ddm->isValid());

    QScopedPointer<Clt::DataDevice> dd1(ddm->getDataDevice(m_seat));
    QVERIFY(dd1->isValid());
    QSignalSpy selectionSpy(dd1.data(), &Clt::DataDevice::selectionOffered);
    QVERIFY(selectionSpy.isValid());
    QSignalSpy selectionClearedSpy(dd1.data(), &Clt::DataDevice::selectionCleared);
    QVERIFY(selectionClearedSpy.isValid());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(!m_serverSeat->selection());

    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);
    QVERIFY(!m_serverSeat->focusedKeyboard());
    QVERIFY(selectionClearedSpy.wait());
    QVERIFY(selectionSpy.isEmpty());
    QVERIFY(!selectionClearedSpy.isEmpty());

    selectionClearedSpy.clear();
    QVERIFY(!m_serverSeat->selection());

    // Now let's try to set a selection - we have keyboard focus, so it should be sent to us.
    QScopedPointer<Clt::DataSource> ds(ddm->createDataSource());
    QVERIFY(ds->isValid());

    ds->offer(QStringLiteral("text/plain"));
    dd1->setSelection(0, ds.data());

    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 1);
    auto ddi = m_serverSeat->selection();
    QVERIFY(ddi);

    auto df = selectionSpy.first().first().value<Clt::DataOffer*>();
    QCOMPARE(df->offeredMimeTypes().count(), 1);
    QCOMPARE(df->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // Try to clear.
    dd1->setSelection(0);
    QVERIFY(selectionClearedSpy.wait());
    QCOMPARE(selectionClearedSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 1);

    // Unset the keyboard focus.
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    QVERIFY(!m_serverSeat->focusedKeyboardSurface());
    QVERIFY(!m_serverSeat->focusedKeyboard());

    serverSurface->client()->flush();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Try to set Selection.
    dd1->setSelection(0, ds.data());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(selectionSpy.count(), 1);

    // Let's unset the selection on the seat.
    m_serverSeat->setSelection(nullptr);

    // And pass focus back on our surface.
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);

    // We don't have a selection, so it should not send a selection.
    QVERIFY(!selectionSpy.wait(100));

    // Now let's set it manually.
    m_serverSeat->setSelection(ddi);
    QCOMPARE(m_serverSeat->selection(), ddi);
    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 2);

    // Setting the same again should not change.
    m_serverSeat->setSelection(ddi);
    QVERIFY(!selectionSpy.wait(100));

    // Now clear it manually.
    m_serverSeat->setSelection(nullptr);
    QVERIFY(selectionClearedSpy.wait());
    QCOMPARE(selectionSpy.count(), 2);

    // Create a second ddi and a data source.
    QScopedPointer<Clt::DataDevice> dd2(ddm->getDataDevice(m_seat));
    QVERIFY(dd2->isValid());
    QScopedPointer<Clt::DataSource> ds2(ddm->createDataSource());
    QVERIFY(ds2->isValid());
    ds2->offer(QStringLiteral("text/plain"));
    dd2->setSelection(0, ds2.data());
    QVERIFY(selectionSpy.wait());
    QSignalSpy cancelledSpy(ds2.data(), &Clt::DataSource::cancelled);
    QVERIFY(cancelledSpy.isValid());
    m_serverSeat->setSelection(ddi);
    QVERIFY(cancelledSpy.wait());

    // Copy already cleared selection, BUG 383054.
    ddi->sendSelection(ddi);
}

void TestSeat::testSelectionNoDataSource()
{
    // This test verifies that the server doesn't crash when using setSelection with
    // a DataDevice which doesn't have a DataSource yet.

    // First create the DataDevice.
    QScopedPointer<Srv::DataDeviceManagerInterface> ddmi(m_display->createDataDeviceManager());
    ddmi->create();
    QSignalSpy ddiCreatedSpy(ddmi.data(), &Srv::DataDeviceManagerInterface::dataDeviceCreated);
    QVERIFY(ddiCreatedSpy.isValid());

    Clt::Registry registry;
    QSignalSpy dataDeviceManagerSpy(&registry, &Clt::Registry::dataDeviceManagerAnnounced);
    QVERIFY(dataDeviceManagerSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(dataDeviceManagerSpy.wait());
    QScopedPointer<Clt::DataDeviceManager> ddm(
        registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(),
                                         dataDeviceManagerSpy.first().last().value<quint32>()));
    QVERIFY(ddm->isValid());

    QScopedPointer<Clt::DataDevice> dd(ddm->getDataDevice(m_seat));
    QVERIFY(dd->isValid());
    QVERIFY(ddiCreatedSpy.wait());
    auto* ddi = ddiCreatedSpy.first().first().value<Srv::DataDeviceInterface*>();
    QVERIFY(ddi);

    // Now create a surface and pass it keyboard focus.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(!m_serverSeat->selection());
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);

    // Now let's set the selection.
    m_serverSeat->setSelection(ddi);
}

void TestSeat::testDataDeviceForKeyboardSurface()
{
    // This test verifies that the server does not crash when creating a datadevice for the focused
    // keyboard surface and the currentSelection does not have a DataSource.
    // To properly test the functionality this test requires a second client.

    // Create the DataDeviceManager.
    QScopedPointer<Srv::DataDeviceManagerInterface> ddmi(m_display->createDataDeviceManager());
    ddmi->create();
    QSignalSpy ddiCreatedSpy(ddmi.data(), &Srv::DataDeviceManagerInterface::dataDeviceCreated);
    QVERIFY(ddiCreatedSpy.isValid());

    // Create a second Wayland client connection to use it for setSelection.
    auto c = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(c, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    c->setSocketName(s_socketName);

    auto thread = new QThread(this);
    c->moveToThread(thread);
    thread->start();

    c->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    QScopedPointer<Clt::EventQueue> queue(new Clt::EventQueue);
    queue->setup(c);

    QScopedPointer<Clt::Registry> registry(new Clt::Registry);
    QSignalSpy interfacesAnnouncedSpy(registry.data(), &Clt::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry->setEventQueue(queue.data());
    registry->create(c);
    QVERIFY(registry->isValid());
    registry->setup();

    QVERIFY(interfacesAnnouncedSpy.wait());
    QScopedPointer<Clt::Seat> seat(
        registry->createSeat(registry->interface(Clt::Registry::Interface::Seat).name,
                             registry->interface(Clt::Registry::Interface::Seat).version));
    QVERIFY(seat->isValid());
    QScopedPointer<Clt::DataDeviceManager> ddm1(registry->createDataDeviceManager(
        registry->interface(Clt::Registry::Interface::DataDeviceManager).name,
        registry->interface(Clt::Registry::Interface::DataDeviceManager).version));
    QVERIFY(ddm1->isValid());

    // Now create our first datadevice.
    QScopedPointer<Clt::DataDevice> dd1(ddm1->getDataDevice(seat.data()));
    QVERIFY(ddiCreatedSpy.wait());
    auto* ddi = ddiCreatedSpy.first().first().value<Srv::DataDeviceInterface*>();
    QVERIFY(ddi);
    m_serverSeat->setSelection(ddi);

    // Switch to other client.
    // Create a surface and pass it keyboard focus.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_serverSeat->focusedKeyboardSurface(), serverSurface);

    // Now create a DataDevice.
    Clt::Registry registry2;
    QSignalSpy dataDeviceManagerSpy(&registry2, &Clt::Registry::dataDeviceManagerAnnounced);
    QVERIFY(dataDeviceManagerSpy.isValid());
    registry2.setEventQueue(m_queue);
    registry2.create(m_connection->display());
    QVERIFY(registry2.isValid());
    registry2.setup();

    QVERIFY(dataDeviceManagerSpy.wait());
    QScopedPointer<Clt::DataDeviceManager> ddm(
        registry2.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(),
                                          dataDeviceManagerSpy.first().last().value<quint32>()));
    QVERIFY(ddm->isValid());

    QScopedPointer<Clt::DataDevice> dd(ddm->getDataDevice(m_seat));
    QVERIFY(dd->isValid());
    QVERIFY(ddiCreatedSpy.wait());

    // Unset surface and set again.
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);

    // And delete the connection thread again.
    dd1.reset();
    ddm1.reset();
    seat.reset();
    registry.reset();
    queue.reset();
    c->deleteLater();
    thread->quit();
    thread->wait();
    delete thread;
}

void TestSeat::testTouch()
{
    QSignalSpy touchSpy(m_seat, &Clt::Seat::hasTouchChanged);
    QVERIFY(touchSpy.isValid());
    m_serverSeat->setHasTouch(true);
    QVERIFY(touchSpy.wait());

    // Create the surface.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    auto* s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    m_serverSeat->setFocusedTouchSurface(serverSurface);
    // No keyboard yet.
    QCOMPARE(m_serverSeat->focusedTouchSurface(), serverSurface);
    QVERIFY(!m_serverSeat->focusedTouch());

    QSignalSpy touchCreatedSpy(m_serverSeat, &Srv::Seat::touchCreated);
    QVERIFY(touchCreatedSpy.isValid());
    auto* touch = m_seat->createTouch(m_seat);
    QVERIFY(touch->isValid());
    QVERIFY(touchCreatedSpy.wait());
    auto serverTouch = m_serverSeat->focusedTouch();
    QVERIFY(serverTouch);
    QCOMPARE(touchCreatedSpy.first().first().value<Srv::TouchInterface*>(),
             m_serverSeat->focusedTouch());

    QSignalSpy sequenceStartedSpy(touch, &Clt::Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy sequenceEndedSpy(touch, &Clt::Touch::sequenceEnded);
    QVERIFY(sequenceEndedSpy.isValid());
    QSignalSpy sequenceCanceledSpy(touch, &Clt::Touch::sequenceCanceled);
    QVERIFY(sequenceCanceledSpy.isValid());
    QSignalSpy frameEndedSpy(touch, &Clt::Touch::frameEnded);
    QVERIFY(frameEndedSpy.isValid());
    QSignalSpy pointAddedSpy(touch, &Clt::Touch::pointAdded);
    QVERIFY(pointAddedSpy.isValid());
    QSignalSpy pointMovedSpy(touch, &Clt::Touch::pointMoved);
    QVERIFY(pointMovedSpy.isValid());
    QSignalSpy pointRemovedSpy(touch, &Clt::Touch::pointRemoved);
    QVERIFY(pointRemovedSpy.isValid());

    // Try a few things.
    m_serverSeat->setFocusedTouchSurfacePosition(QPointF(10, 20));
    QCOMPARE(m_serverSeat->focusedTouchSurfacePosition(), QPointF(10, 20));
    m_serverSeat->setTimestamp(1);
    QCOMPARE(m_serverSeat->touchDown(QPointF(15, 26)), 0);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 0);
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 0);
    QCOMPARE(pointRemovedSpy.count(), 0);
    auto* tp = sequenceStartedSpy.first().first().value<Clt::TouchPoint*>();
    QVERIFY(tp);
    QCOMPARE(tp->downSerial(), m_display->serial());
    QCOMPARE(tp->id(), 0);
    QVERIFY(tp->isDown());
    QCOMPARE(tp->position(), QPointF(5, 6));
    QCOMPARE(tp->positions().size(), 1);
    QCOMPARE(tp->time(), 1u);
    QCOMPARE(tp->timestamps().count(), 1);
    QCOMPARE(tp->upSerial(), 0u);
    QCOMPARE(tp->surface().data(), s);
    QCOMPARE(touch->sequence().count(), 1);
    QCOMPARE(touch->sequence().first(), tp);

    // Let's end the frame.
    m_serverSeat->touchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(frameEndedSpy.count(), 1);

    // Move the one point.
    m_serverSeat->setTimestamp(2);
    m_serverSeat->touchMove(0, QPointF(10, 20));
    m_serverSeat->touchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 2);
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.first().first().value<Clt::TouchPoint*>(), tp);

    QCOMPARE(tp->id(), 0);
    QVERIFY(tp->isDown());
    QCOMPARE(tp->position(), QPointF(0, 0));
    QCOMPARE(tp->positions().size(), 2);
    QCOMPARE(tp->time(), 2u);
    QCOMPARE(tp->timestamps().count(), 2);
    QCOMPARE(tp->upSerial(), 0u);
    QCOMPARE(tp->surface().data(), s);

    // Add onther point.
    m_serverSeat->setTimestamp(3);
    QCOMPARE(m_serverSeat->touchDown(QPointF(15, 26)), 1);
    m_serverSeat->touchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 3);
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 0);
    QCOMPARE(touch->sequence().count(), 2);
    QCOMPARE(touch->sequence().first(), tp);

    auto* tp2 = pointAddedSpy.first().first().value<Clt::TouchPoint*>();
    QVERIFY(tp2);
    QCOMPARE(touch->sequence().last(), tp2);
    QCOMPARE(tp2->id(), 1);
    QVERIFY(tp2->isDown());
    QCOMPARE(tp2->position(), QPointF(5, 6));
    QCOMPARE(tp2->positions().size(), 1);
    QCOMPARE(tp2->time(), 3u);
    QCOMPARE(tp2->timestamps().count(), 1);
    QCOMPARE(tp2->upSerial(), 0u);
    QCOMPARE(tp2->surface().data(), s);

    // Send it an up.
    m_serverSeat->setTimestamp(4);
    m_serverSeat->touchUp(1);
    m_serverSeat->touchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 4);
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.first().first().value<Clt::TouchPoint*>(), tp2);
    QCOMPARE(tp2->id(), 1);
    QVERIFY(!tp2->isDown());
    QCOMPARE(tp2->position(), QPointF(5, 6));
    QCOMPARE(tp2->positions().size(), 1);
    QCOMPARE(tp2->time(), 4u);
    QCOMPARE(tp2->timestamps().count(), 2);
    QCOMPARE(tp2->upSerial(), m_display->serial());
    QCOMPARE(tp2->surface().data(), s);

    // Send another down and up.
    m_serverSeat->setTimestamp(5);
    QCOMPARE(m_serverSeat->touchDown(QPointF(15, 26)), 1);
    m_serverSeat->touchFrame();
    m_serverSeat->setTimestamp(6);
    m_serverSeat->touchUp(1);

    // And send an up for the first point.
    m_serverSeat->touchUp(0);
    m_serverSeat->touchFrame();
    QVERIFY(frameEndedSpy.wait());
    QTRY_COMPARE(sequenceStartedSpy.count(), 1);
    QTRY_COMPARE(sequenceEndedSpy.count(), 1);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 6);
    QTRY_COMPARE(pointAddedSpy.count(), 2);
    QCOMPARE(pointMovedSpy.count(), 1);
    QTRY_COMPARE(pointRemovedSpy.count(), 3);
    QCOMPARE(touch->sequence().count(), 3);
    QVERIFY(!touch->sequence().at(0)->isDown());
    QVERIFY(!touch->sequence().at(1)->isDown());
    QVERIFY(!touch->sequence().at(2)->isDown());
    QVERIFY(!m_serverSeat->isTouchSequence());

    // Try cancel.
    m_serverSeat->setFocusedTouchSurface(serverSurface, QPointF(15, 26));
    m_serverSeat->setTimestamp(7);
    QCOMPARE(m_serverSeat->touchDown(QPointF(15, 26)), 0);
    m_serverSeat->touchFrame();
    m_serverSeat->cancelTouchSequence();
    QVERIFY(sequenceCanceledSpy.wait());
    QTRY_COMPARE(sequenceStartedSpy.count(), 2);
    QCOMPARE(sequenceEndedSpy.count(), 1);
    QCOMPARE(sequenceCanceledSpy.count(), 1);
    QCOMPARE(frameEndedSpy.count(), 7);
    QCOMPARE(pointAddedSpy.count(), 2);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 3);
    QCOMPARE(touch->sequence().first()->position(), QPointF(0, 0));

    // Destroy touch on client side.
    QSignalSpy unboundSpy(serverTouch, &Srv::TouchInterface::unbound);
    QVERIFY(unboundSpy.isValid());
    QSignalSpy destroyedSpy(serverTouch, &Srv::TouchInterface::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete touch;
    QVERIFY(unboundSpy.wait());
    QCOMPARE(unboundSpy.count(), 1);
    QCOMPARE(destroyedSpy.count(), 0);
    QVERIFY(!serverTouch->resource());

    // Try to call into all the methods of the touch interface, should not crash.
    QCOMPARE(m_serverSeat->focusedTouch(), serverTouch);
    m_serverSeat->setTimestamp(8);
    QCOMPARE(m_serverSeat->touchDown(QPointF(15, 26)), 0);
    m_serverSeat->touchFrame();
    m_serverSeat->touchMove(0, QPointF(0, 0));
    QCOMPARE(m_serverSeat->touchDown(QPointF(15, 26)), 1);
    m_serverSeat->cancelTouchSequence();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(destroyedSpy.count(), 1);

    // Should have unset the focused touch.
    QVERIFY(!m_serverSeat->focusedTouch());

    // But not the focused touch surface.
    QCOMPARE(m_serverSeat->focusedTouchSurface(), serverSurface);
}

void TestSeat::testDisconnect()
{
    // This test verifies that disconnecting the client cleans up correctly.
    QSignalSpy keyboardCreatedSpy(m_serverSeat, &Srv::Seat::keyboardCreated);
    QVERIFY(keyboardCreatedSpy.isValid());
    QSignalSpy pointerCreatedSpy(m_serverSeat, &Srv::Seat::pointerCreated);
    QVERIFY(pointerCreatedSpy.isValid());
    QSignalSpy touchCreatedSpy(m_serverSeat, &Srv::Seat::touchCreated);
    QVERIFY(touchCreatedSpy.isValid());

    // Create the things we need.
    m_serverSeat->setHasKeyboard(true);
    m_serverSeat->setHasPointer(true);
    m_serverSeat->setHasTouch(true);
    QSignalSpy touchSpy(m_seat, &Clt::Seat::hasTouchChanged);
    QVERIFY(touchSpy.isValid());
    QVERIFY(touchSpy.wait());

    QScopedPointer<Clt::Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(!keyboard.isNull());
    QVERIFY(keyboardCreatedSpy.wait());
    auto serverKeyboard = keyboardCreatedSpy.first().first().value<Srv::Keyboard*>();
    QVERIFY(serverKeyboard);

    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    QVERIFY(pointerCreatedSpy.wait());
    auto serverPointer = pointerCreatedSpy.first().first().value<Srv::Pointer*>();
    QVERIFY(serverPointer);

    QScopedPointer<Clt::Touch> touch(m_seat->createTouch());
    QVERIFY(!touch.isNull());
    QVERIFY(touchCreatedSpy.wait());
    auto serverTouch = touchCreatedSpy.first().first().value<Srv::TouchInterface*>();
    QVERIFY(serverTouch);

    // Setup destroys.
    QSignalSpy keyboardDestroyedSpy(serverKeyboard, &QObject::destroyed);
    QVERIFY(keyboardDestroyedSpy.isValid());
    QSignalSpy pointerDestroyedSpy(serverPointer, &QObject::destroyed);
    QVERIFY(pointerDestroyedSpy.isValid());
    QSignalSpy touchDestroyedSpy(serverTouch, &QObject::destroyed);
    QVERIFY(touchDestroyedSpy.isValid());
    QSignalSpy clientDisconnectedSpy(serverKeyboard->client(), &Srv::Client::disconnected);
    QVERIFY(clientDisconnectedSpy.isValid());

    keyboard->release();
    pointer->release();
    touch->release();
    m_relativePointerManager->release();
    m_pointerGestures->release();
    m_compositor->release();
    m_seat->release();
    m_shm->release();
    m_subCompositor->release();
    m_queue->release();

    QCOMPARE(keyboardDestroyedSpy.count(), 0);
    QCOMPARE(pointerDestroyedSpy.count(), 0);
    QCOMPARE(touchDestroyedSpy.count(), 0);

    QVERIFY(m_connection);
    m_connection->deleteLater();
    m_connection = nullptr;

    QVERIFY(clientDisconnectedSpy.wait());
    QCOMPARE(clientDisconnectedSpy.count(), 1);

    QTRY_COMPARE(keyboardDestroyedSpy.count(), 1);
    QTRY_COMPARE(pointerDestroyedSpy.count(), 1);
    QTRY_COMPARE(touchDestroyedSpy.count(), 1);
}

void TestSeat::testPointerEnterOnUnboundSurface()
{
    // Create the things we need.
    m_serverSeat->setHasKeyboard(true);
    m_serverSeat->setHasPointer(true);
    m_serverSeat->setHasTouch(true);
    QSignalSpy pointerChangedSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerChangedSpy.isValid());
    QVERIFY(pointerChangedSpy.wait());

    // Create pointer and Surface.
    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());

    // Create the surface.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &Srv::CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> s(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::SurfaceInterface*>();
    QVERIFY(serverSurface);

    // Unbind the surface again.
    QSignalSpy serverPointerChangedSpy(m_serverSeat, &Srv::Seat::focusedPointerChanged);
    QVERIFY(serverPointerChangedSpy.isValid());
    QSignalSpy surfaceUnboundSpy(serverSurface, &Srv::SurfaceInterface::unbound);
    QVERIFY(surfaceUnboundSpy.isValid());
    s.reset();
    QVERIFY(surfaceUnboundSpy.wait());

    m_serverSeat->setFocusedPointerSurface(serverSurface);

    QVERIFY(!pointerChangedSpy.wait(200));
    QCOMPARE(serverPointerChangedSpy.count(), 2);
    QVERIFY(!m_serverSeat->focusedPointerSurface());
}

QTEST_GUILESS_MAIN(TestSeat)
#include "seat.moc"
