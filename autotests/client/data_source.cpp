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

    void test_offer();
    void test_target_accepts_data();
    void test_target_accepts();
    void test_request_send();
#if 0
    void test_request_send_on_unbound();
#endif
    void test_cancel();
    void test_server_get();
    void test_destroy();

private:
    Wrapland::Server::Display* m_display = nullptr;
    Wrapland::Server::data_device_manager* m_server_device_manager = nullptr;
    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Client::DataDeviceManager* m_device_manager = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    QThread* m_thread = nullptr;
};

constexpr auto socket_name{"kwayland-test-wayland-datasource-0"};

void TestDataSource::init()
{
    qRegisterMetaType<std::string>();
    m_display = new Wrapland::Server::Display(this);
    m_display->set_socket_name(socket_name);
    m_display->start();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connected_spy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(QString::fromStdString(socket_name));

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

    Wrapland::Client::Registry registry;
    QSignalSpy device_manager_spy(&registry,
                                  &Wrapland::Client::Registry::dataDeviceManagerAnnounced);
    QVERIFY(device_manager_spy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_server_device_manager = m_display->createDataDeviceManager(m_display);

    QVERIFY(device_manager_spy.wait());
    m_device_manager
        = registry.createDataDeviceManager(device_manager_spy.first().first().value<quint32>(),
                                           device_manager_spy.first().last().value<quint32>(),
                                           this);
}

void TestDataSource::cleanup()
{
    if (m_device_manager) {
        delete m_device_manager;
        m_device_manager = nullptr;
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

void TestDataSource::test_offer()
{
    qRegisterMetaType<Wrapland::Server::data_source*>();
    QSignalSpy source_created_spy(m_server_device_manager,
                                  &Wrapland::Server::data_device_manager::source_created);
    QVERIFY(source_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());

    QVERIFY(source_created_spy.wait());
    QCOMPARE(source_created_spy.count(), 1);

    QPointer<Wrapland::Server::data_source> server_source
        = source_created_spy.first().first().value<Wrapland::Server::data_source*>();
    QVERIFY(!server_source.isNull());
    QCOMPARE(server_source->mime_types().size(), 0);

    QSignalSpy offered_spy(server_source.data(), &Wrapland::Server::data_source::mime_type_offered);
    QVERIFY(offered_spy.isValid());

    const std::string plain = "text/plain";
    QMimeDatabase db;
    source->offer(db.mimeTypeForName(QString::fromStdString(plain)));

    QVERIFY(offered_spy.wait());
    QCOMPARE(offered_spy.count(), 1);

    QCOMPARE(offered_spy.last().first().value<std::string>(), plain);
    QCOMPARE(server_source->mime_types().size(), 1);
    QCOMPARE(server_source->mime_types().front(), plain);

    const std::string html = "text/html";
    source->offer(db.mimeTypeForName(QString::fromStdString(html)));

    QVERIFY(offered_spy.wait());
    QCOMPARE(offered_spy.count(), 2);
    QCOMPARE(offered_spy.first().first().value<std::string>(), plain);
    QCOMPARE(offered_spy.last().first().value<std::string>(), html);
    QCOMPARE(server_source->mime_types().size(), 2);
    QCOMPARE(server_source->mime_types().front(), plain);
    QCOMPARE(server_source->mime_types().back(), html);

    // try destroying the client side, should trigger a destroy of server side
    source.reset();
    QVERIFY(!server_source.isNull());
    wl_display_flush(m_connection->display());
    // After running the event loop the Wayland event should be delivered.
    QCoreApplication::processEvents();
    QVERIFY(server_source.isNull());
}

void TestDataSource::test_target_accepts_data()
{
    QTest::addColumn<QString>("mimeType");

    QTest::newRow("empty") << "";
    QTest::newRow("text/plain") << "text/plain";
}

void TestDataSource::test_target_accepts()
{
    QSignalSpy source_created_spy(m_server_device_manager,
                                  &Wrapland::Server::data_device_manager::source_created);
    QVERIFY(source_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());

    QSignalSpy target_accepts_spy(source.get(), &Wrapland::Client::DataSource::targetAccepts);
    QVERIFY(target_accepts_spy.isValid());

    QVERIFY(source_created_spy.wait());
    QCOMPARE(source_created_spy.count(), 1);

    QFETCH(QString, mimeType);
    source_created_spy.first().first().value<Wrapland::Server::data_source*>()->accept(
        mimeType.toUtf8().constData());

    QVERIFY(target_accepts_spy.wait());
    QCOMPARE(target_accepts_spy.count(), 1);
    QCOMPARE(target_accepts_spy.first().first().toString(), mimeType);
}

void TestDataSource::test_request_send()
{
    QSignalSpy source_created_spy(m_server_device_manager,
                                  &Wrapland::Server::data_device_manager::source_created);
    QVERIFY(source_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());

    QSignalSpy send_requested_spy(source.get(), &Wrapland::Client::DataSource::sendDataRequested);
    QVERIFY(send_requested_spy.isValid());

    const std::string plain = "text/plain";
    QVERIFY(source_created_spy.wait());
    QCOMPARE(source_created_spy.count(), 1);
    QTemporaryFile file;
    QVERIFY(file.open());
    source_created_spy.first().first().value<Wrapland::Server::data_source*>()->request_data(
        plain, file.handle());

    QVERIFY(send_requested_spy.wait());
    QCOMPARE(send_requested_spy.count(), 1);
    QCOMPARE(send_requested_spy.first().first().toString().toUtf8().constData(), plain);
    QCOMPARE(send_requested_spy.first().last().value<qint32>(), file.handle());
    QVERIFY(send_requested_spy.first().last().value<qint32>() != -1);

    QFile writeFile;
    QVERIFY(writeFile.open(send_requested_spy.first().last().value<qint32>(),
                           QFile::WriteOnly,
                           QFileDevice::AutoCloseHandle));
    writeFile.close();
}

// TODO: Let's try to make it impossible that this can happen in the first place.
#if 0
void TestDataSource::test_request_send_on_unbound()
{
    // This test verifies that the server doesn't crash when requesting a send on an unbound
    // DataSource.
    QSignalSpy source_created_spy(m_server_device_manager, &Wrapland::Server::data_device_manager::sourceCreated);
    QVERIFY(source_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());
    QVERIFY(source_created_spy.wait());
    QCOMPARE(source_created_spy.count(), 1);
    auto sds = source_created_spy.first().first().value<Wrapland::Server::data_source*>();
    QVERIFY(sds);

    QSignalSpy destroyedSpy(sds, &Wrapland::Server::data_source::resourceDestroyed);
    QVERIFY(destroyedSpy.isValid());
    source.reset();
    QVERIFY(destroyedSpy.wait());
    sds->requestData("text/plain", -1);
}
#endif

void TestDataSource::test_cancel()
{
    QSignalSpy source_created_spy(m_server_device_manager,
                                  &Wrapland::Server::data_device_manager::source_created);
    QVERIFY(source_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());
    QSignalSpy cancelled_spy(source.get(), &Wrapland::Client::DataSource::cancelled);
    QVERIFY(cancelled_spy.isValid());

    QVERIFY(source_created_spy.wait());

    QCOMPARE(cancelled_spy.count(), 0);
    source_created_spy.first().first().value<Wrapland::Server::data_source*>()->cancel();

    QVERIFY(cancelled_spy.wait());
    QCOMPARE(cancelled_spy.count(), 1);
}

void TestDataSource::test_server_get()
{
    QSignalSpy source_created_spy(m_server_device_manager,
                                  &Wrapland::Server::data_device_manager::source_created);
    QVERIFY(source_created_spy.isValid());

    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());

    QVERIFY(source_created_spy.wait());
    auto server_source = source_created_spy.first().first().value<Wrapland::Server::data_source*>();
    QVERIFY(server_source);
}

void TestDataSource::test_destroy()
{
    std::unique_ptr<Wrapland::Client::DataSource> source(m_device_manager->createSource());
    QVERIFY(source->isValid());

    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_device_manager,
            &Wrapland::Client::DataDeviceManager::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            m_queue,
            &Wrapland::Client::EventQueue::release);
    connect(m_connection,
            &Wrapland::Client::ConnectionThread::establishedChanged,
            source.get(),
            &Wrapland::Client::DataSource::release);

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!m_connection->established());

    // Now the pool should be destroyed.
    QTRY_VERIFY(!source->isValid());

    // Calling destroy again should not fail.
    source->release();
}

QTEST_GUILESS_MAIN(TestDataSource)
#include "data_source.moc"
