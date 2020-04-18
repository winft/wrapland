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
#include <QMimeDatabase>
#include <QtTest>

#include "../../src/client/connection_thread.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"

#include "../../server/data_device_manager.h"
#include "../../server/data_source.h"
#include "../../server/display.h"

#include <wayland-client.h>

class TestDataSource : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testOffer();
    void testTargetAccepts_data();
    void testTargetAccepts();
    void testRequestSend();
#if 0
    void testRequestSendOnUnbound();
#endif
    void testCancel();
    void testServerGet();
    void testDestroy();

private:
    Wrapland::Server::D_isplay* m_display = nullptr;
    Wrapland::Server::DataDeviceManager* m_serverDataDeviceManager = nullptr;
    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::DataDeviceManager* m_dataDeviceManager = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    QThread* m_thread = nullptr;
};

static const std::string s_socketName = "kwayland-test-wayland-datasource-0";

void TestDataSource::init()
{
    qRegisterMetaType<std::string>();
    m_display = new Wrapland::Server::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(QString::fromStdString(s_socketName));

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

    Wrapland::Client::Registry registry;
    QSignalSpy dataDeviceManagerSpy(&registry,
                                    &Wrapland::Client::Registry::dataDeviceManagerAnnounced);
    QVERIFY(dataDeviceManagerSpy.isValid());
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
}

void TestDataSource::cleanup()
{
    if (m_dataDeviceManager) {
        delete m_dataDeviceManager;
        m_dataDeviceManager = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
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

void TestDataSource::testOffer()
{
    qRegisterMetaType<Wrapland::Server::DataSource*>();
    QSignalSpy dataSourceCreatedSpy(m_serverDataDeviceManager,
                                    &Wrapland::Server::DataDeviceManager::dataSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);

    QPointer<Wrapland::Server::DataSource> serverDataSource
        = dataSourceCreatedSpy.first().first().value<Wrapland::Server::DataSource*>();
    QVERIFY(!serverDataSource.isNull());
    QCOMPARE(serverDataSource->mimeTypes().size(), 0);

    QSignalSpy offeredSpy(serverDataSource.data(), &Wrapland::Server::DataSource::mimeTypeOffered);
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

void TestDataSource::testTargetAccepts_data()
{
    QTest::addColumn<QString>("mimeType");

    QTest::newRow("empty") << "";
    QTest::newRow("text/plain") << "text/plain";
}

void TestDataSource::testTargetAccepts()
{
    QSignalSpy dataSourceCreatedSpy(m_serverDataDeviceManager,
                                    &Wrapland::Server::DataDeviceManager::dataSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QSignalSpy targetAcceptsSpy(dataSource.get(), &Wrapland::Client::DataSource::targetAccepts);
    QVERIFY(targetAcceptsSpy.isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);

    QFETCH(QString, mimeType);
    dataSourceCreatedSpy.first().first().value<Wrapland::Server::DataSource*>()->accept(
        mimeType.toUtf8().constData());

    QVERIFY(targetAcceptsSpy.wait());
    QCOMPARE(targetAcceptsSpy.count(), 1);
    QCOMPARE(targetAcceptsSpy.first().first().toString(), mimeType);
}

void TestDataSource::testRequestSend()
{
    QSignalSpy dataSourceCreatedSpy(m_serverDataDeviceManager,
                                    &Wrapland::Server::DataDeviceManager::dataSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QSignalSpy sendRequestedSpy(dataSource.get(), &Wrapland::Client::DataSource::sendDataRequested);
    QVERIFY(sendRequestedSpy.isValid());

    const std::string plain = "text/plain";
    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    QTemporaryFile file;
    QVERIFY(file.open());
    dataSourceCreatedSpy.first().first().value<Wrapland::Server::DataSource*>()->requestData(
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

// TODO: Let's try to make it impossible that this can happen in the first place.
#if 0
void TestDataSource::testRequestSendOnUnbound()
{
    // This test verifies that the server doesn't crash when requesting a send on an unbound
    // DataSource.
    QSignalSpy dataSourceCreatedSpy(m_serverDataDeviceManager, &Wrapland::Server::DataDeviceManager::dataSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto sds = dataSourceCreatedSpy.first().first().value<Wrapland::Server::DataSource*>();
    QVERIFY(sds);

    QSignalSpy destroyedSpy(sds, &Wrapland::Server::DataSource::resourceDestroyed);
    QVERIFY(destroyedSpy.isValid());
    dataSource.reset();
    QVERIFY(destroyedSpy.wait());
    sds->requestData("text/plain", -1);
}
#endif

void TestDataSource::testCancel()
{
    QSignalSpy dataSourceCreatedSpy(m_serverDataDeviceManager,
                                    &Wrapland::Server::DataDeviceManager::dataSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    QSignalSpy cancelledSpy(dataSource.get(), &Wrapland::Client::DataSource::cancelled);
    QVERIFY(cancelledSpy.isValid());

    QVERIFY(dataSourceCreatedSpy.wait());

    QCOMPARE(cancelledSpy.count(), 0);
    dataSourceCreatedSpy.first().first().value<Wrapland::Server::DataSource*>()->cancel();

    QVERIFY(cancelledSpy.wait());
    QCOMPARE(cancelledSpy.count(), 1);
}

void TestDataSource::testServerGet()
{
    QSignalSpy dataSourceCreatedSpy(m_serverDataDeviceManager,
                                    &Wrapland::Server::DataDeviceManager::dataSourceCreated);
    QVERIFY(dataSourceCreatedSpy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    auto source = dataSourceCreatedSpy.first().first().value<Wrapland::Server::DataSource*>();
    QVERIFY(source);
}

void TestDataSource::testDestroy()
{
    std::unique_ptr<Wrapland::Client::DataSource> dataSource(
        m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_dataDeviceManager,
            &Wrapland::Client::DataDeviceManager::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_queue,
            &Wrapland::Client::EventQueue::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            dataSource.get(),
            &Wrapland::Client::DataSource::release);

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!m_connection->established());

    // Now the pool should be destroyed.
    QTRY_VERIFY(!dataSource->isValid());

    // Calling destroy again should not fail.
    dataSource->release();
}

QTEST_GUILESS_MAIN(TestDataSource)
#include "data_source.moc"
