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
#include "../../src/client/primary_selection.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/pointer.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/primary_selection.h"
#include "../../server/display.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

#include <wayland-client.h>

#include <QtTest>

class TestPrimarySelection : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSetSelection();
    void testSendSelectionOnSeat();
    void testReplaceSource();
    void testOffer();
    void testRequestSend();
    void testCancel();
    void testServerGet();

    void testDestroy();

private:
    Wrapland::Server::Display* m_display = nullptr;
    Wrapland::Server::PrimarySelectionDeviceManager* m_serverPrimarySelectionDeviceManager = nullptr;
    Wrapland::Server::Compositor* m_serverCompositor = nullptr;
    Wrapland::Server::Seat* m_serverSeat = nullptr;
    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::PrimarySelectionDeviceManager* m_dataDeviceManager = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    QThread* m_thread = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-primaryselection-0");

void TestPrimarySelection::init()
{
    qRegisterMetaType<Wrapland::Server::PrimarySelectionDevice*>();
    qRegisterMetaType<Wrapland::Server::PrimarySelectionSource*>();
    qRegisterMetaType<Wrapland::Server::Surface*>();
    qRegisterMetaType<std::string>();

    m_display = new Wrapland::Server::Display(this);
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
                                    SIGNAL(primarySelectionDeviceManagerAnnounced(quint32, quint32)));
    
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

    m_serverPrimarySelectionDeviceManager = m_display->createPrimarySelectionDeviceManager(m_display);

    QVERIFY(dataDeviceManagerSpy.wait());
    m_dataDeviceManager
        = registry.createPrimarySelectionDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(),
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

void TestPrimarySelection::cleanup()
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

void TestPrimarySelection::testCreate()
{
    QSignalSpy dataDeviceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    SIGNAL(primarySelectionDeviceCreated(Wrapland::Server::PrimarySelectionDevice*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> dataDevice(
        m_dataDeviceManager->getPrimarySelectionDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);

    auto serverDevice = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionDevice*>();
    QVERIFY(serverDevice);
    QCOMPARE(serverDevice->seat(), m_serverSeat);
    QVERIFY(!serverDevice->selection());

    QVERIFY(!m_serverSeat->primarySelectionSelection());
    m_serverSeat->setPrimarySelectionSelection(serverDevice);
    QCOMPARE(m_serverSeat->primarySelectionSelection(), serverDevice);

    // and destroy
    QSignalSpy destroyedSpy(serverDevice, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    dataDevice.reset();
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_serverSeat->primarySelectionSelection());
}

void TestPrimarySelection::testSetSelection()
{
    std::unique_ptr<Wrapland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    SIGNAL(primarySelectionDeviceCreated(Wrapland::Server::PrimarySelectionDevice*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> dataDevice(
        m_dataDeviceManager->getPrimarySelectionDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto serverDevice = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionDevice*>();
    QVERIFY(serverDevice);

    QSignalSpy dataSourceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    SIGNAL(primarySelectionSourceCreated(Wrapland::Server::PrimarySelectionSource*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto serverSource = dataSourceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionSource*>();
    QVERIFY(serverSource);

    // everything setup, now we can test setting the selection
    QSignalSpy selectionChangedSpy(serverDevice, &Wrapland::Server::PrimarySelectionDevice::selectionChanged);
    QVERIFY(selectionChangedSpy.isValid());
    QSignalSpy selectionClearedSpy(serverDevice, SIGNAL(selectionCleared()));
    QVERIFY(selectionClearedSpy.isValid());

    QVERIFY(!serverDevice->selection());
    dataDevice->setSelection(1, dataSource.get());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionClearedSpy.count(), 0);
    QCOMPARE(selectionChangedSpy.first().first().value<Wrapland::Server::PrimarySelectionSource*>(),
             serverSource);
    QCOMPARE(serverDevice->selection(), serverSource);

    // Send selection to datadevice.
    QSignalSpy selectionOfferedSpy(dataDevice.get(),
                                   &Wrapland::Client::PrimarySelectionDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    serverDevice->sendSelection(serverDevice);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    auto dataOffer = selectionOfferedSpy.first().first().value<Wrapland::Client::PrimarySelectionOffer*>();
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
    QSignalSpy unboundSpy(serverDevice, &Wrapland::Server::PrimarySelectionDevice::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
}

void TestPrimarySelection::testSendSelectionOnSeat()
{
    // This test verifies that the selection is sent when setting a focused keyboard.

    // First add keyboard support to Seat.
    QSignalSpy keyboardChangedSpy(m_seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_serverSeat->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());

    // Now create DataDevice, Keyboard and a Surface.
    QSignalSpy dataDeviceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    &Wrapland::Server::PrimarySelectionDeviceManager::primarySelectionDeviceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> dataDevice(
        m_dataDeviceManager->getPrimarySelectionDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice
        = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionDevice*>();
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
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.get());

    // We should get a selection offered for that on the data device.
    QSignalSpy selectionOfferedSpy(dataDevice.get(),
                                   &Wrapland::Client::PrimarySelectionDevice::selectionOffered);
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
    QSignalSpy unboundSpy(serverDataDevice, &Wrapland::Server::PrimarySelectionDevice::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
}

void TestPrimarySelection::testReplaceSource()
{
    // This test verifies that replacing a data source cancels the previous source.

    // First add keyboard support to Seat.
    QSignalSpy keyboardChangedSpy(m_seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_serverSeat->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create DataDevice, Keyboard and a Surface
    QSignalSpy dataDeviceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    &Wrapland::Server::PrimarySelectionDeviceManager::primarySelectionDeviceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> dataDevice(
        m_dataDeviceManager->getPrimarySelectionDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice
        = dataDeviceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionDevice*>();
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
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.get());
    QSignalSpy sourceCancelledSpy(dataSource.get(), &Wrapland::Client::PrimarySelectionSource::cancelled);
    QVERIFY(sourceCancelledSpy.isValid());
    // we should get a selection offered for that on the data device
    QSignalSpy selectionOfferedSpy(dataDevice.get(),
                                   &Wrapland::Client::PrimarySelectionDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // create a second data source and replace previous one
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource2(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource2->isValid());

    dataSource2->offer(QStringLiteral("text/plain"));

    QSignalSpy sourceCancelled2Spy(dataSource2.get(), &Wrapland::Client::PrimarySelectionSource::cancelled);
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
    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> dataDevice2(
        m_dataDeviceManager->getPrimarySelectionDevice(m_seat));
    QVERIFY(dataDevice2->isValid());
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource3(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource3->isValid());
    dataSource3->offer(QStringLiteral("text/plain"));
    dataDevice2->setSelection(1, dataSource3.get());
    QVERIFY(sourceCancelled2Spy.wait());

    // try to crash by first destroying dataSource3 and setting a new DataSource
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource4(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource4->isValid());
    dataSource4->offer("text/plain");
    dataSource3.reset();
    dataDevice2->setSelection(1, dataSource4.get());
    QVERIFY(selectionOfferedSpy.wait());
}

void TestPrimarySelection::testOffer()
{
    qRegisterMetaType<Wrapland::Server::PrimarySelectionSource*>();
    QSignalSpy dataSourceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    &Wrapland::Server::PrimarySelectionDeviceManager::primarySelectionSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);

    QPointer<Wrapland::Server::PrimarySelectionSource> serverDataSource
        = dataSourceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionSource*>();
    QVERIFY(!serverDataSource.isNull());
    QCOMPARE(serverDataSource->mimeTypes().size(), 0);

    QSignalSpy offeredSpy(serverDataSource.data(), &Wrapland::Server::PrimarySelectionSource::mimeTypeOffered);
    QVERIFY(offeredSpy.isValid());

    const std::string plain = "text/plain";
    QMimeDatabase db;
    dataSource->offer(db.mimeTypeForName(QString::fromStdString(plain)));

    QVERIFY(offeredSpy.wait());
    QCOMPARE(offeredSpy.count(), 1);

    QCOMPARE(offeredSpy.last().first().value<std::string>(), plain);
    QCOMPARE(serverDataSource->mimeTypes().size(), 1);
    QCOMPARE(serverDataSource->mimeTypes().front(), plain);

    const std::string html = "text/html";
    dataSource->offer(db.mimeTypeForName(QString::fromStdString(html)));

    QVERIFY(offeredSpy.wait());
    QCOMPARE(offeredSpy.count(), 2);
    QCOMPARE(offeredSpy.first().first().value<std::string>(), plain);
    QCOMPARE(offeredSpy.last().first().value<std::string>(), html);
    QCOMPARE(serverDataSource->mimeTypes().size(), 2);
    QCOMPARE(serverDataSource->mimeTypes().front(), plain);
    QCOMPARE(serverDataSource->mimeTypes().back(), html);

    // try destroying the client side, should trigger a destroy of server side
    dataSource.reset();
    QVERIFY(!serverDataSource.isNull());
    wl_display_flush(m_connection->display());
    // After running the event loop the Wayland event should be delivered.
    QCoreApplication::processEvents();
    QVERIFY(serverDataSource.isNull());
}

void TestPrimarySelection::testRequestSend()
{
    QSignalSpy dataSourceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    &Wrapland::Server::PrimarySelectionDeviceManager::primarySelectionSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource->isValid());

    QSignalSpy sendRequestedSpy(dataSource.get(), &Wrapland::Client::PrimarySelectionSource::sendDataRequested);
    QVERIFY(sendRequestedSpy.isValid());

    const std::string plain = "text/plain";
    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    QTemporaryFile file;
    QVERIFY(file.open());
    dataSourceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionSource*>()->requestData(
        plain, file.handle());

    QVERIFY(sendRequestedSpy.wait());
    QCOMPARE(sendRequestedSpy.count(), 1);
    QCOMPARE(sendRequestedSpy.first().first().toString().toUtf8().constData(), plain);
    QCOMPARE(sendRequestedSpy.first().last().value<qint32>(), file.handle());
    QVERIFY(sendRequestedSpy.first().last().value<qint32>() != -1);

    QFile writeFile;
    QVERIFY(writeFile.open(sendRequestedSpy.first().last().value<qint32>(),
                           QFile::WriteOnly,
                           QFileDevice::AutoCloseHandle));
    writeFile.close();
}



void TestPrimarySelection::testCancel()
{
    QSignalSpy dataSourceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    &Wrapland::Server::PrimarySelectionDeviceManager::primarySelectionSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource->isValid());
    QSignalSpy cancelledSpy(dataSource.get(), &Wrapland::Client::PrimarySelectionSource::cancelled);
    QVERIFY(cancelledSpy.isValid());

    QVERIFY(dataSourceCreatedSpy.wait());

    QCOMPARE(cancelledSpy.count(), 0);
    dataSourceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionSource*>()->cancel();

    QVERIFY(cancelledSpy.wait());
    QCOMPARE(cancelledSpy.count(), 1);
}

void TestPrimarySelection::testServerGet()
{
    QSignalSpy dataSourceCreatedSpy(m_serverPrimarySelectionDeviceManager,
                                    &Wrapland::Server::PrimarySelectionDeviceManager::primarySelectionSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> dataSource(
        m_dataDeviceManager->createPrimarySelectionSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    auto source = dataSourceCreatedSpy.first().first().value<Wrapland::Server::PrimarySelectionSource*>();
    QVERIFY(source);
}

void TestPrimarySelection::testDestroy()
{
    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> dataDevice(
        m_dataDeviceManager->getPrimarySelectionDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_dataDeviceManager,
            &Wrapland::Client::PrimarySelectionDeviceManager::release);
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
            &Wrapland::Client::PrimarySelectionDevice::release);
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

QTEST_GUILESS_MAIN(TestPrimarySelection)
#include "primary_selection.moc"
