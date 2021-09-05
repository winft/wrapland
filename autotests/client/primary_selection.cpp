/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "../../src/client/primary_selection.h"
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/pointer.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/primary_selection.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

#include <wayland-client.h>

#include <QThread>
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
    Wrapland::Server::primary_selection_device_manager* m_serverPrimarySelectionDeviceManager
        = nullptr;
    Wrapland::Server::Compositor* m_serverCompositor = nullptr;
    Wrapland::Server::Seat* m_serverSeat = nullptr;
    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::PrimarySelectionDeviceManager* m_deviceManager = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::Seat* m_seat = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    QThread* m_thread = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-primaryselection-0");

void TestPrimarySelection::init()
{
    qRegisterMetaType<Wrapland::Server::primary_selection_device*>();
    qRegisterMetaType<Wrapland::Server::primary_selection_source*>();
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
    QSignalSpy deviceManagerSpy(&registry,
                                SIGNAL(primarySelectionDeviceManagerAnnounced(quint32, quint32)));

    QVERIFY(deviceManagerSpy.isValid());
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

    m_serverPrimarySelectionDeviceManager
        = m_display->createPrimarySelectionDeviceManager(m_display);

    QVERIFY(deviceManagerSpy.wait());
    m_deviceManager = registry.createPrimarySelectionDeviceManager(
        deviceManagerSpy.first().first().value<quint32>(),
        deviceManagerSpy.first().last().value<quint32>(),
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
    if (m_deviceManager) {
        delete m_deviceManager;
        m_deviceManager = nullptr;
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
    QSignalSpy deviceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::device_created);
    QVERIFY(deviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> device(
        m_deviceManager->getDevice(m_seat));
    QVERIFY(device->isValid());

    QVERIFY(deviceCreatedSpy.wait());
    QCOMPARE(deviceCreatedSpy.count(), 1);

    auto serverDevice
        = deviceCreatedSpy.first().first().value<Wrapland::Server::primary_selection_device*>();
    QVERIFY(serverDevice);
    QCOMPARE(serverDevice->seat(), m_serverSeat);
    QVERIFY(!serverDevice->selection());

    QVERIFY(!m_serverSeat->primarySelection());
    m_serverSeat->setPrimarySelection(serverDevice->selection());
    QCOMPARE(m_serverSeat->primarySelection(), serverDevice->selection());

    // and destroy
    QSignalSpy destroyedSpy(serverDevice, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    device.reset();
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_serverSeat->primarySelection());
}

void TestPrimarySelection::testSetSelection()
{
    std::unique_ptr<Wrapland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy deviceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::device_created);
    QVERIFY(deviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> device(
        m_deviceManager->getDevice(m_seat));
    QVERIFY(device->isValid());

    QVERIFY(deviceCreatedSpy.wait());
    QCOMPARE(deviceCreatedSpy.count(), 1);
    auto serverDevice
        = deviceCreatedSpy.first().first().value<Wrapland::Server::primary_selection_device*>();
    QVERIFY(serverDevice);

    QSignalSpy sourceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::source_created);
    QVERIFY(deviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source(
        m_deviceManager->createSource());
    QVERIFY(source->isValid());
    source->offer(QStringLiteral("text/plain"));

    QVERIFY(sourceCreatedSpy.wait());
    QCOMPARE(sourceCreatedSpy.count(), 1);
    auto serverSource
        = sourceCreatedSpy.first().first().value<Wrapland::Server::primary_selection_source*>();
    QVERIFY(serverSource);

    // everything setup, now we can test setting the selection
    QSignalSpy selectionChangedSpy(serverDevice,
                                   &Wrapland::Server::primary_selection_device::selection_changed);
    QVERIFY(selectionChangedSpy.isValid());
    QSignalSpy selectionClearedSpy(serverDevice,
                                   &Wrapland::Server::primary_selection_device::selection_cleared);
    QVERIFY(selectionClearedSpy.isValid());

    QVERIFY(!serverDevice->selection());
    device->setSelection(1, source.get());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionClearedSpy.count(), 0);
    QCOMPARE(serverDevice->selection(), serverSource);

    // Send selection to device.
    QSignalSpy selectionOfferedSpy(device.get(),
                                   &Wrapland::Client::PrimarySelectionDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    serverDevice->send_selection(serverDevice->selection());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    auto offer
        = selectionOfferedSpy.first().first().value<Wrapland::Client::PrimarySelectionOffer*>();
    QVERIFY(offer);
    QCOMPARE(offer->offeredMimeTypes().count(), 1);
    QCOMPARE(offer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // sending a new mimetype to the selection, should be announced in the offer
    QSignalSpy mimeTypeAddedSpy(offer, SIGNAL(mimeTypeOffered(QString)));
    QVERIFY(mimeTypeAddedSpy.isValid());
    source->offer("text/html");
    QVERIFY(mimeTypeAddedSpy.wait());
    QCOMPARE(mimeTypeAddedSpy.count(), 1);
    QCOMPARE(mimeTypeAddedSpy.first().first().toString(), QStringLiteral("text/html"));
    QCOMPARE(offer->offeredMimeTypes().count(), 2);
    QCOMPARE(offer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QCOMPARE(offer->offeredMimeTypes().last().name(), QStringLiteral("text/html"));

    // now clear the selection
    device->clearSelection(1);
    QVERIFY(selectionClearedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionClearedSpy.count(), 1);
    QVERIFY(!serverDevice->selection());

    // set another selection
    device->setSelection(2, source.get());
    QVERIFY(selectionChangedSpy.wait());
    // now unbind the device
    QSignalSpy unboundSpy(serverDevice,
                          &Wrapland::Server::primary_selection_device::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());
    device.reset();
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

    // Now create device, Keyboard and a Surface.
    QSignalSpy deviceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::device_created);
    QVERIFY(deviceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> device(
        m_deviceManager->getDevice(m_seat));
    QVERIFY(device->isValid());
    QVERIFY(deviceCreatedSpy.wait());
    auto serverDevice
        = deviceCreatedSpy.first().first().value<Wrapland::Server::primary_selection_device*>();
    QVERIFY(serverDevice);
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
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source(
        m_deviceManager->createSource());
    QVERIFY(source->isValid());
    source->offer(QStringLiteral("text/plain"));
    device->setSelection(1, source.get());

    // We should get a selection offered for that on the device.
    QSignalSpy selectionOfferedSpy(device.get(),
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

    // Now let's try to destroy the device and set a focused keyboard just while the
    // device is being destroyed.
    m_serverSeat->setFocusedKeyboardSurface(nullptr);
    QSignalSpy unboundSpy(serverDevice,
                          &Wrapland::Server::primary_selection_device::resourceDestroyed);
    QVERIFY(unboundSpy.isValid());
    device.reset();
    QVERIFY(unboundSpy.wait());
    m_serverSeat->setFocusedKeyboardSurface(serverSurface);
}

void TestPrimarySelection::testReplaceSource()
{
    // This test verifies that replacing a source cancels the previous source.

    // First add keyboard support to Seat.
    QSignalSpy keyboardChangedSpy(m_seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_serverSeat->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create device, Keyboard and a Surface
    QSignalSpy deviceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::device_created);
    QVERIFY(deviceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> device(
        m_deviceManager->getDevice(m_seat));
    QVERIFY(device->isValid());
    QVERIFY(deviceCreatedSpy.wait());
    auto serverDevice
        = deviceCreatedSpy.first().first().value<Wrapland::Server::primary_selection_device*>();
    QVERIFY(serverDevice);
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
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source(
        m_deviceManager->createSource());
    QVERIFY(source->isValid());
    source->offer(QStringLiteral("text/plain"));
    device->setSelection(1, source.get());
    QSignalSpy sourceCancelledSpy(source.get(),
                                  &Wrapland::Client::PrimarySelectionSource::cancelled);
    QVERIFY(sourceCancelledSpy.isValid());
    // we should get a selection offered for that on the device
    QSignalSpy selectionOfferedSpy(device.get(),
                                   &Wrapland::Client::PrimarySelectionDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // create a second source and replace previous one
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source2(
        m_deviceManager->createSource());
    QVERIFY(source2->isValid());

    source2->offer(QStringLiteral("text/plain"));

    QSignalSpy sourceCancelled2Spy(source2.get(),
                                   &Wrapland::Client::PrimarySelectionSource::cancelled);
    QVERIFY(sourceCancelled2Spy.isValid());

    device->setSelection(1, source2.get());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    QVERIFY(sourceCancelledSpy.wait());
    QTRY_COMPARE(selectionOfferedSpy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // replace the source with itself, ensure that it did not get cancelled
    device->setSelection(1, source2.get());
    QVERIFY(!sourceCancelled2Spy.wait(500));
    QCOMPARE(selectionOfferedSpy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // create a new device and replace previous one
    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> device2(
        m_deviceManager->getDevice(m_seat));
    QVERIFY(device2->isValid());
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source3(
        m_deviceManager->createSource());
    QVERIFY(source3->isValid());
    source3->offer(QStringLiteral("text/plain"));
    device2->setSelection(1, source3.get());
    QVERIFY(sourceCancelled2Spy.wait());

    // try to crash by first destroying source3 and setting a new source
    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source4(
        m_deviceManager->createSource());
    QVERIFY(source4->isValid());
    source4->offer("text/plain");
    source3.reset();
    device2->setSelection(1, source4.get());
    QVERIFY(selectionOfferedSpy.wait());
}

void TestPrimarySelection::testOffer()
{
    qRegisterMetaType<Wrapland::Server::primary_selection_source*>();
    QSignalSpy sourceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::source_created);
    QVERIFY(sourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source(
        m_deviceManager->createSource());
    QVERIFY(source->isValid());

    QVERIFY(sourceCreatedSpy.wait());
    QCOMPARE(sourceCreatedSpy.count(), 1);

    QPointer<Wrapland::Server::primary_selection_source> serverSource
        = sourceCreatedSpy.first().first().value<Wrapland::Server::primary_selection_source*>();
    QVERIFY(!serverSource.isNull());
    QCOMPARE(serverSource->mime_types().size(), 0);

    QSignalSpy offeredSpy(serverSource.data(),
                          &Wrapland::Server::primary_selection_source::mime_type_offered);
    QVERIFY(offeredSpy.isValid());

    const std::string plain = "text/plain";
    QMimeDatabase db;
    source->offer(db.mimeTypeForName(QString::fromStdString(plain)));

    QVERIFY(offeredSpy.wait());
    QCOMPARE(offeredSpy.count(), 1);

    QCOMPARE(offeredSpy.last().first().value<std::string>(), plain);
    QCOMPARE(serverSource->mime_types().size(), 1);
    QCOMPARE(serverSource->mime_types().front(), plain);

    const std::string html = "text/html";
    source->offer(db.mimeTypeForName(QString::fromStdString(html)));

    QVERIFY(offeredSpy.wait());
    QCOMPARE(offeredSpy.count(), 2);
    QCOMPARE(offeredSpy.first().first().value<std::string>(), plain);
    QCOMPARE(offeredSpy.last().first().value<std::string>(), html);
    QCOMPARE(serverSource->mime_types().size(), 2);
    QCOMPARE(serverSource->mime_types().front(), plain);
    QCOMPARE(serverSource->mime_types().back(), html);

    // try destroying the client side, should trigger a destroy of server side
    source.reset();
    QVERIFY(!serverSource.isNull());
    wl_display_flush(m_connection->display());
    // After running the event loop the Wayland event should be delivered.
    QCoreApplication::processEvents();
    QVERIFY(serverSource.isNull());
}

void TestPrimarySelection::testRequestSend()
{
    QSignalSpy sourceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::source_created);
    QVERIFY(sourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source(
        m_deviceManager->createSource());
    QVERIFY(source->isValid());

    QSignalSpy sendRequestedSpy(source.get(),
                                &Wrapland::Client::PrimarySelectionSource::sendDataRequested);
    QVERIFY(sendRequestedSpy.isValid());

    const std::string plain = "text/plain";
    QVERIFY(sourceCreatedSpy.wait());
    QCOMPARE(sourceCreatedSpy.count(), 1);
    QTemporaryFile file;
    QVERIFY(file.open());
    sourceCreatedSpy.first()
        .first()
        .value<Wrapland::Server::primary_selection_source*>()
        ->request_data(plain, file.handle());

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
    QSignalSpy sourceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::source_created);
    QVERIFY(sourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source(
        m_deviceManager->createSource());
    QVERIFY(source->isValid());
    QSignalSpy cancelledSpy(source.get(), &Wrapland::Client::PrimarySelectionSource::cancelled);
    QVERIFY(cancelledSpy.isValid());

    QVERIFY(sourceCreatedSpy.wait());

    QCOMPARE(cancelledSpy.count(), 0);
    sourceCreatedSpy.first().first().value<Wrapland::Server::primary_selection_source*>()->cancel();

    QVERIFY(cancelledSpy.wait());
    QCOMPARE(cancelledSpy.count(), 1);
}

void TestPrimarySelection::testServerGet()
{
    QSignalSpy sourceCreatedSpy(
        m_serverPrimarySelectionDeviceManager,
        &Wrapland::Server::primary_selection_device_manager::source_created);
    QVERIFY(sourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::PrimarySelectionSource> source(
        m_deviceManager->createSource());
    QVERIFY(source->isValid());

    QVERIFY(sourceCreatedSpy.wait());
    auto sourceSpy
        = sourceCreatedSpy.first().first().value<Wrapland::Server::primary_selection_source*>();
    QVERIFY(sourceSpy);
}

void TestPrimarySelection::testDestroy()
{
    std::unique_ptr<Wrapland::Client::PrimarySelectionDevice> device(
        m_deviceManager->getDevice(m_seat));
    QVERIFY(device->isValid());

    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_deviceManager,
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
            device.get(),
            &Wrapland::Client::PrimarySelectionDevice::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_queue,
            &Wrapland::Client::EventQueue::release);

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!m_connection->established());

    // Now the device should be destroyed.
    QTRY_VERIFY(!device->isValid());

    // Calling destroy again should not fail.
    device->release();
}

QTEST_GUILESS_MAIN(TestPrimarySelection)
#include "primary_selection.moc"
