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
#include "../../src/client/shell.h"
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

#include "../../src/server/shell_interface.h"

class TestDragAndDrop : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void testPointerDragAndDrop();
    void testTouchDragAndDrop();
    void testDragAndDropWithCancelByDestroyDataSource();
    void testPointerEventsIgnored();

private:
    Wrapland::Client::Surface* createSurface();
    Wrapland::Server::Surface* getServerSurface();

    Wrapland::Server::D_isplay* m_display = nullptr;
    Wrapland::Server::Compositor* m_serverCompositor = nullptr;
    Wrapland::Server::DataDeviceManager* m_serverDataDeviceManager = nullptr;
    Wrapland::Server::Seat* m_serverSeat = nullptr;
    Wrapland::Server::ShellInterface* m_shellInterface = nullptr;
    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    Wrapland::Client::DataDevice* m_dataDevice = nullptr;
    Wrapland::Client::DataSource* m_dataSource = nullptr;
    QThread* m_thread = nullptr;
    Wrapland::Client::Registry* m_registry = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::Pointer* m_pointer = nullptr;
    Wrapland::Client::Touch* m_touch = nullptr;
    Wrapland::Client::DataDeviceManager* m_ddm = nullptr;
    Wrapland::Client::ShmPool* m_shm = nullptr;
    Wrapland::Client::Shell* m_shell = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-drag-n-drop-0");

void TestDragAndDrop::init()
{
    delete m_display;
    m_display = new Wrapland::Server::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_serverCompositor = m_display->createCompositor(m_display);
    m_serverSeat = m_display->createSeat(m_display);
    m_serverSeat->setHasPointer(true);
    m_serverSeat->setHasTouch(true);

    m_serverDataDeviceManager = m_display->createDataDeviceManager(m_display);
    m_display->createShm();
    m_shellInterface = m_display->createShell(m_display);
    m_shellInterface->create();

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

    m_registry = new Wrapland::Client::Registry();
    QSignalSpy interfacesAnnouncedSpy(m_registry, &Wrapland::Client::Registry::interfaceAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    QVERIFY(interfacesAnnouncedSpy.wait());
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
    CREATE(m_shell, Shell, Shell)

#undef CREATE

    QSignalSpy pointerSpy(m_seat, &Wrapland::Client::Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    QVERIFY(pointerSpy.wait());
    m_pointer = m_seat->createPointer(m_seat);
    QVERIFY(m_pointer->isValid());
    m_touch = m_seat->createTouch(m_seat);
    QVERIFY(m_touch->isValid());
    m_dataDevice = m_ddm->getDataDevice(m_seat, this);
    QVERIFY(m_dataDevice->isValid());
    m_dataSource = m_ddm->createDataSource(this);
    QVERIFY(m_dataSource->isValid());
    m_dataSource->offer(QStringLiteral("text/plain"));
}

void TestDragAndDrop::cleanup()
{
#define DELETE(name)                                                                               \
    if (name) {                                                                                    \
        delete name;                                                                               \
        name = nullptr;                                                                            \
    }
    DELETE(m_dataSource)
    DELETE(m_dataDevice)
    DELETE(m_shell)
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

Wrapland::Client::Surface* TestDragAndDrop::createSurface()
{
    auto s = m_compositor->createSurface();

    QImage img(QSize(100, 200), QImage::Format_RGB32);
    img.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(img));
    s->damage(QRect(0, 0, 100, 200));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    return s;
}

Wrapland::Server::Surface* TestDragAndDrop::getServerSurface()
{
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    if (!surfaceCreatedSpy.isValid()) {
        return nullptr;
    }
    if (!surfaceCreatedSpy.wait(500)) {
        return nullptr;
    }
    return surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
}

void TestDragAndDrop::testPointerDragAndDrop()
{
    // This test verifies the very basic drag and drop on one surface, an enter, a move and the
    // drop.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> surface(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(
        m_dataSource, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(dataSourceSelectedActionChangedSpy.isValid());

    // now we need to pass pointer focus to the Surface and simulate a button press
    QSignalSpy buttonPressSpy(m_pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(buttonPressSpy.isValid());
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    m_serverSeat->setTimestamp(2);
    m_serverSeat->pointerButtonPressed(1);
    QVERIFY(buttonPressSpy.wait());
    QCOMPARE(buttonPressSpy.first().at(1).value<quint32>(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());
    QSignalSpy dragMotionSpy(m_dataDevice, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(dragMotionSpy.isValid());
    QSignalSpy pointerMotionSpy(m_pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());
    QSignalSpy sourceDropSpy(m_dataSource, &Wrapland::Client::DataSource::dragAndDropPerformed);
    QVERIFY(sourceDropSpy.isValid());

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_serverSeat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(dragStartedSpy.isValid());
    m_dataSource->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                        | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(
        buttonPressSpy.first().first().value<quint32>(), m_dataSource, surface.get());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_serverSeat->dragSurface(), serverSurface);
    QCOMPARE(m_serverSeat->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_serverSeat->dragSource()->icon());
    QCOMPARE(m_serverSeat->dragSource()->dragImplicitGrabSerial(),
             buttonPressSpy.first().first().value<quint32>());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), surface.get());

    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offerActionChangedSpy.isValid());
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(),
             QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(),
                 Wrapland::Client::DataDeviceManager::DnDAction::Copy
                     | Wrapland::Client::DataDeviceManager::DnDAction::Move);

    offer->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                     | Wrapland::Client::DataDeviceManager::DnDAction::Move,
                                 Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // Simulate motion.
    m_serverSeat->setTimestamp(3);
    m_serverSeat->setPointerPos(QPointF(3, 3));
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // Simulate drop.
    QSignalSpy serverDragEndedSpy(m_serverSeat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(serverDragEndedSpy.isValid());
    QSignalSpy droppedSpy(m_dataDevice, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(droppedSpy.isValid());
    m_serverSeat->setTimestamp(4);
    m_serverSeat->pointerButtonReleased(1);
    QVERIFY(sourceDropSpy.isEmpty());
    QVERIFY(droppedSpy.wait());
    QCOMPARE(sourceDropSpy.count(), 1);
    QCOMPARE(serverDragEndedSpy.count(), 1);

    QSignalSpy finishedSpy(m_dataSource, &Wrapland::Client::DataSource::dragAndDropFinished);
    QVERIFY(finishedSpy.isValid());
    offer->dragAndDropFinished();
    QVERIFY(finishedSpy.wait());
    delete offer;

    // Verify that we did not get any further input events.
    QVERIFY(pointerMotionSpy.isEmpty());
    QCOMPARE(buttonPressSpy.count(), 1);
}

void TestDragAndDrop::testTouchDragAndDrop()
{
    // This test verifies the very basic drag and drop on one surface, an enter, a move and the
    // drop.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> s(createSurface());
    s->setSize(QSize(100, 100));
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(
        m_dataSource, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(dataSourceSelectedActionChangedSpy.isValid());

    // now we need to pass touch focus to the Surface and simulate a touch down
    QSignalSpy sequenceStartedSpy(m_touch, &Wrapland::Client::Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy pointAddedSpy(m_touch, &Wrapland::Client::Touch::pointAdded);
    QVERIFY(pointAddedSpy.isValid());
    m_serverSeat->setFocusedTouchSurface(serverSurface);
    m_serverSeat->setTimestamp(2);
    const qint32 touchId = m_serverSeat->touchDown(QPointF(50, 50));
    QVERIFY(sequenceStartedSpy.wait());

    auto tp{sequenceStartedSpy.first().at(0).value<Wrapland::Client::TouchPoint*>()};
    QVERIFY(tp);
    QCOMPARE(tp->time(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());
    QSignalSpy dragMotionSpy(m_dataDevice, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(dragMotionSpy.isValid());
    QSignalSpy touchMotionSpy(m_touch, &Wrapland::Client::Touch::pointMoved);
    QVERIFY(touchMotionSpy.isValid());
    QSignalSpy sourceDropSpy(m_dataSource, &Wrapland::Client::DataSource::dragAndDropPerformed);
    QVERIFY(sourceDropSpy.isValid());

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_serverSeat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(dragStartedSpy.isValid());
    m_dataSource->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                        | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(tp->downSerial(), m_dataSource, s.get());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_serverSeat->dragSurface(), serverSurface);
    QCOMPARE(m_serverSeat->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_serverSeat->dragSource()->icon());
    QCOMPARE(m_serverSeat->dragSource()->dragImplicitGrabSerial(), tp->downSerial());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), s.get());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offerActionChangedSpy.isValid());
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(),
             QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(),
                 Wrapland::Client::DataDeviceManager::DnDAction::Copy
                     | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    offer->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                     | Wrapland::Client::DataDeviceManager::DnDAction::Move,
                                 Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_serverSeat->setTimestamp(3);
    m_serverSeat->touchMove(touchId, QPointF(75, 75));
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(75, 75));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy serverDragEndedSpy(m_serverSeat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(serverDragEndedSpy.isValid());
    QSignalSpy droppedSpy(m_dataDevice, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(droppedSpy.isValid());
    m_serverSeat->setTimestamp(4);
    m_serverSeat->touchUp(touchId);
    QVERIFY(sourceDropSpy.isEmpty());
    QVERIFY(droppedSpy.wait());
    QCOMPARE(sourceDropSpy.count(), 1);
    QCOMPARE(serverDragEndedSpy.count(), 1);

    QSignalSpy finishedSpy(m_dataSource, &Wrapland::Client::DataSource::dragAndDropFinished);
    QVERIFY(finishedSpy.isValid());
    offer->dragAndDropFinished();
    QVERIFY(finishedSpy.wait());
    delete offer;

    // verify that we did not get any further input events
    QVERIFY(touchMotionSpy.isEmpty());
    QCOMPARE(pointAddedSpy.count(), 0);
}

void TestDragAndDrop::testDragAndDropWithCancelByDestroyDataSource()
{
    // This test simulates the problem from BUG 389221.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> s(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(
        m_dataSource, &Wrapland::Client::DataSource::selectedDragAndDropActionChanged);
    QVERIFY(dataSourceSelectedActionChangedSpy.isValid());

    // Now we need to pass pointer focus to the Surface and simulate a button press.
    QSignalSpy buttonPressSpy(m_pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(buttonPressSpy.isValid());
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    m_serverSeat->setTimestamp(2);
    m_serverSeat->pointerButtonPressed(1);
    QVERIFY(buttonPressSpy.wait());
    QCOMPARE(buttonPressSpy.first().at(1).value<quint32>(), quint32(2));

    // Add some signal spies for client side.
    QSignalSpy dragEnteredSpy(m_dataDevice, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());
    QSignalSpy dragMotionSpy(m_dataDevice, &Wrapland::Client::DataDevice::dragMotion);
    QVERIFY(dragMotionSpy.isValid());
    QSignalSpy pointerMotionSpy(m_pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());
    QSignalSpy dragLeftSpy(m_dataDevice, &Wrapland::Client::DataDevice::dragLeft);
    QVERIFY(dragLeftSpy.isValid());

    // Now we can start the drag and drop.
    QSignalSpy dragStartedSpy(m_serverSeat, &Wrapland::Server::Seat::dragStarted);
    QVERIFY(dragStartedSpy.isValid());

    m_dataSource->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                        | Wrapland::Client::DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(buttonPressSpy.first().first().value<quint32>(), m_dataSource, s.get());

    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_serverSeat->dragSurface(), serverSurface);
    QCOMPARE(m_serverSeat->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_serverSeat->dragSource()->icon());
    QCOMPARE(m_serverSeat->dragSource()->dragImplicitGrabSerial(),
             buttonPressSpy.first().first().value<quint32>());

    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), s.get());

    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(
        offer, &Wrapland::Client::DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offerActionChangedSpy.isValid());
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(),
             QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(),
                 Wrapland::Client::DataDeviceManager::DnDAction::Copy
                     | Wrapland::Client::DataDeviceManager::DnDAction::Move);

    offer->setDragAndDropActions(Wrapland::Client::DataDeviceManager::DnDAction::Copy
                                     | Wrapland::Client::DataDeviceManager::DnDAction::Move,
                                 Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(),
             Wrapland::Client::DataDeviceManager::DnDAction::Move);

    // Simulate motion.
    m_serverSeat->setTimestamp(3);
    m_serverSeat->setPointerPos(QPointF(3, 3));
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // Now delete the DataSource.
    delete m_dataSource;
    m_dataSource = nullptr;
    QSignalSpy serverDragEndedSpy(m_serverSeat, &Wrapland::Server::Seat::dragEnded);
    QVERIFY(serverDragEndedSpy.isValid());
    QVERIFY(dragLeftSpy.isEmpty());
    QVERIFY(dragLeftSpy.wait());
    QTRY_COMPARE(dragLeftSpy.count(), 1);
    QTRY_COMPARE(serverDragEndedSpy.count(), 1);

    // Simulate drop.
    QSignalSpy droppedSpy(m_dataDevice, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(droppedSpy.isValid());
    m_serverSeat->setTimestamp(4);
    m_serverSeat->pointerButtonReleased(1);
    QVERIFY(!droppedSpy.wait(500));

    // Verify that we did not get any further input events.
    QVERIFY(pointerMotionSpy.isEmpty());
    QCOMPARE(buttonPressSpy.count(), 2);
}

void TestDragAndDrop::testPointerEventsIgnored()
{
    // This test verifies that all pointer events are ignored on the focused Pointer device during
    // drag.

    // First create a window.
    std::unique_ptr<Wrapland::Client::Surface> s(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    // pass it pointer focus
    m_serverSeat->setFocusedPointerSurface(serverSurface);

    // create signal spies for all the pointer events
    QSignalSpy pointerEnteredSpy(m_pointer, &Wrapland::Client::Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerLeftSpy(m_pointer, &Wrapland::Client::Pointer::left);
    QVERIFY(pointerLeftSpy.isValid());
    QSignalSpy pointerMotionSpy(m_pointer, &Wrapland::Client::Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());
    QSignalSpy axisSpy(m_pointer, &Wrapland::Client::Pointer::axisChanged);
    QVERIFY(axisSpy.isValid());
    QSignalSpy buttonSpy(m_pointer, &Wrapland::Client::Pointer::buttonStateChanged);
    QVERIFY(buttonSpy.isValid());
    QSignalSpy dragEnteredSpy(m_dataDevice, &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());

    // first simulate a few things
    quint32 timestamp = 1;
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setPointerPos(QPointF(10, 10));
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerAxis(Qt::Vertical, 5);
    // verify that we have those
    QVERIFY(axisSpy.wait());
    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 1);
    QVERIFY(buttonSpy.isEmpty());
    QVERIFY(pointerLeftSpy.isEmpty());

    // let's start the drag
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerButtonPressed(1);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.count(), 1);
    m_dataDevice->startDrag(buttonSpy.first().first().value<quint32>(), m_dataSource, s.get());
    QVERIFY(dragEnteredSpy.wait());

    // now simulate all the possible pointer interactions
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerButtonPressed(2);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerButtonReleased(2);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerAxis(Qt::Vertical, 5);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerAxis(Qt::Horizontal, 5);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setFocusedPointerSurface(nullptr);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setFocusedPointerSurface(serverSurface);
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->setPointerPos(QPointF(50, 50));

    // last but not least, simulate the drop
    QSignalSpy droppedSpy(m_dataDevice, &Wrapland::Client::DataDevice::dropped);
    QVERIFY(droppedSpy.isValid());
    m_serverSeat->setTimestamp(timestamp++);
    m_serverSeat->pointerButtonReleased(1);
    QVERIFY(droppedSpy.wait());

    // all the changes should have been ignored
    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 1);
    QCOMPARE(buttonSpy.count(), 1);
    QVERIFY(pointerLeftSpy.isEmpty());
}

QTEST_GUILESS_MAIN(TestDragAndDrop)
#include "drag_and_drop.moc"
