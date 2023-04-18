/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/pointer.h"
#include "../../src/client/pointergestures.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/client.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/pointer_gestures_v1.h"
#include "../../server/pointer_pool.h"
#include "../../server/relative_pointer_v1.h"
#include "../../server/seat.h"
#include "../../server/subcompositor.h"
#include "../../server/surface.h"

#include "../../tests/globals.h"

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class pointer_gestures_test : public QObject
{
    Q_OBJECT
public:
    explicit pointer_gestures_test(QObject* parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void test_pointer_swipe_gesture_data();
    void test_pointer_swipe_gesture();
    void test_pointer_pinch_gesture_data();
    void test_pointer_pinch_gesture();
    void test_pointer_hold_gesture_data();
    void test_pointer_hold_gesture();

private:
    struct {
        std::unique_ptr<Srv::Display> display;
        Wrapland::Server::globals globals;
        Srv::Seat* seat{nullptr};
    } server;

    struct {
        Clt::ConnectionThread* connection{nullptr};
        Clt::EventQueue* queue{nullptr};
        Clt::Compositor* compositor{nullptr};
        Clt::Seat* seat{nullptr};
        Clt::PointerGestures* gestures{nullptr};
        QThread* thread{nullptr};
    } client;
};

constexpr auto socket_name{"wrapland-test-wayland-pointer-gestures-0"};

pointer_gestures_test::pointer_gestures_test(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<Srv::Pointer*>();
    qRegisterMetaType<Srv::Surface*>();
}

void pointer_gestures_test::init()
{
    server.display = std::make_unique<Srv::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());
    server.globals.subcompositor
        = std::make_unique<Wrapland::Server::Subcompositor>(server.display.get());
    server.globals.relative_pointer_manager_v1
        = std::make_unique<Wrapland::Server::RelativePointerManagerV1>(server.display.get());
    server.globals.pointer_gestures_v1
        = std::make_unique<Wrapland::Server::PointerGesturesV1>(server.display.get());

    // Setup connection.
    client.connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(client.connection, &Clt::ConnectionThread::establishedChanged);
    client.connection->setSocketName(socket_name);

    client.thread = new QThread(this);
    client.connection->moveToThread(client.thread);
    client.thread->start();

    client.connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    client.queue = new Clt::EventQueue(this);
    client.queue->setup(client.connection);

    Clt::Registry registry;
    QSignalSpy compositorSpy(&registry, &Clt::Registry::compositorAnnounced);
    QSignalSpy seatSpy(&registry, &Clt::Registry::seatAnnounced);

    registry.setEventQueue(client.queue);
    registry.create(client.connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(compositorSpy.wait());

    server.globals.seats.push_back(std::make_unique<Wrapland::Server::Seat>(server.display.get()));
    server.seat = server.globals.seats.back().get();
    server.seat->setName("seat0");

    QVERIFY(seatSpy.wait());

    client.compositor = new Clt::Compositor(this);
    client.compositor->setup(
        registry.bindCompositor(compositorSpy.first().first().value<quint32>(),
                                compositorSpy.first().last().value<quint32>()));
    QVERIFY(client.compositor->isValid());

    client.seat = registry.createSeat(
        seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QSignalSpy nameSpy(client.seat, &Clt::Seat::nameChanged);
    QVERIFY(nameSpy.wait());

    client.gestures = registry.createPointerGestures(
        registry.interface(Clt::Registry::Interface::PointerGesturesUnstableV1).name,
        registry.interface(Clt::Registry::Interface::PointerGesturesUnstableV1).version,
        this);
    QVERIFY(client.gestures->isValid());
}

void pointer_gestures_test::cleanup()
{
    delete client.gestures;
    client.gestures = nullptr;

    delete client.seat;
    client.seat = nullptr;

    delete client.compositor;
    client.compositor = nullptr;

    delete client.queue;
    client.queue = nullptr;

    if (client.connection) {
        client.connection->deleteLater();
        client.connection = nullptr;
    }
    if (client.thread) {
        client.thread->quit();
        client.thread->wait();
        delete client.thread;
        client.thread = nullptr;
    }

    server = {};
}

void pointer_gestures_test::test_pointer_swipe_gesture_data()
{
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<int>("expectedEndCount");
    QTest::addColumn<int>("expectedCancelCount");

    QTest::newRow("end") << false << 1 << 0;
    QTest::newRow("cancel") << true << 0 << 1;
}

void pointer_gestures_test::test_pointer_swipe_gesture()
{
    // First create the pointer and pointer swipe gesture.
    QSignalSpy hasPointerChangedSpy(client.seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());

    server.seat->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(client.seat->createPointer());
    QScopedPointer<Clt::PointerSwipeGesture> gesture(
        client.gestures->createSwipeGesture(pointer.data()));
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
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(client.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_focused_surface(serverSurface);
    QCOMPARE(server_pointers.get_focus().surface, serverSurface);
    QVERIFY(server_pointers.get_focus().devices.front());

    // Send in the start.
    quint32 timestamp = 1;
    server.seat->setTimestamp(timestamp++);
    server_pointers.start_swipe_gesture(2);

    QVERIFY(startSpy.wait());
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.first().at(0).value<quint32>(), server.display->serial());
    QCOMPARE(startSpy.first().at(1).value<quint32>(), 1u);
    QCOMPARE(gesture->fingerCount(), 2u);
    QCOMPARE(gesture->surface().data(), surface.data());

    // Another start should not be possible.
    server_pointers.start_swipe_gesture(2);
    QVERIFY(!startSpy.wait(200));

    // Send in some updates.
    server.seat->setTimestamp(timestamp++);
    server_pointers.update_swipe_gesture(QSizeF(2, 3));

    QVERIFY(updateSpy.wait());
    server.seat->setTimestamp(timestamp++);
    server_pointers.update_swipe_gesture(QSizeF(4, 5));

    QVERIFY(updateSpy.wait());
    QCOMPARE(updateSpy.count(), 2);
    QCOMPARE(updateSpy.at(0).at(0).toSizeF(), QSizeF(2, 3));
    QCOMPARE(updateSpy.at(0).at(1).value<quint32>(), 2u);
    QCOMPARE(updateSpy.at(1).at(0).toSizeF(), QSizeF(4, 5));
    QCOMPARE(updateSpy.at(1).at(1).value<quint32>(), 3u);

    // Now end or cancel.
    QFETCH(bool, cancel);
    QSignalSpy* spy;

    server.seat->setTimestamp(timestamp++);
    if (cancel) {
        server_pointers.cancel_swipe_gesture();
        spy = &cancelledSpy;
    } else {
        server_pointers.end_swipe_gesture();
        spy = &endSpy;
    }

    QVERIFY(spy->wait());

    QFETCH(int, expectedEndCount);
    QFETCH(int, expectedCancelCount);
    QCOMPARE(endSpy.count(), expectedEndCount);
    QCOMPARE(cancelledSpy.count(), expectedCancelCount);

    QCOMPARE(spy->count(), 1);
    QCOMPARE(spy->first().at(0).value<quint32>(), server.display->serial());
    QCOMPARE(spy->first().at(1).value<quint32>(), 4u);

    QCOMPARE(gesture->fingerCount(), 0u);
    QVERIFY(gesture->surface().isNull());

    // Now a start should be possible again.
    server.seat->setTimestamp(timestamp++);
    server_pointers.start_swipe_gesture(2);
    QVERIFY(startSpy.wait());

    // Unsetting the focused pointer surface should not change anything.
    server_pointers.set_focused_surface(nullptr);
    server.seat->setTimestamp(timestamp++);
    server_pointers.update_swipe_gesture(QSizeF(6, 7));
    QVERIFY(updateSpy.wait());
    // And end.
    server.seat->setTimestamp(timestamp++);
    if (cancel) {
        server_pointers.cancel_swipe_gesture();
    } else {
        server_pointers.end_swipe_gesture();
    }
    QVERIFY(spy->wait());
}

void pointer_gestures_test::test_pointer_pinch_gesture_data()
{
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<int>("expectedEndCount");
    QTest::addColumn<int>("expectedCancelCount");

    QTest::newRow("end") << false << 1 << 0;
    QTest::newRow("cancel") << true << 0 << 1;
}

void pointer_gestures_test::test_pointer_pinch_gesture()
{
    // First create the pointer and pointer swipe gesture.
    QSignalSpy hasPointerChangedSpy(client.seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    server.seat->setHasPointer(true);

    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(client.seat->createPointer());
    QScopedPointer<Clt::PointerPinchGesture> gesture(
        client.gestures->createPinchGesture(pointer.data()));
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
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(client.compositor->createSurface());

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_focused_surface(serverSurface);
    QCOMPARE(server_pointers.get_focus().surface, serverSurface);
    QVERIFY(server_pointers.get_focus().devices.front());

    // Send in the start.
    quint32 timestamp = 1;
    server.seat->setTimestamp(timestamp++);
    server_pointers.start_pinch_gesture(3);

    QVERIFY(startSpy.wait());
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.first().at(0).value<quint32>(), server.display->serial());
    QCOMPARE(startSpy.first().at(1).value<quint32>(), 1u);
    QCOMPARE(gesture->fingerCount(), 3u);
    QCOMPARE(gesture->surface().data(), surface.data());

    // Another start should not be possible.
    server_pointers.start_pinch_gesture(3);
    QVERIFY(!startSpy.wait(200));

    // Send in some updates.
    server.seat->setTimestamp(timestamp++);
    server_pointers.update_pinch_gesture(QSizeF(2, 3), 2, 45);

    QVERIFY(updateSpy.wait());
    server.seat->setTimestamp(timestamp++);
    server_pointers.update_pinch_gesture(QSizeF(4, 5), 1, 90);

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

    server.seat->setTimestamp(timestamp++);
    if (cancel) {
        server_pointers.cancel_pinch_gesture();
        spy = &cancelledSpy;
    } else {
        server_pointers.end_pinch_gesture();
        spy = &endSpy;
    }

    QVERIFY(spy->wait());

    QFETCH(int, expectedEndCount);
    QFETCH(int, expectedCancelCount);
    QCOMPARE(endSpy.count(), expectedEndCount);
    QCOMPARE(cancelledSpy.count(), expectedCancelCount);

    QCOMPARE(spy->count(), 1);
    QCOMPARE(spy->first().at(0).value<quint32>(), server.display->serial());
    QCOMPARE(spy->first().at(1).value<quint32>(), 4u);

    QCOMPARE(gesture->fingerCount(), 0u);
    QVERIFY(gesture->surface().isNull());

    // Now a start should be possible again.
    server.seat->setTimestamp(timestamp++);
    server_pointers.start_pinch_gesture(3);
    QVERIFY(startSpy.wait());

    // Unsetting the focused pointer surface should not change anything.
    server_pointers.set_focused_surface(nullptr);
    server.seat->setTimestamp(timestamp++);
    server_pointers.update_pinch_gesture(QSizeF(6, 7), 2, -45);

    QVERIFY(updateSpy.wait());

    // And end.
    server.seat->setTimestamp(timestamp++);

    if (cancel) {
        server_pointers.cancel_pinch_gesture();
    } else {
        server_pointers.end_pinch_gesture();
    }

    QVERIFY(spy->wait());
}

void pointer_gestures_test::test_pointer_hold_gesture_data()
{
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<int>("expectedEndCount");
    QTest::addColumn<int>("expectedCancelCount");

    QTest::newRow("end") << false << 1 << 0;
    QTest::newRow("cancel") << true << 0 << 1;
}

void pointer_gestures_test::test_pointer_hold_gesture()
{
    // First create the pointer and pointer swipe gesture.
    QSignalSpy hasPointerChangedSpy(client.seat, &Clt::Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    server.seat->setHasPointer(true);

    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Clt::Pointer> pointer(client.seat->createPointer());
    QScopedPointer<Clt::pointer_hold_gesture> gesture(
        client.gestures->create_hold_gesture(pointer.data()));
    QVERIFY(gesture);
    QVERIFY(gesture->isValid());
    QVERIFY(gesture->surface().isNull());
    QCOMPARE(gesture->fingerCount(), 0u);

    QSignalSpy startSpy(gesture.data(), &Clt::pointer_hold_gesture::started);
    QVERIFY(startSpy.isValid());
    QSignalSpy endSpy(gesture.data(), &Clt::pointer_hold_gesture::ended);
    QVERIFY(endSpy.isValid());
    QSignalSpy cancelledSpy(gesture.data(), &Clt::pointer_hold_gesture::cancelled);
    QVERIFY(cancelledSpy.isValid());

    // Now create a surface.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(), &Srv::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Clt::Surface> surface(client.compositor->createSurface());

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    auto& server_pointers = server.seat->pointers();
    server_pointers.set_focused_surface(serverSurface);
    QCOMPARE(server_pointers.get_focus().surface, serverSurface);
    QVERIFY(server_pointers.get_focus().devices.front());

    // Send in the start.
    quint32 timestamp = 1;
    server.seat->setTimestamp(timestamp++);
    server_pointers.start_hold_gesture(3);

    QVERIFY(startSpy.wait());
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.first().at(0).value<quint32>(), server.display->serial());
    QCOMPARE(startSpy.first().at(1).value<quint32>(), 1u);
    QCOMPARE(gesture->fingerCount(), 3u);
    QCOMPARE(gesture->surface().data(), surface.data());

    // Another start should not be possible.
    server_pointers.start_hold_gesture(3);
    QVERIFY(!startSpy.wait(200));

    // Now end or cancel.
    QFETCH(bool, cancel);
    QSignalSpy* spy;

    server.seat->setTimestamp(timestamp++);
    if (cancel) {
        server_pointers.cancel_hold_gesture();
        spy = &cancelledSpy;
    } else {
        server_pointers.end_hold_gesture();
        spy = &endSpy;
    }

    QVERIFY(spy->wait());

    QFETCH(int, expectedEndCount);
    QFETCH(int, expectedCancelCount);
    QCOMPARE(endSpy.count(), expectedEndCount);
    QCOMPARE(cancelledSpy.count(), expectedCancelCount);

    QCOMPARE(spy->count(), 1);
    QCOMPARE(spy->first().at(0).value<quint32>(), server.display->serial());
    QCOMPARE(spy->first().at(1).value<quint32>(), 2u);

    QCOMPARE(gesture->fingerCount(), 0u);
    QVERIFY(gesture->surface().isNull());

    // Now a start should be possible again.
    server.seat->setTimestamp(timestamp++);
    server_pointers.start_hold_gesture(3);
    QVERIFY(startSpy.wait());

    // Unsetting the focused pointer surface should not change anything.
    server_pointers.set_focused_surface(nullptr);
    server.seat->setTimestamp(timestamp++);

    // And end.
    server.seat->setTimestamp(timestamp++);

    if (cancel) {
        server_pointers.cancel_hold_gesture();
    } else {
        server_pointers.end_hold_gesture();
    }

    QVERIFY(spy->wait());
}

QTEST_GUILESS_MAIN(pointer_gestures_test)
#include "pointer_gestures_v1.moc"
