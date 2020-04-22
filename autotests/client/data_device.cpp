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
#include "../../server/seat.h"
#include "../../server/surface.h"

#include <wayland-client.h>

#include <QtTest>

class TestDataDevice : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testDrag_data();
    void testDrag();
    void testDragInternally_data();
    void testDragInternally();
    void testSetSelection();
    void testSendSelectionOnSeat();
    void testReplaceSource();
    void testDestroy();

private:
    Wrapland::Server::D_isplay* m_display = nullptr;
    Wrapland::Server::DataDeviceManager* m_serverDataDeviceManager = nullptr;
    Wrapland::Server::Compositor* m_serverCompositor = nullptr;
    Wrapland::Server::Seat* m_serverSeat = nullptr;
    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::DataDeviceManager* m_dataDeviceManager = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    QThread* m_thread = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-datadevice-0");

void TestDataDevice::init()
{
    qRegisterMetaType<Wrapland::Server::DataDevice*>();
    qRegisterMetaType<Wrapland::Server::DataSource*>();
    qRegisterMetaType<Wrapland::Server::Surface*>();

    m_display = new Wrapland::Server::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy establishedSpy(m_connection,
                              &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(establishedSpy.wait());

    m_queue = new Wrapland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Wrapland::Client::Registry registry;
    QSignalSpy dataDeviceManagerSpy(&registry,
                                    SIGNAL(dataDeviceManagerAnnounced(quint32, quint32)));
    QVERIFY(dataDeviceManagerSpy.isValid());
    QSignalSpy seatSpy(&registry, SIGNAL(seatAnnounced(quint32, quint32)));
    QVERIFY(seatSpy.isValid());
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32, quint32)));
    QVERIFY(compositorSpy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_serverDataDeviceManager = m_display->createDataDeviceManager(m_display);

    QVERIFY(dataDeviceManagerSpy.wait());
    m_dataDeviceManager
        = registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(),
                                           dataDeviceManagerSpy.first().last().value<quint32>(),
                                           this);

    m_serverSeat = m_display->createSeat(m_display);
    m_serverSeat->setHasPointer(true);

    QVERIFY(seatSpy.wait());
    m_seat = registry.createSeat(
        seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QVERIFY(m_seat->isValid());
    QSignalSpy pointerChangedSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerChangedSpy.isValid());
    QVERIFY(pointerChangedSpy.wait());

    m_serverCompositor = m_display->createCompositor(m_display);

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);
    QVERIFY(m_compositor->isValid());
}

void TestDataDevice::cleanup()
{
    if (m_dataDeviceManager) {
        delete m_dataDeviceManager;
        m_dataDeviceManager = nullptr;
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

    delete m_display;
    m_display = nullptr;
}

void TestDataDevice::testCreate()
{
    QSignalSpy dataDeviceCreatedSpy(m_serverDataDeviceManager,
                                    SIGNAL(dataDeviceCreated(Wrapland::Server::DataDevice*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> dataDevice(
        m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);

    auto serverDevice = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::DataDevice*>();
    QVERIFY(serverDevice);
    QCOMPARE(serverDevice->seat(), m_serverSeat);
    QVERIFY(!serverDevice->dragSource());
    QVERIFY(!serverDevice->origin());
    QVERIFY(!serverDevice->icon());
    QVERIFY(!serverDevice->selection());

    QVERIFY(!m_serverSeat->selection());
    m_serverSeat->setSelection(serverDevice);
    QCOMPARE(m_serverSeat->selection(), serverDevice);

    // and destroy
    QSignalSpy destroyedSpy(serverDevice, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    dataDevice.reset();
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_serverSeat->selection());
}

void TestDataDevice::testDrag_data()
{
    QTest::addColumn<bool>("hasGrab");
    QTest::addColumn<bool>("hasPointerFocus");
    QTest::addColumn<bool>("success");

    QTest::newRow("grab and focus") << true << true << true;
    QTest::newRow("no grab") << false << true << false;
    QTest::newRow("no focus") << true << false << false;
    QTest::newRow("no grab, no focus") << false << false << false;
}

void TestDataDevice::testDrag()
{
    std::unique_ptr<Wrapland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_serverDataDeviceManager,
                                    SIGNAL(dataDeviceCreated(Wrapland::Server::DataDevice*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> dataDevice(
        m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto serverDevice = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::DataDevice*>();
    QVERIFY(serverDevice);

    QSignalSpy dataSourceCreatedSpy(m_serverDataDeviceManager,
                                    &Wrapland::Server::DataDeviceManager::dataSourceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto sourceInterface
        = dataSourceCreatedSpy.first().first().value<Wrapland::Server::DataSource*>();
    QVERIFY(sourceInterface);

    QSignalSpy surfaceCreatedSpy(m_serverCompositor,
                                 SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 1);
    auto surfaceInterface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();

    // now we have all we need to start a drag operation
    QSignalSpy dragStartedSpy(serverDevice, SIGNAL(dragStarted()));
    QVERIFY(dragStartedSpy.isValid());
    QSignalSpy dragEnteredSpy(dataDevice.get(), &Wrapland::Client::DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());

    // first we need to fake the pointer enter
    QFETCH(bool, hasGrab);
    QFETCH(bool, hasPointerFocus);
    QFETCH(bool, success);
    if (!hasGrab) {
        // in case we don't have grab, still generate a pointer serial to make it more interesting
        m_serverSeat->pointerButtonPressed(Qt::LeftButton);
    }
    if (hasPointerFocus) {
        m_serverSeat->setFocusedPointerSurface(surfaceInterface);
    }
    if (hasGrab) {
        m_serverSeat->pointerButtonPressed(Qt::LeftButton);
    }

    // TODO: This test would be better, if it could also test that a client trying to guess
    //       the last serial of a different client can't start a drag.
    const quint32 pointerButtonSerial
        = success ? m_serverSeat->pointerButtonSerial(Qt::LeftButton) : 0;

    QCoreApplication::processEvents();
    // finally start the drag
    dataDevice->startDrag(pointerButtonSerial, dataSource.get(), surface.get());
    QCOMPARE(dragStartedSpy.wait(500), success);
    QCOMPARE(!dragStartedSpy.isEmpty(), success);
    QCOMPARE(serverDevice->dragSource(), success ? sourceInterface : nullptr);
    QCOMPARE(serverDevice->origin(), success ? surfaceInterface : nullptr);
    QVERIFY(!serverDevice->icon());

    if (success) {
        // Wait for the drag-enter on itself, otherwise we leak the data offer.
        // There also seem to be no way to eliminate this issue. If the client closes the connection
        // at the moment a drag enters (and afterwards a data offer is sent) the memory is lost.
        // Must this be solved in libwayland?
        QVERIFY(dragEnteredSpy.count() || dragEnteredSpy.wait());
    }
}

void TestDataDevice::testDragInternally_data()
{
    QTest::addColumn<bool>("hasGrab");
    QTest::addColumn<bool>("hasPointerFocus");
    QTest::addColumn<bool>("success");

    QTest::newRow("grab and focus") << true << true << true;
    QTest::newRow("no grab") << false << true << false;
    QTest::newRow("no focus") << true << false << false;
    QTest::newRow("no grab, no focus") << false << false << false;
}

void TestDataDevice::testDragInternally()
{
    std::unique_ptr<Wrapland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_serverDataDeviceManager,
                                    SIGNAL(dataDeviceCreated(Wrapland::Server::DataDevice*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> dataDevice(
        m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto serverDevice = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::DataDevice*>();
    QVERIFY(serverDevice);

    QSignalSpy surfaceCreatedSpy(m_serverCompositor,
                                 SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 1);
    auto surfaceInterface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();

    std::unique_ptr<Wrapland::Client::Surface> iconSurface(m_compositor->createSurface());
    QVERIFY(iconSurface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 2);
    auto iconSurfaceInterface
        = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();

    // now we have all we need to start a drag operation
    QSignalSpy dragStartedSpy(serverDevice, &Wrapland::Server::DataDevice::dragStarted);
    QVERIFY(dragStartedSpy.isValid());

    // first we need to fake the pointer enter
    QFETCH(bool, hasGrab);
    QFETCH(bool, hasPointerFocus);
    QFETCH(bool, success);
    if (!hasGrab) {
        // in case we don't have grab, still generate a pointer serial to make it more interesting
        m_serverSeat->pointerButtonPressed(Qt::LeftButton);
    }
    if (hasPointerFocus) {
        m_serverSeat->setFocusedPointerSurface(surfaceInterface);
    }
    if (hasGrab) {
        m_serverSeat->pointerButtonPressed(Qt::LeftButton);
    }

    // TODO: This test would be better, if it could also test that a client trying to guess
    //       the last serial of a different client can't start a drag.
    const quint32 pointerButtonSerial
        = success ? m_serverSeat->pointerButtonSerial(Qt::LeftButton) : 0;

    QCoreApplication::processEvents();
    // finally start the internal drag
    dataDevice->startDragInternally(pointerButtonSerial, surface.get(), iconSurface.get());
    QCOMPARE(dragStartedSpy.wait(500), success);
    QCOMPARE(!dragStartedSpy.isEmpty(), success);
    QVERIFY(!serverDevice->dragSource());
    QCOMPARE(serverDevice->origin(), success ? surfaceInterface : nullptr);
    QCOMPARE(serverDevice->icon(), success ? iconSurfaceInterface : nullptr);
}

void TestDataDevice::testSetSelection()
{
    std::unique_ptr<Wrapland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_serverDataDeviceManager,
                                    SIGNAL(dataDeviceCreated(Wrapland::Server::DataDevice*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> dataDevice(
        m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto serverDevice = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::DataDevice*>();
    QVERIFY(serverDevice);

    QSignalSpy dataSourceCreatedSpy(m_serverDataDeviceManager,
                                    SIGNAL(dataSourceCreated(Wrapland::Server::DataSource*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto serverSource = dataSourceCreatedSpy.first().first().value<Wrapland::Server::DataSource*>();
    QVERIFY(serverSource);

    // everything setup, now we can test setting the selection
    QSignalSpy selectionChangedSpy(serverDevice,
                                   SIGNAL(selectionChanged(Wrapland::Server::DataSource*)));
    QVERIFY(selectionChangedSpy.isValid());
    QSignalSpy selectionClearedSpy(serverDevice, SIGNAL(selectionCleared()));
    QVERIFY(selectionClearedSpy.isValid());

    QVERIFY(!serverDevice->selection());
    dataDevice->setSelection(1, dataSource.get());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionClearedSpy.count(), 0);
    QCOMPARE(selectionChangedSpy.first().first().value<Wrapland::Server::DataSource*>(),
             serverSource);
    QCOMPARE(serverDevice->selection(), serverSource);

    // Send selection to datadevice.
    QSignalSpy selectionOfferedSpy(dataDevice.get(),
                                   &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    serverDevice->sendSelection(serverDevice);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    auto dataOffer = selectionOfferedSpy.first().first().value<Wrapland::Client::DataOffer*>();
    QVERIFY(dataOffer);
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 1);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // sending a new mimetype to the selection, should be announced in the offer
    QSignalSpy mimeTypeAddedSpy(dataOffer, SIGNAL(mimeTypeOffered(QString)));
    QVERIFY(mimeTypeAddedSpy.isValid());
    dataSource->offer("text/html");
    QVERIFY(mimeTypeAddedSpy.wait());
    QCOMPARE(mimeTypeAddedSpy.count(), 1);
    QCOMPARE(mimeTypeAddedSpy.first().first().toString(), QStringLiteral("text/html"));
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 2);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QCOMPARE(dataOffer->offeredMimeTypes().last().name(), QStringLiteral("text/html"));

    // now clear the selection
    dataDevice->clearSelection(1);
    QVERIFY(selectionClearedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionClearedSpy.count(), 1);
    QVERIFY(!serverDevice->selection());

    // set another selection
    dataDevice->setSelection(2, dataSource.get());
    QVERIFY(selectionChangedSpy.wait());
    // now unbind the dataDevice
    QSignalSpy unboundSpy(serverDevice, &Wrapland::Server::DataDevice::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());

    // TODO: This should be impossible to happen in the first place, correct?
#if 0
    // send a selection to the unbound data device
    serverDevice->sendSelection(serverDevice);
#endif
}

void TestDataDevice::testSendSelectionOnSeat()
{
    // This test verifies that the selection is sent when setting a focused keyboard.

    // First add keyboard support to Seat.
    QSignalSpy keyboardChangedSpy(m_seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_serverSeat->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());

    // Now create DataDevice, Keyboard and a Surface.
    QSignalSpy dataDeviceCreatedSpy(m_serverDataDeviceManager,
                                    &Wrapland::Server::DataDeviceManager::dataDeviceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::DataDevice> dataDevice(
        m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice
        = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::DataDevice*>();
    QVERIFY(serverDataDevice);
    std::unique_ptr<Wrapland::Client::Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);

    // Now set the selection.
    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.get());

    // We should get a selection offered for that on the data device.
    QSignalSpy selectionOfferedSpy(dataDevice.get(),
                                   &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // Now unfocus the keyboard.
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    // if setting the same surface again, we should get another offer
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 2);

    // Now let's try to destroy the data device and set a focused keyboard just while the data
    // device is being destroyed.
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    QSignalSpy unboundSpy(serverDataDevice, &Wrapland::Server::DataDevice::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
}

void TestDataDevice::testReplaceSource()
{
    // This test verifies that replacing a data source cancels the previous source.

    // First add keyboard support to Seat.
    QSignalSpy keyboardChangedSpy(m_seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_serverSeat->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create DataDevice, Keyboard and a Surface
    QSignalSpy dataDeviceCreatedSpy(m_serverDataDeviceManager,
                                    &Wrapland::Server::DataDeviceManager::dataDeviceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataDevice> dataDevice(
        m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice
        = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::DataDevice*>();
    QVERIFY(serverDataDevice);
    std::unique_ptr<Wrapland::Client::Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);

    // now set the selection
    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.get());
    QSignalSpy sourceCancelledSpy(dataSource.get(), &Wrapland::Client::DataSource::cancelled);
    QVERIFY(sourceCancelledSpy.isValid());
    // we should get a selection offered for that on the data device
    QSignalSpy selectionOfferedSpy(dataDevice.get(),
                                   &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // create a second data source and replace previous one
    std::unique_ptr<Wrapland::Client::DataSource> dataSource2(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource2->isValid());

    dataSource2->offer(QStringLiteral("text/plain"));

    QSignalSpy sourceCancelled2Spy(dataSource2.get(), &Wrapland::Client::DataSource::cancelled);
    QVERIFY(sourceCancelled2Spy.isValid());

    dataDevice->setSelection(1, dataSource2.get());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    QVERIFY(sourceCancelledSpy.wait());
    QTRY_COMPARE(selectionOfferedSpy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // replace the data source with itself, ensure that it did not get cancelled
    dataDevice->setSelection(1, dataSource2.get());
    QVERIFY(!sourceCancelled2Spy.wait(500));
    QCOMPARE(selectionOfferedSpy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // create a new DataDevice and replace previous one
    std::unique_ptr<Wrapland::Client::DataDevice> dataDevice2(
        m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice2->isValid());
    std::unique_ptr<Wrapland::Client::DataSource> dataSource3(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource3->isValid());
    dataSource3->offer(QStringLiteral("text/plain"));
    dataDevice2->setSelection(1, dataSource3.get());
    QVERIFY(sourceCancelled2Spy.wait());

    // try to crash by first destroying dataSource3 and setting a new DataSource
    std::unique_ptr<Wrapland::Client::DataSource> dataSource4(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource4->isValid());
    dataSource4->offer("text/plain");
    dataSource3.reset();
    dataDevice2->setSelection(1, dataSource4.get());
    QVERIFY(selectionOfferedSpy.wait());
}

void TestDataDevice::testDestroy()
{
    std::unique_ptr<Wrapland::Client::DataDevice> dataDevice(
        m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_dataDeviceManager,
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
            dataDevice.get(),
            &Wrapland::Client::DataDevice::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_queue,
            &Wrapland::Client::EventQueue::release);

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!m_connection->established());

    // Now the data device should be destroyed.
    QTRY_VERIFY(!dataDevice->isValid());

    // Calling destroy again should not fail.
    dataDevice->release();
}

QTEST_GUILESS_MAIN(TestDataDevice)
#include "data_device.moc"