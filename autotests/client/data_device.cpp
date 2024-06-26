/********************************************************************
Copyright © 2014 Martin Gräßlin <mgraesslin@kde.org>
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
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/pointer.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/data_device.h"
#include "../../server/data_device_manager.h"
#include "../../server/data_source.h"
#include "../../server/display.h"
#include "../../server/drag_pool.h"
#include "../../server/pointer_pool.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

#include "../../tests/globals.h"

#include <QtTest>
#include <wayland-client.h>

class TestDataDevice : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_create();
    void test_drag_data();
    void test_drag();
    void test_drag_internally_data();
    void test_drag_internally();
    void test_set_selection();
    void test_send_selection_on_seat();
    void test_replace_source();
    void test_destroy();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::DataDeviceManager* m_device_manager = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    QThread* m_thread = nullptr;
};

constexpr auto socket_name{"wrapland-test-wayland-datadevice-0"};

void TestDataDevice::init()
{
    qRegisterMetaType<Wrapland::Server::data_device*>();
    qRegisterMetaType<Wrapland::Server::data_source*>();
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy established_spy(m_connection,
                               &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(established_spy.wait());

    m_queue = new Wrapland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Wrapland::Client::Registry registry;
    QSignalSpy device_manager_spy(&registry, SIGNAL(dataDeviceManagerAnnounced(quint32, quint32)));
    QVERIFY(device_manager_spy.isValid());
    QSignalSpy seat_spy(&registry, SIGNAL(seatAnnounced(quint32, quint32)));
    QVERIFY(seat_spy.isValid());
    QSignalSpy compositor_spy(&registry, SIGNAL(compositorAnnounced(quint32, quint32)));
    QVERIFY(compositor_spy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    server.globals.data_device_manager
        = std::make_unique<Wrapland::Server::data_device_manager>(server.display.get());
    QVERIFY(device_manager_spy.wait());
    m_device_manager
        = registry.createDataDeviceManager(device_manager_spy.first().first().value<quint32>(),
                                           device_manager_spy.first().last().value<quint32>(),
                                           this);

    server.globals.seats.emplace_back(
        std::make_unique<Wrapland::Server::Seat>(server.display.get()));
    server.seat = server.globals.seats.back().get();
    server.seat->setHasPointer(true);

    QVERIFY(seat_spy.wait());
    m_seat = registry.createSeat(
        seat_spy.first().first().value<quint32>(), seat_spy.first().last().value<quint32>(), this);
    QVERIFY(m_seat->isValid());
    QSignalSpy pointer_changed_spy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointer_changed_spy.isValid());
    QVERIFY(pointer_changed_spy.wait());

    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());

    QVERIFY(compositor_spy.wait());
    m_compositor = registry.createCompositor(compositor_spy.first().first().value<quint32>(),
                                             compositor_spy.first().last().value<quint32>(),
                                             this);
    QVERIFY(m_compositor->isValid());
}

void TestDataDevice::cleanup()
{
    if (m_device_manager) {
        delete m_device_manager;
        m_device_manager = nullptr;
    }
    if (m_seat) {
        delete m_seat;
        m_seat = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    m_connection->deleteLater();
    m_thread->quit();
    m_thread->wait();
    delete m_thread;

    m_thread = nullptr;
    m_connection = nullptr;

    server = {};
}

void TestDataDevice::test_create()
{
    QSignalSpy device_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::device_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> device(m_device_manager->getDevice(m_seat));
    QVERIFY(device->isValid());

    QVERIFY(device_created_spy.wait());
    QCOMPARE(device_created_spy.count(), 1);

    auto server_device = device_created_spy.first().first().value<Wrapland::Server::data_device*>();
    QVERIFY(server_device);
    QCOMPARE(server_device->seat(), server.seat);
    QVERIFY(!server_device->selection());

    QVERIFY(!server.seat->selection());
    server.seat->setSelection(server_device->selection());
    QCOMPARE(server.seat->selection(), server_device->selection());

    // and destroy
    QSignalSpy destroyedSpy(server_device, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    device.reset();
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!server.seat->selection());
}

void TestDataDevice::test_drag_data()
{
    QTest::addColumn<bool>("hasGrab");
    QTest::addColumn<bool>("hasPointerFocus");
    QTest::addColumn<bool>("success");

    QTest::newRow("grab and focus") << true << true << true;
    QTest::newRow("no grab") << false << true << false;
    QTest::newRow("no focus") << true << false << false;
    QTest::newRow("no grab, no focus") << false << false << false;
}

void TestDataDevice::test_drag()
{
    std::unique_ptr<Wrapland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy device_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::device_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> device(m_device_manager->getDevice(m_seat));
    QVERIFY(device->isValid());

    QVERIFY(device_created_spy.wait());
    QCOMPARE(device_created_spy.count(), 1);
    auto server_device = device_created_spy.first().first().value<Wrapland::Server::data_device*>();
    QVERIFY(server_device);

    QSignalSpy source_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::source_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());

    QVERIFY(source_created_spy.wait());
    QCOMPARE(source_created_spy.count(), 1);
    auto sourceInterface
        = source_created_spy.first().first().value<Wrapland::Server::data_source*>();
    QVERIFY(sourceInterface);

    QSignalSpy surface_created_spy(server.globals.compositor.get(),
                                   &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surface_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(surface_created_spy.wait());
    QCOMPARE(surface_created_spy.count(), 1);
    auto surfaceInterface = surface_created_spy.first().first().value<Wrapland::Server::Surface*>();

    // now we have all we need to start a drag operation
    QSignalSpy drag_started_spy(server.seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());
    QSignalSpy drag_entered_spy(device.get(), &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());

    // first we need to fake the pointer enter
    QFETCH(bool, hasGrab);
    QFETCH(bool, hasPointerFocus);
    QFETCH(bool, success);
    if (!hasGrab) {
        // in case we don't have grab, still generate a pointer serial to make it more interesting
        server.seat->pointers().button_pressed(Qt::LeftButton);
    }
    if (hasPointerFocus) {
        server.seat->pointers().set_focused_surface(surfaceInterface);
    }
    if (hasGrab) {
        server.seat->pointers().button_pressed(Qt::LeftButton);
    }

    // TODO: This test would be better, if it could also test that a client trying to guess
    //       the last serial of a different client can't start a drag.
    auto const pointerButtonSerial
        = success ? server.seat->pointers().button_serial(Qt::LeftButton) : 0;

    QCoreApplication::processEvents();

    // finally start the drag
    device->startDrag(pointerButtonSerial, source.get(), surface.get());
    QCOMPARE(drag_started_spy.wait(500), success);
    QCOMPARE(!drag_started_spy.isEmpty(), success);
    QCOMPARE(server.seat->drags().get_source().src, success ? sourceInterface : nullptr);
    QCOMPARE(server.seat->drags().get_source().surfaces.origin,
             success ? surfaceInterface : nullptr);
    QVERIFY(!server.seat->drags().get_source().surfaces.icon);

    if (success) {
        // Wait for the drag-enter on itself, otherwise we leak the data offer.
        // There also seem to be no way to eliminate this issue. If the client closes the connection
        // at the moment a drag enters (and afterwards a data offer is sent) the memory is lost.
        // Must this be solved in libwayland?
        QVERIFY(drag_entered_spy.count() || drag_entered_spy.wait());
    }
}

void TestDataDevice::test_drag_internally_data()
{
    QTest::addColumn<bool>("hasGrab");
    QTest::addColumn<bool>("hasPointerFocus");
    QTest::addColumn<bool>("success");

    QTest::newRow("grab and focus") << true << true << true;
    QTest::newRow("no grab") << false << true << false;
    QTest::newRow("no focus") << true << false << false;
    QTest::newRow("no grab, no focus") << false << false << false;
}

void TestDataDevice::test_drag_internally()
{
    std::unique_ptr<Wrapland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy device_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::device_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> device(m_device_manager->getDevice(m_seat));
    QVERIFY(device->isValid());

    QVERIFY(device_created_spy.wait());
    QCOMPARE(device_created_spy.count(), 1);
    auto server_device = device_created_spy.first().first().value<Wrapland::Server::data_device*>();
    QVERIFY(server_device);

    QSignalSpy surface_created_spy(server.globals.compositor.get(),
                                   SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(surface_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(surface_created_spy.wait());
    QCOMPARE(surface_created_spy.count(), 1);
    auto surfaceInterface = surface_created_spy.first().first().value<Wrapland::Server::Surface*>();

    std::unique_ptr<Wrapland::Client::Surface> iconSurface(m_compositor->createSurface());
    QVERIFY(iconSurface->isValid());

    QVERIFY(surface_created_spy.wait());
    QCOMPARE(surface_created_spy.count(), 2);
    auto iconSurfaceInterface
        = surface_created_spy.last().first().value<Wrapland::Server::Surface*>();

    // now we have all we need to start a drag operation
    QSignalSpy drag_started_spy(server.seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());

    // first we need to fake the pointer enter
    QFETCH(bool, hasGrab);
    QFETCH(bool, hasPointerFocus);
    QFETCH(bool, success);
    if (!hasGrab) {
        // in case we don't have grab, still generate a pointer serial to make it more interesting
        server.seat->pointers().button_pressed(Qt::LeftButton);
    }
    if (hasPointerFocus) {
        server.seat->pointers().set_focused_surface(surfaceInterface);
    }
    if (hasGrab) {
        server.seat->pointers().button_pressed(Qt::LeftButton);
    }

    // TODO: This test would be better, if it could also test that a client trying to guess
    //       the last serial of a different client can't start a drag.
    quint32 const pointerButtonSerial
        = success ? server.seat->pointers().button_serial(Qt::LeftButton) : 0;

    QCoreApplication::processEvents();
    // finally start the internal drag
    device->startDragInternally(pointerButtonSerial, surface.get(), iconSurface.get());
    QCOMPARE(drag_started_spy.wait(500), success);
    QCOMPARE(!drag_started_spy.isEmpty(), success);
    QVERIFY(!server.seat->drags().get_source().src);
    QCOMPARE(server.seat->drags().get_source().surfaces.origin,
             success ? surfaceInterface : nullptr);
    QCOMPARE(server.seat->drags().get_source().surfaces.icon,
             success ? iconSurfaceInterface : nullptr);
}

void TestDataDevice::test_set_selection()
{
    std::unique_ptr<Wrapland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy device_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::device_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> device(m_device_manager->getDevice(m_seat));
    QVERIFY(device->isValid());

    QVERIFY(device_created_spy.wait());
    QCOMPARE(device_created_spy.count(), 1);
    auto server_device = device_created_spy.first().first().value<Wrapland::Server::data_device*>();
    QVERIFY(server_device);

    QSignalSpy source_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::source_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());
    source->offer(QStringLiteral("text/plain"));

    QVERIFY(source_created_spy.wait());
    QCOMPARE(source_created_spy.count(), 1);
    auto serverSource = source_created_spy.first().first().value<Wrapland::Server::data_source*>();
    QVERIFY(serverSource);

    // everything setup, now we can test setting the selection
    QSignalSpy selectionChangedSpy(server_device,
                                   &Wrapland::Server::data_device::selection_changed);
    QVERIFY(selectionChangedSpy.isValid());

    QVERIFY(!server_device->selection());
    device->setSelection(1, source.get());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(server_device->selection(), serverSource);

    // Send selection to datadevice.
    QSignalSpy selection_offered_spy(device.get(), &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selection_offered_spy.isValid());
    server_device->send_selection(server_device->selection());
    QVERIFY(selection_offered_spy.wait());
    QCOMPARE(selection_offered_spy.count(), 1);

    auto dataOffer = selection_offered_spy.first().first().value<Wrapland::Client::DataOffer*>();
    QVERIFY(dataOffer);
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 1);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // sending a new mimetype to the selection, should be announced in the offer
    QSignalSpy mimeTypeAddedSpy(dataOffer, SIGNAL(mimeTypeOffered(QString)));
    QVERIFY(mimeTypeAddedSpy.isValid());
    source->offer("text/html");
    QVERIFY(mimeTypeAddedSpy.wait());
    QCOMPARE(mimeTypeAddedSpy.count(), 1);
    QCOMPARE(mimeTypeAddedSpy.first().first().toString(), QStringLiteral("text/html"));
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 2);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QCOMPARE(dataOffer->offeredMimeTypes().last().name(), QStringLiteral("text/html"));

    // now clear the selection
    device->setSelection(1, nullptr);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 2);
    QVERIFY(!server_device->selection());

    // set another selection
    device->setSelection(2, source.get());
    QVERIFY(selectionChangedSpy.wait());

    // Now unbind the data device.
    QSignalSpy unboundSpy(server_device, &Wrapland::Server::data_device::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());
    device.reset();
    QVERIFY(unboundSpy.wait());

    // TODO: This should be impossible to happen in the first place, correct?
#if 0
    // send a selection to the unbound data device
    server_device->send_selection(server_device);
#endif
}

void TestDataDevice::test_send_selection_on_seat()
{
    // This test verifies that the selection is sent when setting a focused keyboard.

    // First add keyboard support to Seat.
    QSignalSpy keyboardChangedSpy(m_seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    server.seat->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());

    // Now create DataDevice, Keyboard and a Surface.
    QSignalSpy device_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::device_created);
    QVERIFY(device_created_spy.isValid());
    std::unique_ptr<Wrapland::Client::DataDevice> device(m_device_manager->getDevice(m_seat));
    QVERIFY(device->isValid());
    QVERIFY(device_created_spy.wait());
    auto server_device = device_created_spy.first().first().value<Wrapland::Server::data_device*>();
    QVERIFY(server_device);
    std::unique_ptr<Wrapland::Client::Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surface_created_spy(server.globals.compositor.get(),
                                   &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surface_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surface_created_spy.wait());

    auto serverSurface = surface_created_spy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    server.seat->setFocusedKeyboardSurface(serverSurface);

    // Now set the selection.
    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());
    source->offer(QStringLiteral("text/plain"));
    device->setSelection(1, source.get());

    // We should get a selection offered for that on the data device.
    QSignalSpy selection_offered_spy(device.get(), &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selection_offered_spy.isValid());
    QVERIFY(selection_offered_spy.wait());
    QCOMPARE(selection_offered_spy.count(), 1);

    // Now unfocus the keyboard.
    server.seat->setFocusedKeyboardSurface(nullptr);
    // if setting the same surface again, we should get another offer
    server.seat->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(selection_offered_spy.wait());
    QCOMPARE(selection_offered_spy.count(), 2);

    // Now let's try to destroy the data device and set a focused keyboard just while the data
    // device is being destroyed.
    server.seat->setFocusedKeyboardSurface(nullptr);
    QSignalSpy unboundSpy(server_device, &Wrapland::Server::data_device::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());
    device.reset();
    QVERIFY(unboundSpy.wait());
    server.seat->setFocusedKeyboardSurface(serverSurface);
}

void TestDataDevice::test_replace_source()
{
    // This test verifies that replacing a data source cancels the previous source.

    // First add keyboard support to Seat.
    QSignalSpy keyboardChangedSpy(m_seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    server.seat->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());

    // Now create DataDevice, Keyboard and a Surface.
    QSignalSpy device_created_spy(server.globals.data_device_manager.get(),
                                  &Wrapland::Server::data_device_manager::device_created);
    QVERIFY(device_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> device(m_device_manager->getDevice(m_seat));
    QVERIFY(device->isValid());
    QVERIFY(device_created_spy.wait());
    auto server_device = device_created_spy.first().first().value<Wrapland::Server::data_device*>();
    QVERIFY(server_device);
    std::unique_ptr<Wrapland::Client::Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surface_created_spy(server.globals.compositor.get(),
                                   &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surface_created_spy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surface_created_spy.wait());

    auto serverSurface = surface_created_spy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    server.seat->setFocusedKeyboardSurface(serverSurface);

    // Now set the selection.
    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());
    source->offer(QStringLiteral("text/plain"));
    device->setSelection(1, source.get());
    QSignalSpy sourceCancelledSpy(source.get(), &Wrapland::Client::DataSource::cancelled);
    QVERIFY(sourceCancelledSpy.isValid());

    // We should get a selection offered for that on the data device.
    QSignalSpy selection_offered_spy(device.get(), &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selection_offered_spy.isValid());
    QVERIFY(selection_offered_spy.wait());
    QCOMPARE(selection_offered_spy.count(), 1);

    // Create a second data source and replace previous one.
    std::unique_ptr<Wrapland::Client::DataSource> source_2(m_device_manager->createSource());
    QVERIFY(source_2->isValid());

    source_2->offer(QStringLiteral("text/plain"));

    QSignalSpy sourceCancelled2Spy(source_2.get(), &Wrapland::Client::DataSource::cancelled);
    QVERIFY(sourceCancelled2Spy.isValid());

    device->setSelection(1, source_2.get());
    QCOMPARE(selection_offered_spy.count(), 1);

    QVERIFY(sourceCancelledSpy.wait());
    QTRY_COMPARE(selection_offered_spy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // Replace the data source with itself, ensure that it did not get cancelled.
    device->setSelection(1, source_2.get());
    QVERIFY(!sourceCancelled2Spy.wait(500));
    QCOMPARE(selection_offered_spy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // Create a new DataDevice and replace previous one.
    std::unique_ptr<Wrapland::Client::DataDevice> device_2(m_device_manager->getDevice(m_seat));
    QVERIFY(device_2->isValid());
    std::unique_ptr<Wrapland::Client::DataSource> source_3(m_device_manager->createSource());
    QVERIFY(source_3->isValid());
    source_3->offer(QStringLiteral("text/plain"));
    device_2->setSelection(1, source_3.get());
    QVERIFY(sourceCancelled2Spy.wait());

    // Try to crash by first destroying source_3 and setting a new DataSource.
    std::unique_ptr<Wrapland::Client::DataSource> source_4(m_device_manager->createSource());
    QVERIFY(source_4->isValid());
    source_4->offer("text/plain");
    source_3.reset();
    device_2->setSelection(1, source_4.get());
    QVERIFY(selection_offered_spy.wait());
}

void TestDataDevice::test_destroy()
{
    std::unique_ptr<Wrapland::Client::DataDevice> device(m_device_manager->getDevice(m_seat));
    QVERIFY(device->isValid());

    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_device_manager,
            &Wrapland::Client::DataDeviceManager::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_seat,
            &Wrapland::Client::Seat::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_compositor,
            &Wrapland::Client::Compositor::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            device.get(),
            &Wrapland::Client::DataDevice::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_queue,
            &Wrapland::Client::EventQueue::release);

    server = {};
    QTRY_VERIFY(!m_connection->established());

    // Now the data device should be destroyed.
    QTRY_VERIFY(!device->isValid());

    // Calling destroy again should not fail.
    device->release();
}

QTEST_GUILESS_MAIN(TestDataDevice)
#include "data_device.moc"
