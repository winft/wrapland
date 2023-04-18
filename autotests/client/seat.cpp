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

#include "../../server/buffer.h"
#include "../../server/client.h"
#include "../../server/compositor.h"
#include "../../server/data_device.h"
#include "../../server/data_device_manager.h"
#include "../../server/data_source.h"
#include "../../server/display.h"
#include "../../server/keyboard.h"
#include "../../server/keyboard_pool.h"
#include "../../server/pointer_gestures_v1.h"
#include "../../server/pointer_pool.h"
#include "../../server/relative_pointer_v1.h"
#include "../../server/seat.h"
#include "../../server/subcompositor.h"
#include "../../server/surface.h"
#include "../../server/touch.h"
#include "../../server/touch_pool.h"

#include "../../tests/globals.h"

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

    void testPointerAxis();
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
    void testKeymap();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

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

constexpr auto socket_name{"wrapland-test-wayland-seat-0"};

TestSeat::TestSeat(QObject* parent)
    : QObject(parent)
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
    qRegisterMetaType<Wrapland::Server::data_device*>();
    qRegisterMetaType<Wrapland::Server::Keyboard*>();
    qRegisterMetaType<Wrapland::Server::Pointer*>();
    qRegisterMetaType<Wrapland::Server::Touch*>();
    qRegisterMetaType<Wrapland::Server::Surface*>();
}

void TestSeat::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());
    server.globals.subcompositor
        = std::make_unique<Wrapland::Server::Subcompositor>(server.display.get());
    server.globals.relative_pointer_manager_v1
        = std::make_unique<Wrapland::Server::RelativePointerManagerV1>(server.display.get());
    server.globals.pointer_gestures_v1
        = std::make_unique<Wrapland::Server::PointerGesturesV1>(server.display.get());

    // Setup connection.
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    m_connection->setSocketName(socket_name);

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

    server.globals.seats.emplace_back(
        std::make_unique<Wrapland::Server::Seat>(server.display.get()));
    server.seat = server.globals.seats.back().get();
    server.seat->setName("seat0");

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

    server = {};
}

void TestSeat::testName()
{
    // No name set yet.
    QCOMPARE(m_seat->name(), QStringLiteral("seat0"));

    QSignalSpy spy(m_seat, &Clt::Seat::nameChanged);
    QVERIFY(spy.isValid());

    const std::string name("foobar");
    server.seat->setName(name);
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

    server.seat->setHasPointer(pointer);
    server.seat->setHasKeyboard(keyboard);
    server.seat->setHasTouch(touch);

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
    server.seat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    auto s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());

    Srv::Surface* serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    QSignalSpy focusedPointerChangedSpy(server.seat, &Srv::Seat::focusedPointerChanged);
    QVERIFY(focusedPointerChangedSpy.isValid());

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_position(QPoint(20, 18));
    server_pointers.set_focused_surface(serverSurface, QPoint(10, 15));
    QCOMPARE(focusedPointerChangedSpy.count(), 1);
    QVERIFY(!focusedPointerChangedSpy.first().first().value<Srv::Pointer*>());

    // No pointer yet.
    QVERIFY(server_pointers.get_focus().surface);
    QVERIFY(server_pointers.get_focus().devices.empty());

    auto p = m_seat->createPointer(m_seat);
    QSignalSpy frameSpy(p, &Clt::Pointer::frame);
    QVERIFY(frameSpy.isValid());
    Clt::Pointer const& cp = *p;
    QVERIFY(p->isValid());

    QScopedPointer<Clt::RelativePointer> relativePointer(
        m_relativePointerManager->createRelativePointer(p));
    QVERIFY(relativePointer->isValid());

    QSignalSpy pointerCreatedSpy(server.seat, &Srv::Seat::pointerCreated);
    QVERIFY(pointerCreatedSpy.isValid());

    // Once the pointer is created it should be set as the focused pointer.
    QVERIFY(pointerCreatedSpy.wait());
    QVERIFY(server_pointers.get_focus().devices.front());
    QCOMPARE(pointerCreatedSpy.first().first().value<Srv::Pointer*>(),
             server_pointers.get_focus().devices.front());
    QCOMPARE(focusedPointerChangedSpy.count(), 2);
    QCOMPARE(focusedPointerChangedSpy.last().first().value<Srv::Pointer*>(),
             server_pointers.get_focus().devices.front());
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 1);

    server_pointers.set_focused_surface(nullptr);
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
    server_pointers.set_focused_surface(serverSurface, QPoint(10, 15));
    QCOMPARE(server_pointers.get_focus().surface, serverSurface);

    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), server.display->serial());
    QCOMPARE(enteredSpy.first().last().toPoint(), QPoint(10, 3));
    QTRY_COMPARE(frameSpy.count(), 3);

    auto serverPointer = server_pointers.get_focus().devices.front();
    QVERIFY(serverPointer);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);
    QCOMPARE(focusedPointerChangedSpy.count(), 4);
    QCOMPARE(focusedPointerChangedSpy.last().first().value<Srv::Pointer*>(), serverPointer);

    // Test motion.
    server.seat->setTimestamp(1);
    server_pointers.set_position(QPoint(10, 16));
    server_pointers.frame();

    QVERIFY(motionSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 4);
    QCOMPARE(motionSpy.first().first().toPoint(), QPoint(0, 1));
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // Test relative motion.
    server_pointers.relative_motion(QSizeF(1, 2), QSizeF(3, 4), quint64(-1));
    server_pointers.frame();

    QVERIFY(relativeMotionSpy.wait());
    QCOMPARE(relativeMotionSpy.count(), 1);
    QTRY_COMPARE(frameSpy.count(), 5);
    QCOMPARE(relativeMotionSpy.first().at(0).toSizeF(), QSizeF(1, 2));
    QCOMPARE(relativeMotionSpy.first().at(1).toSizeF(), QSizeF(3, 4));
    QCOMPARE(relativeMotionSpy.first().at(2).value<quint64>(), quint64(-1));

    // Test axis.
    server.seat->setTimestamp(2);
    server_pointers.send_axis(Qt::Horizontal, 10);

    QVERIFY(axisSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 5);

    server.seat->setTimestamp(3);
    server_pointers.send_axis(Qt::Vertical, 20);
    server_pointers.frame();

    QVERIFY(axisSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 6);
    QCOMPARE(axisSpy.first().at(0).value<quint32>(), quint32(2));
    QCOMPARE(axisSpy.first().at(1).value<Clt::Pointer::Axis>(), Clt::Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.first().at(2).value<qreal>(), qreal(10));

    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(3));
    QCOMPARE(axisSpy.last().at(1).value<Clt::Pointer::Axis>(), Clt::Pointer::Axis::Vertical);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), qreal(20));

    // Test button.
    server.seat->setTimestamp(4);
    server_pointers.button_pressed(1);
    server_pointers.frame();

    QVERIFY(buttonSpy.wait());
    QTRY_COMPARE(buttonSpy.count(), 1);
    QTRY_COMPARE(frameSpy.count(), 7);
    QCOMPARE(buttonSpy.at(0).at(0).value<quint32>(), server.display->serial());

    server.seat->setTimestamp(5);
    server_pointers.button_pressed(2);
    server_pointers.frame();

    QVERIFY(buttonSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 8);
    QCOMPARE(buttonSpy.at(1).at(0).value<quint32>(), server.display->serial());

    server.seat->setTimestamp(6);
    server_pointers.button_released(2);
    server_pointers.frame();

    QVERIFY(buttonSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 9);
    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), server.display->serial());

    server.seat->setTimestamp(7);
    server_pointers.button_released(1);
    server_pointers.frame();

    QVERIFY(buttonSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 10);
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

    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), server_pointers.button_serial(2));
    // Timestamp
    QCOMPARE(buttonSpy.at(2).at(1).value<quint32>(), quint32(6));
    // Button
    QCOMPARE(buttonSpy.at(2).at(2).value<quint32>(), quint32(2));
    QCOMPARE(buttonSpy.at(2).at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Released);

    QCOMPARE(buttonSpy.at(3).at(0).value<quint32>(), server_pointers.button_serial(1));
    // Timestamp
    QCOMPARE(buttonSpy.at(3).at(1).value<quint32>(), quint32(7));
    // Button
    QCOMPARE(buttonSpy.at(3).at(2).value<quint32>(), quint32(1));
    QCOMPARE(buttonSpy.at(3).at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Released);

    // Leave the surface.
    server_pointers.set_focused_surface(nullptr);
    QCOMPARE(focusedPointerChangedSpy.count(), 5);
    QVERIFY(leftSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 11);
    QCOMPARE(leftSpy.first().first().value<quint32>(), server.display->serial());
    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());

    // Now a relative motion should not be sent to the relative pointer.
    server_pointers.relative_motion(QSizeF(1, 2), QSizeF(3, 4), quint64(-1));
    QVERIFY(!relativeMotionSpy.wait(200));

    // Enter it again.
    server_pointers.set_focused_surface(serverSurface, QPoint(0, 0));
    QCOMPARE(focusedPointerChangedSpy.count(), 6);
    QVERIFY(enteredSpy.wait());
    QTRY_COMPARE(frameSpy.count(), 12);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    // Send another relative motion event.
    server_pointers.relative_motion(QSizeF(4, 5), QSizeF(6, 7), quint64(1));
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
    QVERIFY(server_pointers.get_focus().devices.empty());
    // The focused surface is still the same since it does still exist and it was once set
    // and not changed since then.
    QCOMPARE(server_pointers.get_focus().surface, serverSurface);

    server.seat->setTimestamp(8);
    server_pointers.set_position(QPoint(10, 15));
    server.seat->setTimestamp(9);
    server_pointers.button_pressed(1);
    server.seat->setTimestamp(10);
    server_pointers.button_released(1);
    server.seat->setTimestamp(11);
    server_pointers.send_axis(Qt::Horizontal, 10);
    server.seat->setTimestamp(12);
    server_pointers.send_axis(Qt::Vertical, 20);

    server_pointers.set_focused_surface(nullptr);
    QCOMPARE(focusedPointerChangedSpy.count(), 8);

    server_pointers.set_focused_surface(serverSurface);
    QCOMPARE(focusedPointerChangedSpy.count(), 9);

    QCOMPARE(server_pointers.get_focus().surface, serverSurface);
    QVERIFY(server_pointers.get_focus().devices.empty());

    // Create a pointer again.
    p = m_seat->createPointer(m_seat);
    QVERIFY(focusedPointerChangedSpy.wait());
    QCOMPARE(focusedPointerChangedSpy.count(), 10);
    QCOMPARE(server_pointers.get_focus().surface, serverSurface);
    serverPointer = server_pointers.get_focus().devices.front();
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
    QVERIFY(!server_pointers.get_focus().surface);
    QVERIFY(server_pointers.get_focus().devices.empty());
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
    server.seat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    auto s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_position(QPoint(20, 18));
    QFETCH(QMatrix4x4, enterTransformation);
    server_pointers.set_focused_surface(serverSurface, enterTransformation);
    QCOMPARE(server_pointers.get_focus().transformation, enterTransformation);

    // No pointer yet.
    QVERIFY(server_pointers.get_focus().surface);
    QVERIFY(server_pointers.get_focus().devices.empty());

    auto p = m_seat->createPointer(m_seat);
    Clt::Pointer const& cp = *p;
    QVERIFY(p->isValid());
    QSignalSpy pointerCreatedSpy(server.seat, &Srv::Seat::pointerCreated);
    QVERIFY(pointerCreatedSpy.isValid());

    // Once the pointer is created it should be set as the focused pointer.
    QVERIFY(pointerCreatedSpy.wait());
    QVERIFY(server_pointers.get_focus().devices.front());
    QCOMPARE(pointerCreatedSpy.first().first().value<Srv::Pointer*>(),
             server_pointers.get_focus().devices.front());

    server_pointers.set_focused_surface(nullptr);
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
    server_pointers.set_focused_surface(serverSurface, enterTransformation);
    QCOMPARE(server_pointers.get_focus().surface, serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), server.display->serial());
    QTEST(enteredSpy.first().last().toPointF(), "expectedEnterPoint");

    auto serverPointer = server_pointers.get_focus().devices.front();
    QVERIFY(serverPointer);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    // Test motion.
    server.seat->setTimestamp(1);
    server_pointers.set_position(QPoint(10, 16));
    QVERIFY(motionSpy.wait());
    QTEST(motionSpy.first().first().toPointF(), "expectedMovePoint");
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // Leave the surface.
    server_pointers.set_focused_surface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.first().first().value<quint32>(), server.display->serial());
    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());

    // Enter it again.
    server_pointers.set_focused_surface(serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    delete s;
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    QVERIFY(!server_pointers.get_focus().surface);
}

Q_DECLARE_METATYPE(Qt::MouseButton)

void TestSeat::testPointerButton_data()
{
    QTest::addColumn<Qt::MouseButton>("qtButton");
    QTest::addColumn<quint32>("waylandButton");

    // clang-format off
    QTest::newRow("left")    << Qt::LeftButton    << quint32(BTN_LEFT);
    QTest::newRow("right")   << Qt::RightButton   << quint32(BTN_RIGHT);
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
    server.seat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    QScopedPointer<Clt::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    QSignalSpy buttonChangedSpy(p.data(), &Clt::Pointer::buttonStateChanged);
    QVERIFY(buttonChangedSpy.isValid());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_position(QPoint(20, 18));
    server_pointers.set_focused_surface(serverSurface, QPoint(10, 15));
    QVERIFY(server_pointers.get_focus().surface);
    QVERIFY(server_pointers.get_focus().devices.front());

    QCoreApplication::processEvents();

    server_pointers.set_focused_surface(serverSurface, QPoint(10, 15));

    auto serverPointer = server_pointers.get_focus().devices.front();
    QVERIFY(serverPointer);
    QFETCH(Qt::MouseButton, qtButton);
    QFETCH(quint32, waylandButton);

    quint32 msec = QDateTime::currentMSecsSinceEpoch();
    QCOMPARE(server_pointers.is_button_pressed(waylandButton), false);
    QCOMPARE(server_pointers.is_button_pressed(qtButton), false);
    server.seat->setTimestamp(msec);
    server_pointers.button_pressed(qtButton);
    QCOMPARE(server_pointers.is_button_pressed(waylandButton), true);
    QCOMPARE(server_pointers.is_button_pressed(qtButton), true);

    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 1);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(),
             server_pointers.button_serial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(),
             server_pointers.button_serial(qtButton));
    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), msec);
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Pressed);

    msec++;
    server.seat->setTimestamp(msec);
    server_pointers.button_released(qtButton);
    QCOMPARE(server_pointers.is_button_pressed(waylandButton), false);
    QCOMPARE(server_pointers.is_button_pressed(qtButton), false);

    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 2);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(),
             server_pointers.button_serial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(),
             server_pointers.button_serial(qtButton));

    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), msec);
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<Clt::Pointer::ButtonState>(),
             Clt::Pointer::ButtonState::Released);
}

void TestSeat::testPointerAxis()
{
    // First create the pointer.
    QSignalSpy hasPointerChangedSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    server.seat->setHasPointer(true);

    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());
    QVERIFY(pointer);

    // Now create a surface.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_focused_surface(serverSurface);
    QCOMPARE(server_pointers.get_focus().surface, serverSurface);
    QVERIFY(server_pointers.get_focus().devices.front());

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
    server.seat->setTimestamp(timestamp++);
    server_pointers.send_axis(Qt::Vertical, 10, 1, Srv::PointerAxisSource::Wheel);
    server_pointers.frame();

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
    server.seat->setTimestamp(timestamp++);
    server_pointers.send_axis(Qt::Horizontal, 42, 0, Srv::PointerAxisSource::Finger);
    server_pointers.frame();

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
    server.seat->setTimestamp(timestamp++);
    server_pointers.send_axis(Qt::Horizontal, 0, 0, Srv::PointerAxisSource::Finger);
    server_pointers.frame();

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
    server.seat->setTimestamp(timestamp++);
    server_pointers.send_axis(Qt::Horizontal, 42, 1, Srv::PointerAxisSource::Unknown);
    server_pointers.frame();

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

void TestSeat::testCursor()
{
    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    server.seat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    QScopedPointer<Clt::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    QSignalSpy enteredSpy(p.data(), &Clt::Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_position(QPoint(20, 18));
    server_pointers.set_focused_surface(serverSurface, QPoint(10, 15));

    quint32 serial = server.display->serial();
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), serial);
    QVERIFY(server_pointers.get_focus().surface);
    QVERIFY(server_pointers.get_focus().devices.front());
    QVERIFY(!server_pointers.get_focus().devices.front()->cursor());

    QSignalSpy cursorChangedSpy(server_pointers.get_focus().devices.front(),
                                &Srv::Pointer::cursorChanged);
    QVERIFY(cursorChangedSpy.isValid());
    // Just remove the pointer.
    p->setCursor(nullptr);
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(cursorChangedSpy.count(), 1);
    auto cursor = server_pointers.get_focus().devices.front()->cursor();
    QVERIFY(cursor);
    QVERIFY(!cursor->surface());
    QCOMPARE(cursor->hotspot(), QPoint());
    QCOMPARE(cursor->enteredSerial(), serial);
    QCOMPARE(cursor->pointer(), server_pointers.get_focus().devices.front());

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
    QCOMPARE(cursor->surface()->state().buffer->shmImage()->createQImage(), img);

    // And add another image to the surface.
    QImage blue(QSize(10, 20), QImage::Format_ARGB32_Premultiplied);
    blue.fill(Qt::blue);
    cursorSurface->attachBuffer(m_shm->createBuffer(blue));
    cursorSurface->damage(QRect(0, 0, 10, 20));
    cursorSurface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.count(), 4);
    QCOMPARE(cursorChangedSpy.count(), 5);
    QCOMPARE(cursor->surface()->state().buffer->shmImage()->createQImage(), blue);

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
    server.seat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    // Create pointer.
    QScopedPointer<Clt::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    QSignalSpy enteredSpy(p.data(), &Clt::Pointer::entered);

    QVERIFY(enteredSpy.isValid());
    // Create surface.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Send enter to the surface.
    auto& server_pointers = server.seat->pointers();
    server_pointers.set_focused_surface(serverSurface);
    QVERIFY(enteredSpy.wait());

    // Create a signal spy for the cursor changed signal.
    auto pointer = server_pointers.get_focus().devices.front();
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
    QCOMPARE(pointer->cursor()->surface()->state().buffer->shmImage()->createQImage(), red);

    // And damage the surface.
    QImage blue(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    blue.fill(Qt::blue);
    cursorSurface->attachBuffer(m_shm->createBuffer(blue));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(pointer->cursor()->surface()->state().buffer->shmImage()->createQImage(), blue);
}

void TestSeat::testKeyboard()
{
    QSignalSpy keyboardSpy(m_seat, &Clt::Seat::hasKeyboardChanged);
    QVERIFY(keyboardSpy.isValid());
    server.seat->setHasKeyboard(true);
    QVERIFY(keyboardSpy.wait());

    // Create the surface.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    auto* s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    server.seat->setFocusedKeyboardSurface(serverSurface);

    // No keyboard yet.
    auto& keyboards = server.seat->keyboards();
    QCOMPARE(keyboards.get_focus().surface, serverSurface);
    QVERIFY(keyboards.get_focus().devices.empty());

    auto* keyboard = m_seat->createKeyboard(m_seat);
    QSignalSpy repeatInfoSpy(keyboard, &Clt::Keyboard::keyRepeatChanged);
    QVERIFY(repeatInfoSpy.isValid());
    Clt::Keyboard const& ckeyboard = *keyboard;
    QVERIFY(keyboard->isValid());
    QCOMPARE(keyboard->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard->keyRepeatDelay(), 0);
    QCOMPARE(keyboard->keyRepeatRate(), 0);
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    auto serverKeyboard = keyboards.get_focus().devices.front();
    QVERIFY(serverKeyboard);

    // We should get the repeat info announced.
    QCOMPARE(repeatInfoSpy.count(), 1);
    QCOMPARE(keyboard->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard->keyRepeatDelay(), 0);
    QCOMPARE(keyboard->keyRepeatRate(), 0);

    // Let's change repeat in server.
    keyboards.set_repeat_info(25, 660);
    keyboards.get_focus().devices.front()->client()->flush();
    QVERIFY(repeatInfoSpy.wait());
    QCOMPARE(repeatInfoSpy.count(), 2);
    QCOMPARE(keyboard->isKeyRepeatEnabled(), true);
    QCOMPARE(keyboard->keyRepeatRate(), 25);
    QCOMPARE(keyboard->keyRepeatDelay(), 660);

    server.seat->setTimestamp(1);
    keyboards.key(KEY_K, Wrapland::Server::key_state::pressed);
    server.seat->setTimestamp(2);
    keyboards.key(KEY_D, Wrapland::Server::key_state::pressed);
    server.seat->setTimestamp(3);
    keyboards.key(KEY_E, Wrapland::Server::key_state::pressed);

    QSignalSpy leftSpy(keyboard, &Clt::Keyboard::left);
    QVERIFY(leftSpy.isValid());
    server.seat->setFocusedKeyboardSurface(nullptr);
    QVERIFY(leftSpy.wait());

    QSignalSpy modifierSpy(keyboard, &Clt::Keyboard::modifiersChanged);
    QVERIFY(modifierSpy.isValid());

    QSignalSpy enteredSpy(keyboard, &Clt::Keyboard::entered);
    QVERIFY(enteredSpy.isValid());
    server.seat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(keyboards.get_focus().surface, serverSurface);
    QCOMPARE(keyboards.get_focus().devices.front()->focusedSurface(), serverSurface);

    // We get the modifiers sent after the enter.
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 1);
    QCOMPARE(modifierSpy.first().at(0).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(1).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(2).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(3).value<quint32>(), quint32(0));
    QCOMPARE(enteredSpy.count(), 1);

    // TODO: get through API
    QCOMPARE(enteredSpy.first().first().value<quint32>(), server.display->serial() - 1);

    QSignalSpy keyChangedSpy(keyboard, &Clt::Keyboard::keyChanged);
    QVERIFY(keyChangedSpy.isValid());

    server.seat->setTimestamp(4);
    keyboards.key(KEY_E, Wrapland::Server::key_state::released);
    QVERIFY(keyChangedSpy.wait());
    server.seat->setTimestamp(5);
    keyboards.key(KEY_D, Wrapland::Server::key_state::released);
    QVERIFY(keyChangedSpy.wait());
    server.seat->setTimestamp(6);
    keyboards.key(KEY_K, Wrapland::Server::key_state::released);
    QVERIFY(keyChangedSpy.wait());
    server.seat->setTimestamp(7);
    keyboards.key(KEY_F1, Wrapland::Server::key_state::pressed);
    QVERIFY(keyChangedSpy.wait());
    server.seat->setTimestamp(8);
    keyboards.key(KEY_F1, Wrapland::Server::key_state::released);
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
    keyboards.key(KEY_F1, Wrapland::Server::key_state::released);
    QVERIFY(!keyChangedSpy.wait(200));

    // Let's press it again.
    keyboards.key(KEY_F1, Wrapland::Server::key_state::pressed);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 6);

    // Press again should be ignored.
    keyboards.key(KEY_F1, Wrapland::Server::key_state::pressed);
    QVERIFY(!keyChangedSpy.wait(200));

    // And release.
    keyboards.key(KEY_F1, Wrapland::Server::key_state::released);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 7);

    keyboards.update_modifiers(1, 2, 3, 4);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 2);
    QCOMPARE(modifierSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(modifierSpy.last().at(1).value<quint32>(), quint32(2));
    QCOMPARE(modifierSpy.last().at(2).value<quint32>(), quint32(3));
    QCOMPARE(modifierSpy.last().at(3).value<quint32>(), quint32(4));

    leftSpy.clear();
    server.seat->setFocusedKeyboardSurface(nullptr);
    QVERIFY(!keyboards.get_focus().surface);
    QVERIFY(keyboards.get_focus().devices.empty());
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);

    // TODO: get through API
    QCOMPARE(leftSpy.first().first().value<quint32>(), server.display->serial() - 1);

    QVERIFY(!keyboard->enteredSurface());
    QVERIFY(!ckeyboard.enteredSurface());

    // Enter it again.
    server.seat->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(keyboards.get_focus().surface, serverSurface);
    QCOMPARE(keyboards.get_focus().devices.front()->focusedSurface(), serverSurface);
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
    QVERIFY(!keyboards.get_focus().surface);
    QVERIFY(keyboards.get_focus().devices.empty());
    QVERIFY(!serverKeyboard->focusedSurface());

    // Let's create a Surface again.
    QScopedPointer<Clt::Surface> s2(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 2);
    serverSurface = surfaceCreatedSpy.last().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    server.seat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(keyboards.get_focus().surface, serverSurface);
    QCOMPARE(keyboards.get_focus().devices.front(), serverKeyboard);

    // Delete the Keyboard.
    QSignalSpy destroyedSpy(serverKeyboard, &Srv::Keyboard::destroyed);
    QVERIFY(destroyedSpy.isValid());

    delete keyboard;
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(destroyedSpy.count(), 1);

    // Verify that calling into the Keyboard related functionality doesn't crash.
    server.seat->setTimestamp(9);
    keyboards.key(KEY_F2, Wrapland::Server::key_state::pressed);
    server.seat->setTimestamp(10);
    keyboards.key(KEY_F2, Wrapland::Server::key_state::released);
    keyboards.set_repeat_info(30, 560);
    keyboards.set_repeat_info(25, 660);
    keyboards.update_modifiers(5, 6, 7, 8);
    server.seat->setFocusedKeyboardSurface(nullptr);
    server.seat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(keyboards.get_focus().surface, serverSurface);
    QVERIFY(keyboards.get_focus().devices.empty());

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
    QCOMPARE(keyboards.get_focus().surface, serverSurface);

    serverKeyboard = keyboards.get_focus().devices.front();

    QVERIFY(serverKeyboard);
    QSignalSpy keyboard2DestroyedSpy(serverKeyboard, &Srv::Keyboard::destroyed);
    QVERIFY(keyboard2DestroyedSpy.isValid());

    delete keyboard2;
    QVERIFY(keyboard2DestroyedSpy.wait());

    // This should have unset it on the server.
    QVERIFY(keyboards.get_focus().devices.empty());

    // But not the surface.
    QCOMPARE(keyboards.get_focus().surface, serverSurface);
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
    Clt::Seat const& s2(s);
    QCOMPARE((wl_seat*)s2, wlSeat);
}

void TestSeat::testDestroy()
{

    QSignalSpy keyboardSpy(m_seat, &Clt::Seat::hasKeyboardChanged);
    QVERIFY(keyboardSpy.isValid());
    server.seat->setHasKeyboard(true);
    QVERIFY(keyboardSpy.wait());
    auto* k = m_seat->createKeyboard(m_seat);
    QVERIFY(k->isValid());

    QSignalSpy pointerSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    server.seat->setHasPointer(true);
    QVERIFY(pointerSpy.wait());
    auto* p = m_seat->createPointer(m_seat);
    QVERIFY(p->isValid());

    QSignalSpy touchSpy(m_seat, &Clt::Seat::hasTouchChanged);
    QVERIFY(touchSpy.isValid());
    server.seat->setHasTouch(true);
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

    server = {};
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
    server.seat->setHasKeyboard(true);

    auto ddmi = std::make_unique<Wrapland::Server::data_device_manager>(server.display.get());

    QSignalSpy ddiCreatedSpy(ddmi.get(), &Srv::data_device_manager::device_created);
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

    QScopedPointer<Clt::DataDevice> dd1(ddm->getDevice(m_seat));

    QVERIFY(ddiCreatedSpy.wait());
    QCOMPARE(ddiCreatedSpy.count(), 1);

    auto ddi = ddiCreatedSpy.first().first().value<Srv::data_device*>();
    QVERIFY(ddi);

    QVERIFY(dd1->isValid());
    QSignalSpy selectionSpy(dd1.data(), &Clt::DataDevice::selectionOffered);
    QVERIFY(selectionSpy.isValid());

    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(!server.seat->selection());

    auto keyboard = m_seat->createKeyboard(m_seat);
    QSignalSpy entered_spy(keyboard, &Clt::Keyboard::entered);
    QVERIFY(entered_spy.isValid());

    server.seat->setHasKeyboard(true);
    server.seat->setFocusedKeyboardSurface(serverSurface);

    auto& keyboards = server.seat->keyboards();
    QCOMPARE(keyboards.get_focus().surface, serverSurface);
    QVERIFY(keyboards.get_focus().devices.empty());
    QVERIFY(entered_spy.wait());
    QVERIFY(selectionSpy.isEmpty());

    selectionSpy.clear();
    QVERIFY(!server.seat->selection());

    // Now let's try to set a selection - we have keyboard focus, so it should be sent to us.
    QScopedPointer<Clt::DataSource> ds(ddm->createSource());
    QVERIFY(ds->isValid());

    ds->offer(QStringLiteral("text/plain"));
    dd1->setSelection(0, ds.data());

    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 1);
    auto server_data_source = server.seat->selection();
    QVERIFY(server_data_source);

    auto df = selectionSpy.first().first().value<Clt::DataOffer*>();
    QCOMPARE(df->offeredMimeTypes().count(), 1);
    QCOMPARE(df->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // Try to clear.
    dd1->setSelection(0, nullptr);
    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 2);

    // Unset the keyboard focus.
    server.seat->setFocusedKeyboardSurface(nullptr);
    QVERIFY(!keyboards.get_focus().surface);
    QVERIFY(keyboards.get_focus().devices.empty());

    serverSurface->client()->flush();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Try to set Selection.
    dd1->setSelection(0, ds.data());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(selectionSpy.count(), 2);
    QVERIFY(!dd1->offeredSelection());

    // Let's unset the selection on the seat.
    server.seat->setSelection(nullptr);

    // And pass focus back on our surface.
    server.seat->setFocusedKeyboardSurface(serverSurface);

    // We don't have a selection, so it should not send a selection.
    QVERIFY(!selectionSpy.wait(100));

    // Now let's set it manually.
    server.seat->setSelection(server_data_source);
    QCOMPARE(server.seat->selection(), ddi->selection());
    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 3);

    // Setting the same again should not change.
    server.seat->setSelection(server_data_source);
    QVERIFY(!selectionSpy.wait(100));

    // Now clear it manually.
    server.seat->setSelection(nullptr);
    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 4);

    // Create a second ddi and a data source.
    QScopedPointer<Clt::DataDevice> dd2(ddm->getDevice(m_seat));
    QVERIFY(dd2->isValid());
    QScopedPointer<Clt::DataSource> ds2(ddm->createSource());
    QVERIFY(ds2->isValid());
    ds2->offer(QStringLiteral("text/plain"));
    dd2->setSelection(0, ds2.data());
    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 5);
    QSignalSpy cancelledSpy(ds2.data(), &Clt::DataSource::cancelled);
    QVERIFY(cancelledSpy.isValid());
    server.seat->setSelection(server_data_source);
    QVERIFY(cancelledSpy.wait());

    // If we don't wait for the selection signal as well the test still works but we sporadically
    // leak memory from the offer not being processed completely in the client and the lastOffer
    // member variable not being cleaned up.
    // TODO(romangg): Fix leak in client library when selection is not updated in time.
    QVERIFY(selectionSpy.count() == 6 || selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 6);

    // Copy already cleared selection, BUG 383054.
    ddi->send_selection(ddi->selection());
}

void TestSeat::testSelectionNoDataSource()
{
    // This test verifies that the server doesn't crash when using setSelection with
    // a DataDevice which doesn't have a DataSource yet.

    auto ddmi = std::make_unique<Wrapland::Server::data_device_manager>(server.display.get());
    QSignalSpy ddiCreatedSpy(ddmi.get(), &Srv::data_device_manager::device_created);
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

    QScopedPointer<Clt::DataDevice> dd(ddm->getDevice(m_seat));
    QVERIFY(dd->isValid());

    QVERIFY(ddiCreatedSpy.wait());
    QCOMPARE(ddiCreatedSpy.count(), 1);

    auto ddi = ddiCreatedSpy.first().first().value<Srv::data_device*>();
    QVERIFY(ddi);

    // Now create a surface and pass it keyboard focus.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(!server.seat->selection());

    server.seat->setHasKeyboard(true);
    server.seat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(server.seat->keyboards().get_focus().surface, serverSurface);

    // Now let's set the selection.
    server.seat->setSelection(ddi->selection());
}

void TestSeat::testDataDeviceForKeyboardSurface()
{
    // This test verifies that the server does not crash when creating a datadevice for the focused
    // keyboard surface and the currentSelection does not have a DataSource.
    // To properly test the functionality this test requires a second client.

    auto ddmi = std::make_unique<Wrapland::Server::data_device_manager>(server.display.get());
    QSignalSpy ddiCreatedSpy(ddmi.get(), &Srv::data_device_manager::device_created);
    QVERIFY(ddiCreatedSpy.isValid());

    // Create a second Wayland client connection to use it for setSelection.
    auto c = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(c, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    c->setSocketName(socket_name);

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
    QScopedPointer<Clt::DataDevice> dd1(ddm1->getDevice(seat.data()));
    QVERIFY(ddiCreatedSpy.wait());
    auto* ddi = ddiCreatedSpy.first().first().value<Srv::data_device*>();
    QVERIFY(ddi);
    server.seat->setSelection(ddi->selection());

    // Switch to other client.
    // Create a surface and pass it keyboard focus.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();

    server.seat->setHasKeyboard(true);
    server.seat->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(server.seat->keyboards().get_focus().surface, serverSurface);

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

    QScopedPointer<Clt::DataDevice> dd(ddm->getDevice(m_seat));
    QVERIFY(dd->isValid());
    QVERIFY(ddiCreatedSpy.wait());

    // Unset surface and set again.
    server.seat->setFocusedKeyboardSurface(nullptr);
    server.seat->setFocusedKeyboardSurface(serverSurface);

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
    server.seat->setHasTouch(true);
    QVERIFY(touchSpy.wait());

    // Create the surface.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    auto* s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    auto& server_touches = server.seat->touches();
    server_touches.set_focused_surface(serverSurface);
    // No keyboard yet.
    QCOMPARE(server_touches.get_focus().surface, serverSurface);
    QVERIFY(server_touches.get_focus().devices.empty());

    QSignalSpy touchCreatedSpy(server.seat, &Srv::Seat::touchCreated);
    QVERIFY(touchCreatedSpy.isValid());
    auto* touch = m_seat->createTouch(m_seat);
    QVERIFY(touch->isValid());
    QVERIFY(touchCreatedSpy.wait());
    auto serverTouch = server_touches.get_focus().devices.front();
    QVERIFY(serverTouch);
    QCOMPARE(touchCreatedSpy.first().first().value<Srv::Touch*>(),
             server_touches.get_focus().devices.front());

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
    server_touches.set_focused_surface_position(QPointF(10, 20));
    QCOMPARE(server_touches.get_focus().offset, QPointF(10, 20));
    server.seat->setTimestamp(1);
    QCOMPARE(server_touches.touch_down(QPointF(15, 26)), 0);
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
    QCOMPARE(tp->downSerial(), server.display->serial());
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
    server_touches.touch_frame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(frameEndedSpy.count(), 1);

    // Move the one point.
    server.seat->setTimestamp(2);
    server_touches.touch_move(0, QPointF(10, 20));
    server_touches.touch_frame();
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
    server.seat->setTimestamp(3);
    QCOMPARE(server_touches.touch_down(QPointF(15, 26)), 1);
    server_touches.touch_frame();
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
    server.seat->setTimestamp(4);
    server_touches.touch_up(1);
    server_touches.touch_frame();
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
    QCOMPARE(tp2->upSerial(), server.display->serial());
    QCOMPARE(tp2->surface().data(), s);

    // Send another down and up.
    server.seat->setTimestamp(5);
    QCOMPARE(server_touches.touch_down(QPointF(15, 26)), 1);
    server_touches.touch_frame();
    server.seat->setTimestamp(6);
    server_touches.touch_up(1);

    // And send an up for the first point.
    server_touches.touch_up(0);
    server_touches.touch_frame();
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
    QVERIFY(!server_touches.is_in_progress());

    // Try cancel.
    server_touches.set_focused_surface(serverSurface, QPointF(15, 26));
    server.seat->setTimestamp(7);
    QCOMPARE(server_touches.touch_down(QPointF(15, 26)), 0);
    server_touches.touch_frame();
    server_touches.cancel_sequence();
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
    QSignalSpy destroyedSpy(serverTouch, &Srv::Touch::resourceDestroyed);
    QVERIFY(destroyedSpy.isValid());

    delete touch;
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(destroyedSpy.count(), 1);

    // Try to call into all the methods of the touch interface, should not crash.
    QVERIFY(server_touches.get_focus().devices.empty());
    server.seat->setTimestamp(8);
    QCOMPARE(server_touches.touch_down(QPointF(15, 26)), 0);
    server_touches.touch_frame();
    server_touches.touch_move(0, QPointF(0, 0));
    QCOMPARE(server_touches.touch_down(QPointF(15, 26)), 1);
    server_touches.cancel_sequence();

    // Should have unset the focused touch.
    QVERIFY(server_touches.get_focus().devices.empty());

    // But not the focused touch surface.
    QCOMPARE(server_touches.get_focus().surface, serverSurface);
}

void TestSeat::testDisconnect()
{
    // This test verifies that disconnecting the client cleans up correctly.
    QSignalSpy keyboardCreatedSpy(server.seat, &Srv::Seat::keyboardCreated);
    QVERIFY(keyboardCreatedSpy.isValid());
    QSignalSpy pointerCreatedSpy(server.seat, &Srv::Seat::pointerCreated);
    QVERIFY(pointerCreatedSpy.isValid());
    QSignalSpy touchCreatedSpy(server.seat, &Srv::Seat::touchCreated);
    QVERIFY(touchCreatedSpy.isValid());

    // Create the things we need.
    server.seat->setHasKeyboard(true);
    server.seat->setHasPointer(true);
    server.seat->setHasTouch(true);
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
    auto serverTouch = touchCreatedSpy.first().first().value<Srv::Touch*>();
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
    // We currently don't allow to set the pointer on an unbound surface. The consumer must listen
    // for the destroy event on a surface instead. That's the API contract.
    // We might change this back again if it makes sense, but what's the advantage of handling this
    // case? It will just fail silently and the session seems broken.
    // For the general question on object lifetime see also issue #38.
#if 0
    // Create the things we need.
    server.seat->setHasKeyboard(true);
    server.seat->setHasPointer(true);
    server.seat->setHasTouch(true);
    QSignalSpy pointerChangedSpy(m_seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(pointerChangedSpy.isValid());
    QVERIFY(pointerChangedSpy.wait());

    // Create pointer and Surface.
    QScopedPointer<Clt::Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());

    // Create the surface.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> s(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto* serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Unbind the surface again.
    QSignalSpy serverPointerChangedSpy(server.seat, &Srv::Seat::focusedPointerChanged);
    QVERIFY(serverPointerChangedSpy.isValid());
    QSignalSpy surfaceUnboundSpy(serverSurface, &Srv::Surface::resourceDestroyed);
    QVERIFY(surfaceUnboundSpy.isValid());
    s.reset();
    QVERIFY(surfaceUnboundSpy.wait());

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_focused_surface(serverSurface);

    QVERIFY(!pointerChangedSpy.wait(200));
    QCOMPARE(serverPointerChangedSpy.count(), 2);
    QVERIFY(!server_pointers.get_focus().surface);
#endif
}

void TestSeat::testKeymap()
{
    server.seat->setHasKeyboard(true);
    QSignalSpy keyboardChangedSpy(m_seat, &Clt::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    QVERIFY(keyboardChangedSpy.wait());

    std::unique_ptr<Clt::Keyboard> keyboard(m_seat->createKeyboard());
    QSignalSpy keymapChangedSpy(keyboard.get(), &Clt::Keyboard::keymapChanged);
    QVERIFY(keymapChangedSpy.isValid());

    auto& keyboards = server.seat->keyboards();

    constexpr auto keymap1 = "foo";
    keyboards.set_keymap(keymap1);

    // Not yet received because does not have focus.
    QVERIFY(!keymapChangedSpy.wait(500));

    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    auto surface = std::unique_ptr<Clt::Surface>(m_compositor->createSurface());

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // With focus the keymap is changed.
    keyboards.set_focused_surface(serverSurface);
    QVERIFY(keymapChangedSpy.wait());

    auto fd = keymapChangedSpy.first().first().toInt();
    QVERIFY(fd != -1);
    QCOMPARE(keymapChangedSpy.first().last().value<quint32>(), 3u);

    QFile file;
    QVERIFY(file.open(fd, QIODevice::ReadOnly));
    auto address
        = reinterpret_cast<char*>(file.map(0, keymapChangedSpy.first().last().value<quint32>()));
    QVERIFY(address);
    QCOMPARE(qstrcmp(address, "foo"), 0);
    file.close();

    // Change the keymap.
    keymapChangedSpy.clear();

    constexpr auto keymap2 = "bar";
    keyboards.set_keymap(keymap2);

    // Since we still have focus the keymap is received immediately.
    QVERIFY(keymapChangedSpy.wait());

    fd = keymapChangedSpy.first().first().toInt();
    QVERIFY(fd != -1);
    QCOMPARE(keymapChangedSpy.first().last().value<quint32>(), 3u);
    QVERIFY(file.open(fd, QIODevice::ReadWrite));
    address
        = reinterpret_cast<char*>(file.map(0, keymapChangedSpy.first().last().value<quint32>()));
    QVERIFY(address);
    QCOMPARE(qstrcmp(address, "bar"), 0);
}

QTEST_GUILESS_MAIN(TestSeat)
#include "seat.moc"
