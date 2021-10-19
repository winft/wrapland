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

#include "../../server/data_device.h"
#include "../../server/data_source.h"
#include "../../server/display.h"
#include "../../server/drag_pool.h"
#include "../../server/globals.h"
#include "../../server/pointer_pool.h"
#include "../../server/surface.h"
#include "../../server/touch_pool.h"

class TestDragAndDrop : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void test_pointer();
    void test_touch();
    void test_cancel_by_destroyed_data_source();
    void test_target_removed();
    void test_pointer_events_ignored();

private:
    struct Client;
    Wrapland::Client::Surface* create_surface(Client& client);
    Wrapland::Server::Surface* get_server_surface();

    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

    struct Client {
        Wrapland::Client::ConnectionThread* connection = nullptr;
        QThread* thread = nullptr;
        Wrapland::Client::EventQueue* queue = nullptr;
        Wrapland::Client::Compositor* compositor = nullptr;
        Wrapland::Client::Registry* registry = nullptr;
        Wrapland::Client::DataDevice* device = nullptr;
        Wrapland::Server::data_device* server_device{nullptr};
        Wrapland::Client::DataSource* source = nullptr;
        Wrapland::Client::Seat* seat = nullptr;
        Wrapland::Client::Pointer* pointer = nullptr;
        Wrapland::Client::Touch* touch = nullptr;
        Wrapland::Client::DataDeviceManager* ddm = nullptr;
        Wrapland::Client::ShmPool* shm = nullptr;
    } c_1, c_2;
    Client* clients[2] = {&c_1, &c_2};
};

constexpr auto socket_name{"wrapland-test-wayland-drag-n-drop-0"};

void TestDragAndDrop::init()
{
    qRegisterMetaType<Wrapland::Server::data_device*>();
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.globals.compositor = server.display->createCompositor();

    server.globals.seats.push_back(server.display->createSeat());
    server.seat = server.globals.seats.back().get();
    server.seat->setHasPointer(true);
    server.seat->setHasTouch(true);

    server.globals.data_device_manager = server.display->createDataDeviceManager();
    server.display->createShm();

    for (auto client : clients) {
        // setup connection
        client->connection = new Wrapland::Client::ConnectionThread;
        QSignalSpy connected_spy(client->connection,
                                 &Wrapland::Client::ConnectionThread::establishedChanged);
        QVERIFY(connected_spy.isValid());
        client->connection->setSocketName(socket_name);

        client->thread = new QThread(this);
        client->connection->moveToThread(client->thread);
        client->thread->start();

        client->connection->establishConnection();
        QVERIFY(connected_spy.count() || connected_spy.wait());
        QCOMPARE(connected_spy.count(), 1);

        client->queue = new Wrapland::Client::EventQueue(this);
        QVERIFY(!client->queue->isValid());
        client->queue->setup(client->connection);
        QVERIFY(client->queue->isValid());

        client->registry = new Wrapland::Client::Registry();
        QSignalSpy interfaces_announced_spy(client->registry,
                                            &Wrapland::Client::Registry::interfaceAnnounced);
        QVERIFY(interfaces_announced_spy.isValid());

        QVERIFY(!client->registry->eventQueue());
        client->registry->setEventQueue(client->queue);
        QCOMPARE(client->registry->eventQueue(), client->queue);
        client->registry->create(client->connection);
        QVERIFY(client->registry->isValid());
        client->registry->setup();

        QVERIFY(interfaces_announced_spy.wait());
#define CREATE(variable, factory, iface)                                                           \
    variable = client->registry->create##factory(                                                  \
        client->registry->interface(Wrapland::Client::Registry::Interface::iface).name,            \
        client->registry->interface(Wrapland::Client::Registry::Interface::iface).version,         \
        this);                                                                                     \
    QVERIFY(variable);

        CREATE(client->compositor, Compositor, Compositor)
        CREATE(client->seat, Seat, Seat)
        CREATE(client->ddm, DataDeviceManager, DataDeviceManager)
        CREATE(client->shm, ShmPool, Shm)

#undef CREATE

        QSignalSpy pointerSpy(client->seat, &Wrapland::Client::Seat::hasPointerChanged);
        QVERIFY(pointerSpy.isValid());
        QVERIFY(pointerSpy.wait());
        client->pointer = client->seat->createPointer(client->seat);
        QVERIFY(client->pointer->isValid());
        client->touch = client->seat->createTouch(client->seat);
        QVERIFY(client->touch->isValid());

        QSignalSpy device_created_spy(server.globals.data_device_manager.get(),
                                      &Wrapland::Server::data_device_manager::device_created);
        QVERIFY(device_created_spy.isValid());
        client->device = client->ddm->getDevice(client->seat, this);
        QVERIFY(client->device->isValid());

        QVERIFY(device_created_spy.wait());
        QCOMPARE(device_created_spy.count(), 1);
        client->server_device
            = device_created_spy.first().first().value<Wrapland::Server::data_device*>();
        QVERIFY(client->server_device);

        client->source = client->ddm->createSource(this);
        QVERIFY(client->source->isValid());
        client->source->offer(QStringLiteral("text/plain"));
    }
}

void TestDragAndDrop::cleanup()
{
    for (auto client : clients) {
#define DELETE(name)                                                                               \
    if (name) {                                                                                    \
        delete name;                                                                               \
        name = nullptr;                                                                            \
    }
        DELETE(client->source)
        DELETE(client->device)
        DELETE(client->shm)
        DELETE(client->compositor)
        DELETE(client->ddm)
        DELETE(client->seat)
        DELETE(client->queue)
        DELETE(client->registry)
#undef DELETE
        if (client->thread) {
            client->thread->quit();
            client->thread->wait();
            delete client->thread;
            client->thread = nullptr;
        }
        delete client->connection;
        client->connection = nullptr;
    }

    server = {};
}

Wrapland::Client::Surface* TestDragAndDrop::create_surface(Client& client)
{
    auto surface = client.compositor->createSurface();

    QImage img(QSize(100, 200), QImage::Format_RGB32);
    img.fill(Qt::red);
    surface->attachBuffer(client.shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 200));
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    return surface;
}

Wrapland::Server::Surface* TestDragAndDrop::get_server_surface()
{
    QSignalSpy surface_created_spy(server.globals.compositor.get(),
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
    std::unique_ptr<Wrapland::Client::Surface> surface(create_surface(c_1));
    auto server_surface = get_server_surface();
    QVERIFY(server_surface);

    QSignalSpy source_selected_action_changed_spy(
        c_1.source, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(source_selected_action_changed_spy.isValid());

    // now we need to pass pointer focus to the Surface and simulate a button press
    QSignalSpy button_press_spy(c_1.pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(button_press_spy.isValid());
    server.seat->pointers().set_focused_surface(server_surface);
    server.seat->setTimestamp(2);
    server.seat->pointers().button_pressed(1);
    QVERIFY(button_press_spy.wait());
    QCOMPARE(button_press_spy.first().at(1).value<quint32>(), quint32(2));

    // add some signal spies for client side
    QSignalSpy drag_entered_spy(c_1.device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());
    QSignalSpy drag_motion_spy(c_1.device, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(drag_motion_spy.isValid());
    QSignalSpy pointer_motion_spy(c_1.pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointer_motion_spy.isValid());
    QSignalSpy source_drop_spy(c_1.source, &Wrapland::Client::DataSource::dragAndDropPerformed);
    QVERIFY(source_drop_spy.isValid());

    // now we can start the drag and drop
    QSignalSpy drag_started_spy(server.seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());
    c_1.source->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                      | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    c_1.device->startDrag(
        button_press_spy.first().first().value<quint32>(), c_1.source, surface.get());
    QVERIFY(drag_started_spy.wait());

    auto& server_drags = server.seat->drags();
    QCOMPARE(server_drags.get_target().surface, server_surface);
    QCOMPARE(server_drags.get_target().transformation, QMatrix4x4());
    QVERIFY(!server_drags.get_source().surfaces.icon);
    QCOMPARE(server_drags.get_source().serial, button_press_spy.first().first().value<quint32>());
    QVERIFY(drag_entered_spy.wait());
    QCOMPARE(drag_entered_spy.count(), 1);
    QCOMPARE(drag_entered_spy.first().first().value<quint32>(), server.display->serial());
    QCOMPARE(drag_entered_spy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(c_1.device->dragSurface().data(), surface.get());

    auto offer = c_1.device->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offer_action_changed_spy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offer_action_changed_spy.isValid());
    QCOMPARE(c_1.device->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(c_1.device->dragOffer()->offeredMimeTypes().first().name(),
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
    QCOMPARE(c_1.source->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // Simulate motion.
    server.seat->setTimestamp(3);
    server.seat->pointers().set_position(QPointF(3, 3));
    QVERIFY(drag_motion_spy.wait());
    QCOMPARE(drag_motion_spy.count(), 1);
    QCOMPARE(drag_motion_spy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(drag_motion_spy.first().last().toUInt(), 3u);

    // Simulate drop.
    QSignalSpy server_drag_ended_spy(server.seat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(server_drag_ended_spy.isValid());
    QSignalSpy dropped_spy(c_1.device, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(dropped_spy.isValid());
    server.seat->setTimestamp(4);
    server.seat->pointers().button_released(1);
    QVERIFY(source_drop_spy.isEmpty());
    QVERIFY(dropped_spy.wait());
    QCOMPARE(source_drop_spy.count(), 1);
    QCOMPARE(server_drag_ended_spy.count(), 1);

    QSignalSpy finished_spy(c_1.source, &Wrapland::Client::DataSource::dragAndDropFinished);
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
    std::unique_ptr<Wrapland::Client::Surface> s(create_surface(c_1));
    s->setSize(QSize(100, 100));
    auto server_surface = get_server_surface();
    QVERIFY(server_surface);

    QSignalSpy source_selected_action_changed_spy(
        c_1.source, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(source_selected_action_changed_spy.isValid());

    // now we need to pass touch focus to the Surface and simulate a touch down
    QSignalSpy sequence_started_spy(c_1.touch, &Wrapland::Client::Touch::sequenceStarted);
    QVERIFY(sequence_started_spy.isValid());
    QSignalSpy point_added_spy(c_1.touch, &Wrapland::Client::Touch::pointAdded);
    QVERIFY(point_added_spy.isValid());

    auto& server_touches = server.seat->touches();
    server_touches.set_focused_surface(server_surface);
    server.seat->setTimestamp(2);
    auto const touchId = server_touches.touch_down(QPointF(50, 50));
    QVERIFY(sequence_started_spy.wait());

    auto tp{sequence_started_spy.first().at(0).value<Wrapland::Client::TouchPoint*>()};
    QVERIFY(tp);
    QCOMPARE(tp->time(), quint32(2));

    // add some signal spies for client side
    QSignalSpy drag_entered_spy(c_1.device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());
    QSignalSpy drag_motion_spy(c_1.device, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(drag_motion_spy.isValid());
    QSignalSpy touch_motion_spy(c_1.touch, &Wrapland::Client::Touch::pointMoved);
    QVERIFY(touch_motion_spy.isValid());
    QSignalSpy source_drop_spy(c_1.source, &Wrapland::Client::DataSource::dragAndDropPerformed);
    QVERIFY(source_drop_spy.isValid());

    // now we can start the drag and drop
    QSignalSpy drag_started_spy(server.seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());
    c_1.source->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                      | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    c_1.device->startDrag(tp->downSerial(), c_1.source, s.get());
    QVERIFY(drag_started_spy.wait());

    auto& server_drags = server.seat->drags();
    QCOMPARE(server_drags.get_target().surface, server_surface);
    QCOMPARE(server_drags.get_target().transformation, QMatrix4x4());
    QVERIFY(!server_drags.get_source().surfaces.icon);
    QCOMPARE(server_drags.get_source().serial, tp->downSerial());
    QVERIFY(drag_entered_spy.wait());
    QCOMPARE(drag_entered_spy.count(), 1);
    QCOMPARE(drag_entered_spy.first().first().value<quint32>(), server.display->serial());
    QCOMPARE(drag_entered_spy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(c_1.device->dragSurface().data(), s.get());
    auto offer = c_1.device->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offer_action_changed_spy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offer_action_changed_spy.isValid());
    QCOMPARE(c_1.device->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(c_1.device->dragOffer()->offeredMimeTypes().first().name(),
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
    QCOMPARE(c_1.source->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // simulate motion
    server.seat->setTimestamp(3);
    server_touches.touch_move(touchId, QPointF(75, 75));
    QVERIFY(drag_motion_spy.wait());
    QCOMPARE(drag_motion_spy.count(), 1);
    QCOMPARE(drag_motion_spy.first().first().toPointF(), QPointF(75, 75));
    QCOMPARE(drag_motion_spy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy server_drag_ended_spy(server.seat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(server_drag_ended_spy.isValid());
    QSignalSpy dropped_spy(c_1.device, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(dropped_spy.isValid());
    server.seat->setTimestamp(4);
    server_touches.touch_up(touchId);
    QVERIFY(source_drop_spy.isEmpty());
    QVERIFY(dropped_spy.wait());
    QCOMPARE(source_drop_spy.count(), 1);
    QCOMPARE(server_drag_ended_spy.count(), 1);

    QSignalSpy finished_spy(c_1.source, &Wrapland::Client::DataSource::dragAndDropFinished);
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
    std::unique_ptr<Wrapland::Client::Surface> s(create_surface(c_1));
    auto server_surface = get_server_surface();
    QVERIFY(server_surface);

    QSignalSpy source_selected_action_changed_spy(
        c_1.source, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(source_selected_action_changed_spy.isValid());

    // Now we need to pass pointer focus to the Surface and simulate a button press.
    QSignalSpy button_press_spy(c_1.pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(button_press_spy.isValid());
    server.seat->pointers().set_focused_surface(server_surface);
    server.seat->setTimestamp(2);
    server.seat->pointers().button_pressed(1);
    QVERIFY(button_press_spy.wait());
    QCOMPARE(button_press_spy.first().at(1).value<quint32>(), quint32(2));

    // Add some signal spies for client side.
    QSignalSpy drag_entered_spy(c_1.device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());
    QSignalSpy drag_motion_spy(c_1.device, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(drag_motion_spy.isValid());
    QSignalSpy pointer_motion_spy(c_1.pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointer_motion_spy.isValid());
    QSignalSpy drag_left_spy(c_1.device, &Wrapland::Client::DataDevice::dragLeft);
    QVERIFY(drag_left_spy.isValid());

    // Now we can start the drag and drop.
    QSignalSpy drag_started_spy(server.seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());

    c_1.source->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                      | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    c_1.device->startDrag(button_press_spy.first().first().value<quint32>(), c_1.source, s.get());

    QVERIFY(drag_started_spy.wait());

    auto& server_drags = server.seat->drags();
    QCOMPARE(server_drags.get_target().surface, server_surface);
    QCOMPARE(server_drags.get_target().transformation, QMatrix4x4());
    QVERIFY(!server_drags.get_source().surfaces.icon);
    QCOMPARE(server_drags.get_source().serial, button_press_spy.first().first().value<quint32>());

    QVERIFY(drag_entered_spy.wait());
    QCOMPARE(drag_entered_spy.count(), 1);
    QCOMPARE(drag_entered_spy.first().first().value<quint32>(), server.display->serial());
    QCOMPARE(drag_entered_spy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(c_1.device->dragSurface().data(), s.get());

    auto offer = c_1.device->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offer_action_changed_spy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offer_action_changed_spy.isValid());
    QCOMPARE(c_1.device->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(c_1.device->dragOffer()->offeredMimeTypes().first().name(),
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
    QCOMPARE(c_1.source->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // Simulate motion.
    server.seat->setTimestamp(3);
    server.seat->pointers().set_position(QPointF(3, 3));
    QVERIFY(drag_motion_spy.wait());
    QCOMPARE(drag_motion_spy.count(), 1);
    QCOMPARE(drag_motion_spy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(drag_motion_spy.first().last().toUInt(), 3u);

    // Now delete the DataSource.
    delete c_1.source;
    c_1.source = nullptr;
    QSignalSpy server_drag_ended_spy(server.seat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(server_drag_ended_spy.isValid());
    QVERIFY(drag_left_spy.isEmpty());
    QVERIFY(drag_left_spy.wait());
    QTRY_COMPARE(drag_left_spy.count(), 1);
    QTRY_COMPARE(server_drag_ended_spy.count(), 1);

    // Simulate drop.
    QSignalSpy dropped_spy(c_1.device, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(dropped_spy.isValid());
    server.seat->setTimestamp(4);
    server.seat->pointers().button_released(1);
    QVERIFY(!dropped_spy.wait(500));

    // Verify that we did not get any further input events.
    QVERIFY(pointer_motion_spy.isEmpty());
    QCOMPARE(button_press_spy.count(), 2);
}

void TestDragAndDrop::test_target_removed()
{
    // Checks that if target goes away mid-drag server handles this correctly.

    std::unique_ptr<Wrapland::Client::Surface> surface_1(create_surface(c_1));
    auto server_surface_1 = get_server_surface();
    QVERIFY(server_surface_1);
    std::unique_ptr<Wrapland::Client::Surface> surface_2(create_surface(c_2));
    auto server_surface_2 = get_server_surface();
    QVERIFY(server_surface_2);

    QSignalSpy source_selected_action_changed_spy(
        c_1.source, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(source_selected_action_changed_spy.isValid());

    // Now we need to pass pointer focus to the Surface and simulate a button press.
    QSignalSpy button_press_spy(c_1.pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(button_press_spy.isValid());
    server.seat->pointers().set_focused_surface(server_surface_1);
    server.seat->setTimestamp(2);
    server.seat->pointers().button_pressed(1);
    QVERIFY(button_press_spy.wait());
    QCOMPARE(button_press_spy.first().at(1).value<quint32>(), quint32(2));

    // Now we can start the drag and drop.
    QSignalSpy drag_started_spy(server.seat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(drag_started_spy.isValid());

    c_1.source->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                      | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    c_1.device->startDrag(
        button_press_spy.first().first().value<quint32>(), c_1.source, surface_1.get());

    QVERIFY(drag_started_spy.wait());

    auto& server_drags = server.seat->drags();
    QCOMPARE(server_drags.get_target().surface, server_surface_1);
    QCOMPARE(server_drags.get_target().transformation, QMatrix4x4());
    QVERIFY(!server_drags.get_source().surfaces.icon);
    QCOMPARE(server_drags.get_source().serial, button_press_spy.first().first().value<quint32>());

    QSignalSpy drag_entered_spy(c_2.device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());

    // Now move drag to the second client.
    server_drags.set_target(server_surface_2);
    QCOMPARE(server_drags.get_target().surface, server_surface_2);

    QVERIFY(drag_entered_spy.wait());
    QCOMPARE(drag_entered_spy.count(), 1);
    QCOMPARE(drag_entered_spy.first().first().value<quint32>(), server.display->serial());
    QCOMPARE(drag_entered_spy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(c_2.device->dragSurface().data(), surface_2.get());

    auto offer = c_2.device->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offer_action_changed_spy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offer_action_changed_spy.isValid());
    QCOMPARE(c_2.device->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(c_2.device->dragOffer()->offeredMimeTypes().first().name(),
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
    QVERIFY(source_selected_action_changed_spy.count() == 1
            || source_selected_action_changed_spy.wait());
    QCOMPARE(source_selected_action_changed_spy.count(), 1);
    QCOMPARE(c_1.source->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // Now delete the second client's data device.
    QSignalSpy device_destroyed_spy(c_2.server_device,
                                    &Wrapland::Server::data_device::resourceDestroyed);
    QVERIFY(device_destroyed_spy.isValid());
    delete c_2.device;
    c_2.device = nullptr;
    QVERIFY(device_destroyed_spy.wait());

    // Simulate drop.
    QSignalSpy dropped_spy(c_1.source, &Wrapland::Client::DataSource::dragAndDropPerformed);
    QVERIFY(dropped_spy.isValid());
    server.seat->setTimestamp(4);
    server.seat->pointers().button_released(1);
    QVERIFY(dropped_spy.wait(500));
}

void TestDragAndDrop::test_pointer_events_ignored()
{
    // This test verifies that all pointer events are ignored on the focused Pointer device during
    // drag.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> s(create_surface(c_1));
    auto server_surface = get_server_surface();
    QVERIFY(server_surface);

    // pass it pointer focus
    server.seat->pointers().set_focused_surface(server_surface);

    // create signal spies for all the pointer events
    QSignalSpy pointer_entered_spy(c_1.pointer, &Wrapland::Client::Pointer::entered);
    QVERIFY(pointer_entered_spy.isValid());
    QSignalSpy pointer_left_spy(c_1.pointer, &Wrapland::Client::Pointer::left);
    QVERIFY(pointer_left_spy.isValid());
    QSignalSpy pointer_motion_spy(c_1.pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointer_motion_spy.isValid());
    QSignalSpy axis_spy(c_1.pointer, &Wrapland::Client::Pointer::axisChanged);
    QVERIFY(axis_spy.isValid());
    QSignalSpy button_spy(c_1.pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(button_spy.isValid());
    QSignalSpy drag_entered_spy(c_1.device, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(drag_entered_spy.isValid());

    // first simulate a few things
    quint32 timestamp = 1;
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().set_position(QPointF(10, 10));
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().send_axis(Qt::Vertical, 5);
    // verify that we have those
    QVERIFY(axis_spy.wait());
    QCOMPARE(axis_spy.count(), 1);
    QCOMPARE(pointer_motion_spy.count(), 1);
    QCOMPARE(pointer_entered_spy.count(), 1);
    QVERIFY(button_spy.isEmpty());
    QVERIFY(pointer_left_spy.isEmpty());

    // let's start the drag
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().button_pressed(1);
    QVERIFY(button_spy.wait());
    QCOMPARE(button_spy.count(), 1);
    c_1.device->startDrag(button_spy.first().first().value<quint32>(), c_1.source, s.get());
    QVERIFY(drag_entered_spy.wait());

    // now simulate all the possible pointer interactions
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().button_pressed(2);
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().button_released(2);
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().send_axis(Qt::Vertical, 5);
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().send_axis(Qt::Horizontal, 5);
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().set_focused_surface(nullptr);
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().set_focused_surface(server_surface);
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().set_position(QPointF(50, 50));

    // last but not least, simulate the drop
    QSignalSpy dropped_spy(c_1.device, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(dropped_spy.isValid());
    server.seat->setTimestamp(timestamp++);
    server.seat->pointers().button_released(1);
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
