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

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/pointer.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/surface.h"
#include "../../src/client/touch.h"

#include "../../server/compositor.h"
#include "../../server/data_device.h"
#include "../../server/data_device_manager.h"
#include "../../server/data_source.h"
#include "../../server/display.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

class TestDragAndDrop : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void test_pointer();
    void test_touch();
    void test_cancel_by_destroyed_data_source();
    void test_pointer_events_ignored();

private:
    Wrapland::Client::Surface* create_surface();
    Wrapland::Server::Surface* get_server_surface();

    Wrapland::Server::Display* m_display = nullptr;
    Wrapland::Server::Compositor* m_server_compositor = nullptr;
    Wrapland::Server::DataDeviceManager* m_server_device_manager = nullptr;
    Wrapland::Server::Seat* m_server_seat = nullptr;

    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    Wrapland::Client::DataDevice* m_device = nullptr;
    Wrapland::Client::DataSource* m_source = nullptr;
    QThread* m_thread = nullptr;
    Wrapland::Client::Registry* m_registry = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::Pointer* m_pointer = nullptr;
    Wrapland::Client::Touch* m_touch = nullptr;
    Wrapland::Client::DataDeviceManager* m_ddm = nullptr;
    Wrapland::Client::ShmPool* m_shm = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-drag-n-drop-0");

void TestDragAndDrop::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    m_display = new Wrapland::Server::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connected_spy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connected_spy.isValid());
    m_connection->setSocketName(s_socketName);

    m_server_compositor = m_display->createCompositor(m_display);
    m_server_seat = m_display->createSeat(m_display);
    m_server_seat->setHasPointer(true);
    m_server_seat->setHasTouch(true);

    m_server_device_manager = m_display->createDataDeviceManager(m_display);
    m_display->createShm();

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connected_spy.count() || connected_spy.wait());
    QCOMPARE(connected_spy.count(), 1);

    m_queue = new Wrapland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Wrapland::Client::Registry();
    QSignalSpy interfaces_announced_spy(m_registry,
                                        &Wrapland::Client::Registry::interfaceAnnounced);
    QVERIFY(interfaces_announced_spy.isValid());

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    QVERIFY(interfaces_announced_spy.wait());
#define CREATE(variable, factory, iface)                                                           \
    variable = m_registry->create##factory(                                                        \
        m_registry->interface(Wrapland::Client::Registry::Interface::iface).name,                  \
        m_registry->interface(Wrapland::Client::Registry::Interface::iface).version,               \
        this);                                                                                     \
    QVERIFY(variable);

    CREATE(m_compositor, Compositor, Compositor)
    CREATE(m_seat, Seat, Seat)
    CREATE(m_ddm, DataDeviceManager, DataDeviceManager)
    CREATE(m_shm, ShmPool, Shm)

#undef CREATE

    QSignalSpy pointerSpy(m_seat, &Wrapland::Client::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    QVERIFY(pointerSpy.wait());
    m_pointer = m_seat->createPointer(m_seat);
    QVERIFY(m_pointer->isValid());
    m_touch = m_seat->createTouch(m_seat);
    QVERIFY(m_touch->isValid());
    m_device = m_ddm->getDataDevice(m_seat, this);
    QVERIFY(m_device->isValid());
    m_source = m_ddm->createDataSource(this);
    QVERIFY(m_source->isValid());
    m_source->offer(QStringLiteral("text/plain"));
}

void TestDragAndDrop::cleanup()
{
#define DELETE(name)                                                                               \
    if (name) {                                                                                    \
        delete name;                                                                               \
        name = nullptr;                                                                            \
    }
    DELETE(m_source)
    DELETE(m_device)
    DELETE(m_shm)
    DELETE(m_compositor)
    DELETE(m_ddm)
    DELETE(m_seat)
    DELETE(m_queue)
    DELETE(m_registry)
#undef DELETE
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

Wrapland::Client::Surface* TestDragAndDrop::create_surface()
{
    auto s = m_compositor->createSurface();

    QImage img(QSize(100, 200), QImage::Format_RGB32);
    img.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(img));
    s->damage(QRect(0, 0, 100, 200));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    return s;
}

Wrapland::Server::Surface* TestDragAndDrop::get_server_surface()
{
    QSignalSpy surface_created_spy(m_server_compositor,
                                   &Wrapland::Server::Compositor::surfaceCreated);
    if (!surface_created_spy.isValid()) {
        return nullptr;
    }
    if (!surface_created_spy.wait(500)) {
        return nullptr;
    }
    return surface_created_spy.first().first().value<Wrapland::Server::Surface*>();
}

void TestDragAndDrop::test_pointer()
{
    // This test verifies the very basic drag and drop on one surface, an enter, a move and the
    // drop.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> surface(create_surface());
    auto server_surface = get_server_surface();
    QVERIFY(server_surface);

    QSignalSpy source_selected_action_changed_spy(
        m_source, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(source_selected_action_changed_spy.isValid());

    // now we need to pass pointer focus to the Surface and simulate a button press
    QSignalSpy button_press_spy(m_pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(button_press_spy.isValid());
    m_server_seat->setFocusedPointerSurface(server_surface);
    m_server_seat->setTimestamp(2);
    m_server_seat->pointerButtonPressed(1);
    QVERIFY(button_press_spy.wait());
    QCOMPARE(button_press_spy.first().at(1).value<quint32>(), quint32(2));

    // add some signal spies for client side
    QSignalSpy drag_entered_spy(m_device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());
    QSignalSpy drag_motion_spy(m_device, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(drag_motion_spy.isValid());
    QSignalSpy pointer_motion_spy(m_pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointer_motion_spy.isValid());
    QSignalSpy source_drop_spy(m_source, &Wrapland::Client::DataSource::dragAndDropPerformed);
    QVERIFY(source_drop_spy.isValid());

    // now we can start the drag and drop
    QSignalSpy drag_started_spy(m_server_seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());
    m_source->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                    | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    m_device->startDrag(button_press_spy.first().first().value<quint32>(), m_source, surface.get());
    QVERIFY(drag_started_spy.wait());
    QCOMPARE(m_server_seat->dragSurface(), server_surface);
    QCOMPARE(m_server_seat->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_server_seat->dragSource()->icon());
    QCOMPARE(m_server_seat->dragSource()->dragImplicitGrabSerial(),
             button_press_spy.first().first().value<quint32>());
    QVERIFY(drag_entered_spy.wait());
    QCOMPARE(drag_entered_spy.count(), 1);
    QCOMPARE(drag_entered_spy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(drag_entered_spy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_device->dragSurface().data(), surface.get());

    auto offer = m_device->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offer_action_changed_spy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offer_action_changed_spy.isValid());
    QCOMPARE(m_device->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_device->dragOffer()->offeredMimeTypes().first().name(),
             QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(),
                 Wrapland::Client::DataDeviceManager::DnDAction::Copy
                     | Wrapland::Client::DataDeviceManager::DnDAction::Move);

    offer->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                     | Wrapland::Client::DataDeviceManager::DnDAction::Move,
                                 Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offer_action_changed_spy.wait());
    QCOMPARE(offer_action_changed_spy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(source_selected_action_changed_spy.count(), 1);
    QCOMPARE(m_source->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // Simulate motion.
    m_server_seat->setTimestamp(3);
    m_server_seat->setPointerPos(QPointF(3, 3));
    QVERIFY(drag_motion_spy.wait());
    QCOMPARE(drag_motion_spy.count(), 1);
    QCOMPARE(drag_motion_spy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(drag_motion_spy.first().last().toUInt(), 3u);

    // Simulate drop.
    QSignalSpy server_drag_ended_spy(m_server_seat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(server_drag_ended_spy.isValid());
    QSignalSpy dropped_spy(m_device, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(dropped_spy.isValid());
    m_server_seat->setTimestamp(4);
    m_server_seat->pointerButtonReleased(1);
    QVERIFY(source_drop_spy.isEmpty());
    QVERIFY(dropped_spy.wait());
    QCOMPARE(source_drop_spy.count(), 1);
    QCOMPARE(server_drag_ended_spy.count(), 1);

    QSignalSpy finished_spy(m_source, &Wrapland::Client::DataSource::dragAndDropFinished);
    QVERIFY(finished_spy.isValid());
    offer->dragAndDropFinished();
    QVERIFY(finished_spy.wait());
    delete offer;

    // Verify that we did not get any further input events.
    QVERIFY(pointer_motion_spy.isEmpty());
    QCOMPARE(button_press_spy.count(), 1);
}

void TestDragAndDrop::test_touch()
{
    // This test verifies the very basic drag and drop on one surface, an enter, a move and the
    // drop.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> s(create_surface());
    s->setSize(QSize(100, 100));
    auto server_surface = get_server_surface();
    QVERIFY(server_surface);

    QSignalSpy source_selected_action_changed_spy(
        m_source, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(source_selected_action_changed_spy.isValid());

    // now we need to pass touch focus to the Surface and simulate a touch down
    QSignalSpy sequence_started_spy(m_touch, &Wrapland::Client::Touch::sequenceStarted);
    QVERIFY(sequence_started_spy.isValid());
    QSignalSpy point_added_spy(m_touch, &Wrapland::Client::Touch::pointAdded);
    QVERIFY(point_added_spy.isValid());
    m_server_seat->setFocusedTouchSurface(server_surface);
    m_server_seat->setTimestamp(2);
    const qint32 touchId = m_server_seat->touchDown(QPointF(50, 50));
    QVERIFY(sequence_started_spy.wait());

    auto tp{sequence_started_spy.first().at(0).value<Wrapland::Client::TouchPoint*>()};
    QVERIFY(tp);
    QCOMPARE(tp->time(), quint32(2));

    // add some signal spies for client side
    QSignalSpy drag_entered_spy(m_device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());
    QSignalSpy drag_motion_spy(m_device, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(drag_motion_spy.isValid());
    QSignalSpy touch_motion_spy(m_touch, &Wrapland::Client::Touch::pointMoved);
    QVERIFY(touch_motion_spy.isValid());
    QSignalSpy source_drop_spy(m_source, &Wrapland::Client::DataSource::dragAndDropPerformed);
    QVERIFY(source_drop_spy.isValid());

    // now we can start the drag and drop
    QSignalSpy drag_started_spy(m_server_seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());
    m_source->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                    | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    m_device->startDrag(tp->downSerial(), m_source, s.get());
    QVERIFY(drag_started_spy.wait());
    QCOMPARE(m_server_seat->dragSurface(), server_surface);
    QCOMPARE(m_server_seat->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_server_seat->dragSource()->icon());
    QCOMPARE(m_server_seat->dragSource()->dragImplicitGrabSerial(), tp->downSerial());
    QVERIFY(drag_entered_spy.wait());
    QCOMPARE(drag_entered_spy.count(), 1);
    QCOMPARE(drag_entered_spy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(drag_entered_spy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_device->dragSurface().data(), s.get());
    auto offer = m_device->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offer_action_changed_spy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offer_action_changed_spy.isValid());
    QCOMPARE(m_device->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_device->dragOffer()->offeredMimeTypes().first().name(),
             QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(),
                 Wrapland::Client::DataDeviceManager::DnDAction::Copy
                     | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    offer->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                     | Wrapland::Client::DataDeviceManager::DnDAction::Move,
                                 Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offer_action_changed_spy.wait());
    QCOMPARE(offer_action_changed_spy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(source_selected_action_changed_spy.count(), 1);
    QCOMPARE(m_source->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_server_seat->setTimestamp(3);
    m_server_seat->touchMove(touchId, QPointF(75, 75));
    QVERIFY(drag_motion_spy.wait());
    QCOMPARE(drag_motion_spy.count(), 1);
    QCOMPARE(drag_motion_spy.first().first().toPointF(), QPointF(75, 75));
    QCOMPARE(drag_motion_spy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy server_drag_ended_spy(m_server_seat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(server_drag_ended_spy.isValid());
    QSignalSpy dropped_spy(m_device, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(dropped_spy.isValid());
    m_server_seat->setTimestamp(4);
    m_server_seat->touchUp(touchId);
    QVERIFY(source_drop_spy.isEmpty());
    QVERIFY(dropped_spy.wait());
    QCOMPARE(source_drop_spy.count(), 1);
    QCOMPARE(server_drag_ended_spy.count(), 1);

    QSignalSpy finished_spy(m_source, &Wrapland::Client::DataSource::dragAndDropFinished);
    QVERIFY(finished_spy.isValid());
    offer->dragAndDropFinished();
    QVERIFY(finished_spy.wait());
    delete offer;

    // verify that we did not get any further input events
    QVERIFY(touch_motion_spy.isEmpty());
    QCOMPARE(point_added_spy.count(), 0);
}

void TestDragAndDrop::test_cancel_by_destroyed_data_source()
{
    // This test simulates the problem from BUG 389221.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> s(create_surface());
    auto server_surface = get_server_surface();
    QVERIFY(server_surface);

    QSignalSpy source_selected_action_changed_spy(
        m_source, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(source_selected_action_changed_spy.isValid());

    // Now we need to pass pointer focus to the Surface and simulate a button press.
    QSignalSpy button_press_spy(m_pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(button_press_spy.isValid());
    m_server_seat->setFocusedPointerSurface(server_surface);
    m_server_seat->setTimestamp(2);
    m_server_seat->pointerButtonPressed(1);
    QVERIFY(button_press_spy.wait());
    QCOMPARE(button_press_spy.first().at(1).value<quint32>(), quint32(2));

    // Add some signal spies for client side.
    QSignalSpy drag_entered_spy(m_device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());
    QSignalSpy drag_motion_spy(m_device, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(drag_motion_spy.isValid());
    QSignalSpy pointer_motion_spy(m_pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointer_motion_spy.isValid());
    QSignalSpy drag_left_spy(m_device, &Wrapland::Client::DataDevice::dragLeft);
    QVERIFY(drag_left_spy.isValid());

    // Now we can start the drag and drop.
    QSignalSpy drag_started_spy(m_server_seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());

    m_source->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                    | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    m_device->startDrag(button_press_spy.first().first().value<quint32>(), m_source, s.get());

    QVERIFY(drag_started_spy.wait());
    QCOMPARE(m_server_seat->dragSurface(), server_surface);
    QCOMPARE(m_server_seat->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_server_seat->dragSource()->icon());
    QCOMPARE(m_server_seat->dragSource()->dragImplicitGrabSerial(),
             button_press_spy.first().first().value<quint32>());

    QVERIFY(drag_entered_spy.wait());
    QCOMPARE(drag_entered_spy.count(), 1);
    QCOMPARE(drag_entered_spy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(drag_entered_spy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_device->dragSurface().data(), s.get());

    auto offer = m_device->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offer_action_changed_spy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offer_action_changed_spy.isValid());
    QCOMPARE(m_device->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_device->dragOffer()->offeredMimeTypes().first().name(),
             QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(),
                 Wrapland::Client::DataDeviceManager::DnDAction::Copy
                     | Wrapland::Client::DataDeviceManager::DnDAction::Move);

    offer->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                     | Wrapland::Client::DataDeviceManager::DnDAction::Move,
                                 Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offer_action_changed_spy.wait());
    QCOMPARE(offer_action_changed_spy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(source_selected_action_changed_spy.count(), 1);
    QCOMPARE(m_source->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // Simulate motion.
    m_server_seat->setTimestamp(3);
    m_server_seat->setPointerPos(QPointF(3, 3));
    QVERIFY(drag_motion_spy.wait());
    QCOMPARE(drag_motion_spy.count(), 1);
    QCOMPARE(drag_motion_spy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(drag_motion_spy.first().last().toUInt(), 3u);

    // Now delete the DataSource.
    delete m_source;
    m_source = nullptr;
    QSignalSpy server_drag_ended_spy(m_server_seat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(server_drag_ended_spy.isValid());
    QVERIFY(drag_left_spy.isEmpty());
    QVERIFY(drag_left_spy.wait());
    QTRY_COMPARE(drag_left_spy.count(), 1);
    QTRY_COMPARE(server_drag_ended_spy.count(), 1);

    // Simulate drop.
    QSignalSpy dropped_spy(m_device, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(dropped_spy.isValid());
    m_server_seat->setTimestamp(4);
    m_server_seat->pointerButtonReleased(1);
    QVERIFY(!dropped_spy.wait(500));

    // Verify that we did not get any further input events.
    QVERIFY(pointer_motion_spy.isEmpty());
    QCOMPARE(button_press_spy.count(), 2);
}

void TestDragAndDrop::test_pointer_events_ignored()
{
    // This test verifies that all pointer events are ignored on the focused Pointer device during
    // drag.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> s(create_surface());
    auto server_surface = get_server_surface();
    QVERIFY(server_surface);

    // pass it pointer focus
    m_server_seat->setFocusedPointerSurface(server_surface);

    // create signal spies for all the pointer events
    QSignalSpy pointer_entered_spy(m_pointer, &Wrapland::Client::Pointer::entered);
    QVERIFY(pointer_entered_spy.isValid());
    QSignalSpy pointer_left_spy(m_pointer, &Wrapland::Client::Pointer::left);
    QVERIFY(pointer_left_spy.isValid());
    QSignalSpy pointer_motion_spy(m_pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointer_motion_spy.isValid());
    QSignalSpy axis_spy(m_pointer, &Wrapland::Client::Pointer::axisChanged);
    QVERIFY(axis_spy.isValid());
    QSignalSpy button_spy(m_pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(button_spy.isValid());
    QSignalSpy drag_entered_spy(m_device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());

    // first simulate a few things
    quint32 timestamp = 1;
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->setPointerPos(QPointF(10, 10));
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->pointerAxis(Qt::Vertical, 5);
    // verify that we have those
    QVERIFY(axis_spy.wait());
    QCOMPARE(axis_spy.count(), 1);
    QCOMPARE(pointer_motion_spy.count(), 1);
    QCOMPARE(pointer_entered_spy.count(), 1);
    QVERIFY(button_spy.isEmpty());
    QVERIFY(pointer_left_spy.isEmpty());

    // let's start the drag
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->pointerButtonPressed(1);
    QVERIFY(button_spy.wait());
    QCOMPARE(button_spy.count(), 1);
    m_device->startDrag(button_spy.first().first().value<quint32>(), m_source, s.get());
    QVERIFY(drag_entered_spy.wait());

    // now simulate all the possible pointer interactions
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->pointerButtonPressed(2);
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->pointerButtonReleased(2);
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->pointerAxis(Qt::Vertical, 5);
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->pointerAxis(Qt::Horizontal, 5);
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->setFocusedPointerSurface(nullptr);
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->setFocusedPointerSurface(server_surface);
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->setPointerPos(QPointF(50, 50));

    // last but not least, simulate the drop
    QSignalSpy dropped_spy(m_device, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(dropped_spy.isValid());
    m_server_seat->setTimestamp(timestamp++);
    m_server_seat->pointerButtonReleased(1);
    QVERIFY(dropped_spy.wait());

    // all the changes should have been ignored
    QCOMPARE(axis_spy.count(), 1);
    QCOMPARE(pointer_motion_spy.count(), 1);
    QCOMPARE(pointer_entered_spy.count(), 1);
    QCOMPARE(button_spy.count(), 1);
    QVERIFY(pointer_left_spy.isEmpty());
}

QTEST_GUILESS_MAIN(TestDragAndDrop)
#include "drag_and_drop.moc"
