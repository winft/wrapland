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
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/compositor.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/pointer.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"
#include "../../src/server/display.h"
#include "../../src/server/datadevicemanager_interface.h"
#include "../../src/server/datasource_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/pointer_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/surface_interface.h"

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
    Wrapland::Server::Display *m_display = nullptr;
    Wrapland::Server::DataDeviceManagerInterface *m_dataDeviceManagerInterface = nullptr;
    Wrapland::Server::CompositorInterface *m_compositorInterface = nullptr;
    Wrapland::Server::SeatInterface *m_seatInterface = nullptr;
    Wrapland::Client::ConnectionThread *m_connection = nullptr;
    Wrapland::Client::DataDeviceManager *m_dataDeviceManager = nullptr;
    Wrapland::Client::Compositor *m_compositor = nullptr;
    Wrapland::Client::Seat *m_seat = nullptr;
    Wrapland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-datadevice-0");

void TestDataDevice::init()
{
    qRegisterMetaType<Wrapland::Server::DataSourceInterface*>();
    using namespace Wrapland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy establishedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
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
    QSignalSpy dataDeviceManagerSpy(&registry, SIGNAL(dataDeviceManagerAnnounced(quint32,quint32)));
    QVERIFY(dataDeviceManagerSpy.isValid());
    QSignalSpy seatSpy(&registry, SIGNAL(seatAnnounced(quint32,quint32)));
    QVERIFY(seatSpy.isValid());
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QVERIFY(compositorSpy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_dataDeviceManagerInterface = m_display->createDataDeviceManager(m_display);
    m_dataDeviceManagerInterface->create();
    QVERIFY(m_dataDeviceManagerInterface->isValid());

    QVERIFY(dataDeviceManagerSpy.wait());
    m_dataDeviceManager = registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(),
                                                           dataDeviceManagerSpy.first().last().value<quint32>(), this);

    m_seatInterface = m_display->createSeat(m_display);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->create();
    QVERIFY(m_seatInterface->isValid());

    QVERIFY(seatSpy.wait());
    m_seat = registry.createSeat(seatSpy.first().first().value<quint32>(),
                                 seatSpy.first().last().value<quint32>(), this);
    QVERIFY(m_seat->isValid());
    QSignalSpy pointerChangedSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerChangedSpy.isValid());
    QVERIFY(pointerChangedSpy.wait());

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(), this);
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
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataDeviceCreated(Wrapland::Server::DataDeviceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(deviceInterface);
    QCOMPARE(deviceInterface->seat(), m_seatInterface);
    QVERIFY(!deviceInterface->dragSource());
    QVERIFY(!deviceInterface->origin());
    QVERIFY(!deviceInterface->icon());
    QVERIFY(!deviceInterface->selection());
    QVERIFY(deviceInterface->parentResource());

    QVERIFY(!m_seatInterface->selection());
    m_seatInterface->setSelection(deviceInterface);
    QCOMPARE(m_seatInterface->selection(), deviceInterface);

    // and destroy
    QSignalSpy destroyedSpy(deviceInterface, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    dataDevice.reset();
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_seatInterface->selection());
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
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    std::unique_ptr<Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataDeviceCreated(Wrapland::Server::DataDeviceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(deviceInterface);

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataSourceCreated(Wrapland::Server::DataSourceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto sourceInterface = dataSourceCreatedSpy.first().first().value<DataSourceInterface*>();
    QVERIFY(sourceInterface);

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(Wrapland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 1);
    auto surfaceInterface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();

    // now we have all we need to start a drag operation
    QSignalSpy dragStartedSpy(deviceInterface, SIGNAL(dragStarted()));
    QVERIFY(dragStartedSpy.isValid());
    QSignalSpy dragEnteredSpy(dataDevice.get(), &DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());

    // first we need to fake the pointer enter
    QFETCH(bool, hasGrab);
    QFETCH(bool, hasPointerFocus);
    QFETCH(bool, success);
    if (!hasGrab) {
        // in case we don't have grab, still generate a pointer serial to make it more interesting
        m_seatInterface->pointerButtonPressed(Qt::LeftButton);
    }
    if (hasPointerFocus) {
        m_seatInterface->setFocusedPointerSurface(surfaceInterface);
    }
    if (hasGrab) {
        m_seatInterface->pointerButtonPressed(Qt::LeftButton);
    }

    // TODO: This test would be better, if it could also test that a client trying to guess
    //       the last serial of a different client can't start a drag.
    const quint32 pointerButtonSerial = success ? m_seatInterface->pointerButtonSerial(Qt::LeftButton) : 0;

    QCoreApplication::processEvents();
    // finally start the drag
    dataDevice->startDrag(pointerButtonSerial, dataSource.get(), surface.get());
    QCOMPARE(dragStartedSpy.wait(500), success);
    QCOMPARE(!dragStartedSpy.isEmpty(), success);
    QCOMPARE(deviceInterface->dragSource(), success ? sourceInterface : nullptr);
    QCOMPARE(deviceInterface->origin(), success ? surfaceInterface : nullptr);
    QVERIFY(!deviceInterface->icon());

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
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    std::unique_ptr<Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataDeviceCreated(Wrapland::Server::DataDeviceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(deviceInterface);

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(Wrapland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 1);
    auto surfaceInterface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();

    std::unique_ptr<Surface> iconSurface(m_compositor->createSurface());
    QVERIFY(iconSurface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 2);
    auto iconSurfaceInterface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();

    // now we have all we need to start a drag operation
    QSignalSpy dragStartedSpy(deviceInterface, SIGNAL(dragStarted()));
    QVERIFY(dragStartedSpy.isValid());

    // first we need to fake the pointer enter
    QFETCH(bool, hasGrab);
    QFETCH(bool, hasPointerFocus);
    QFETCH(bool, success);
    if (!hasGrab) {
        // in case we don't have grab, still generate a pointer serial to make it more interesting
        m_seatInterface->pointerButtonPressed(Qt::LeftButton);
    }
    if (hasPointerFocus) {
        m_seatInterface->setFocusedPointerSurface(surfaceInterface);
    }
    if (hasGrab) {
        m_seatInterface->pointerButtonPressed(Qt::LeftButton);
    }

    // TODO: This test would be better, if it could also test that a client trying to guess
    //       the last serial of a different client can't start a drag.
    const quint32 pointerButtonSerial = success ? m_seatInterface->pointerButtonSerial(Qt::LeftButton) : 0;

    QCoreApplication::processEvents();
    // finally start the internal drag
    dataDevice->startDragInternally(pointerButtonSerial, surface.get(), iconSurface.get());
    QCOMPARE(dragStartedSpy.wait(500), success);
    QCOMPARE(!dragStartedSpy.isEmpty(), success);
    QVERIFY(!deviceInterface->dragSource());
    QCOMPARE(deviceInterface->origin(), success ? surfaceInterface : nullptr);
    QCOMPARE(deviceInterface->icon(), success ? iconSurfaceInterface : nullptr);
}

void TestDataDevice::testSetSelection()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    std::unique_ptr<Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataDeviceCreated(Wrapland::Server::DataDeviceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(deviceInterface);

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataSourceCreated(Wrapland::Server::DataSourceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto sourceInterface = dataSourceCreatedSpy.first().first().value<DataSourceInterface*>();
    QVERIFY(sourceInterface);

    // everything setup, now we can test setting the selection
    QSignalSpy selectionChangedSpy(deviceInterface, SIGNAL(selectionChanged(Wrapland::Server::DataSourceInterface*)));
    QVERIFY(selectionChangedSpy.isValid());
    QSignalSpy selectionClearedSpy(deviceInterface, SIGNAL(selectionCleared()));
    QVERIFY(selectionClearedSpy.isValid());

    QVERIFY(!deviceInterface->selection());
    dataDevice->setSelection(1, dataSource.get());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionClearedSpy.count(), 0);
    QCOMPARE(selectionChangedSpy.first().first().value<DataSourceInterface*>(), sourceInterface);
    QCOMPARE(deviceInterface->selection(), sourceInterface);

    // send selection to datadevice
    QSignalSpy selectionOfferedSpy(dataDevice.get(), SIGNAL(selectionOffered(Wrapland::Client::DataOffer*)));
    QVERIFY(selectionOfferedSpy.isValid());
    deviceInterface->sendSelection(deviceInterface);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);
    auto dataOffer = selectionOfferedSpy.first().first().value<DataOffer*>();
    QVERIFY(dataOffer);
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 1);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // sending a new mimetype to the selection, should be announced in the offer
    QSignalSpy mimeTypeAddedSpy(dataOffer, SIGNAL(mimeTypeOffered(QString)));
    QVERIFY(mimeTypeAddedSpy.isValid());
    dataSource->offer(QStringLiteral("text/html"));
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
    QVERIFY(!deviceInterface->selection());

    // set another selection
    dataDevice->setSelection(2, dataSource.get());
    QVERIFY(selectionChangedSpy.wait());
    // now unbind the dataDevice
    QSignalSpy unboundSpy(deviceInterface, &DataDeviceInterface::unbound);
    QVERIFY(unboundSpy.isValid());
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
    // send a selection to the unbound data device
    deviceInterface->sendSelection(deviceInterface);
}

void TestDataDevice::testSendSelectionOnSeat()
{
    // this test verifies that the selection is sent when setting a focused keyboard
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    // first add keyboard support to Seat
    QSignalSpy keyboardChangedSpy(m_seat, &Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create DataDevice, Keyboard and a Surface
    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, &DataDeviceManagerInterface::dataDeviceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());
    std::unique_ptr<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(serverDataDevice);
    std::unique_ptr<Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);

    // now set the selection
    std::unique_ptr<DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.get());
    // we should get a selection offered for that on the data device
    QSignalSpy selectionOfferedSpy(dataDevice.get(), &DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // now unfocus the keyboard
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    // if setting the same surface again, we should get another offer
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 2);

    // now let's try to destroy the data device and set a focused keyboard just while the data device is being destroyedd
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QSignalSpy unboundSpy(serverDataDevice, &Resource::unbound);
    QVERIFY(unboundSpy.isValid());
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
}

void TestDataDevice::testReplaceSource()
{
    // this test verifies that replacing a data source cancels the previous source
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    // first add keyboard support to Seat
    QSignalSpy keyboardChangedSpy(m_seat, &Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create DataDevice, Keyboard and a Surface
    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, &DataDeviceManagerInterface::dataDeviceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());
    std::unique_ptr<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(serverDataDevice);
    std::unique_ptr<Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);

    // now set the selection
    std::unique_ptr<DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.get());
    QSignalSpy sourceCancelledSpy(dataSource.get(), &DataSource::cancelled);
    QVERIFY(sourceCancelledSpy.isValid());
    // we should get a selection offered for that on the data device
    QSignalSpy selectionOfferedSpy(dataDevice.get(), &DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // create a second data source and replace previous one
    std::unique_ptr<DataSource> dataSource2(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource2->isValid());

    dataSource2->offer(QStringLiteral("text/plain"));

    QSignalSpy sourceCancelled2Spy(dataSource2.get(), &DataSource::cancelled);
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
    std::unique_ptr<DataDevice> dataDevice2(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice2->isValid());
    std::unique_ptr<DataSource> dataSource3(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource3->isValid());
    dataSource3->offer(QStringLiteral("text/plain"));
    dataDevice2->setSelection(1, dataSource3.get());
    QVERIFY(sourceCancelled2Spy.wait());

    // try to crash by first destroying dataSource3 and setting a new DataSource
    std::unique_ptr<DataSource> dataSource4(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource4->isValid());
    dataSource4->offer(QStringLiteral("text/plain"));
    dataSource3.reset();
    dataDevice2->setSelection(1, dataSource4.get());
    QVERIFY(selectionOfferedSpy.wait());
}

void TestDataDevice::testDestroy()
{
    using namespace Wrapland::Client;

    std::unique_ptr<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    connect(m_connection, &ConnectionThread::establishedChanged, m_dataDeviceManager, &DataDeviceManager::release);
    connect(m_connection, &ConnectionThread::establishedChanged, m_seat, &Seat::release);
    connect(m_connection, &ConnectionThread::establishedChanged, m_compositor, &Compositor::release);
    connect(m_connection, &ConnectionThread::establishedChanged, dataDevice.get(), &DataDevice::release);
    connect(m_connection, &ConnectionThread::establishedChanged, m_queue, &EventQueue::release);

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!m_connection->established());

    // Now the data device should be destroyed.
    QTRY_VERIFY(!dataDevice->isValid());

    // Calling destroy again should not fail.
    dataDevice->release();
}

QTEST_GUILESS_MAIN(TestDataDevice)
#include "test_datadevice.moc"
