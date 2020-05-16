/********************************************************************
Copyright 2016  Oleg Chernovskiy <kanedias@xaker.ru>
Copyright 2018  Roman Gilg <subdiff@gmail.com>

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

#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/remote_access.h"
#include "../../src/client/registry.h"
#include "../../src/client/output.h"

#include "../../server/display.h"
#include "../../server/output.h"
#include "../../server/remote_access.h"

#include <linux/input.h>

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

Q_DECLARE_METATYPE(Clt::RemoteBuffer const*)
Q_DECLARE_METATYPE(void const*)

class RemoteAccessTest : public QObject
{
    Q_OBJECT
public:
    explicit RemoteAccessTest(QObject *parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void testSendReleaseSingle();
    void testSendReleaseMultiple();
    void testSendReleaseCrossScreen();
    void testSendClientGone();
    void testSendReceiveClientGone();

private:
    Srv::D_isplay *m_display = nullptr;
    Srv::Output *m_serverOutput[2] = {nullptr};
    Srv::RemoteAccessManager *m_serverRemoteAccess = nullptr;
};

class MockupClient : public QObject
{
    Q_OBJECT
public:
    MockupClient(QObject *parent = nullptr);
    ~MockupClient();

    void bindOutput(int index);

    Clt::ConnectionThread *connection = nullptr;
    QThread *thread = nullptr;
    Clt::EventQueue *queue = nullptr;
    Clt::Registry *registry = nullptr;
    Clt::RemoteAccessManager *remoteAccess = nullptr;
    Clt::Output *outputs[2] = {nullptr};
};

static const QString s_socketName = QStringLiteral("wrapland-test-remote-access-0");

MockupClient::MockupClient(QObject *parent)
    : QObject(parent)
{
    // setup connection
    connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    connection->setSocketName(s_socketName);

    thread = new QThread(this);
    connection->moveToThread(thread);
    thread->start();

    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    queue = new Clt::EventQueue(this);
    queue->setup(connection);

    registry = new Clt::Registry(this);
    QSignalSpy interfacesAnnouncedSpy(registry, &Clt::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry->setEventQueue(queue);
    registry->create(connection);
    QVERIFY(registry->isValid());
    registry->setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    remoteAccess = registry->createRemoteAccessManager(
                                            registry->interface(Clt::Registry::Interface::RemoteAccessManager).name,
                                            registry->interface(Clt::Registry::Interface::RemoteAccessManager).version,
                                            this);
    QVERIFY(remoteAccess->isValid());
    connection->flush();
}

MockupClient::~MockupClient()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(outputs[0])
    CLEANUP(outputs[1])
    CLEANUP(remoteAccess)
    CLEANUP(queue)
    CLEANUP(registry)
    if (thread) {
        if (connection) {
            connection->flush();
            connection->deleteLater();
            connection = nullptr;
        }

        thread->quit();
        thread->wait();
        delete thread;
        thread = nullptr;
    }
#undef CLEANUP
}

void MockupClient::bindOutput(int index)
{
    // client-bound output
    outputs[index] = registry->createOutput(registry->interfaces(Clt::Registry::Interface::Output)[index].name,
                             registry->interfaces(Clt::Registry::Interface::Output)[index].version,
                             this);
    QVERIFY(outputs[index]->isValid());
    connection->flush();
}

RemoteAccessTest::RemoteAccessTest(QObject *parent)
    : QObject(parent)
{
//    qRegisterMetaType<Srv::RemoteBufferHandle const*>();
}

void RemoteAccessTest::init()
{
//    qRegisterMetaType<Clt::RemoteBuffer const*>();
//    qRegisterMetaType<void const*>();

    delete m_display;
    m_display = new Srv::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->running());

    m_display->createShm();

    auto initOutputIface = [this](int i) {
        m_serverOutput[i] = m_display->createOutput();
    };
    initOutputIface(0);
    initOutputIface(1);

    m_serverRemoteAccess = m_display->createRemoteAccessManager();

    QSignalSpy bufferReleasedSpy(m_serverRemoteAccess, &Srv::RemoteAccessManager::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
}

void RemoteAccessTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_serverOutput[0])
    CLEANUP(m_serverOutput[1])
    CLEANUP(m_serverRemoteAccess)
    CLEANUP(m_display)
#undef CLEANUP
}

void RemoteAccessTest::testSendReleaseSingle()
{
    // this test verifies that a buffer is sent to client and returned back

    // setup
    QVERIFY(!m_serverRemoteAccess->bound());
    auto *client = new MockupClient(this);
    client->bindOutput(0);

    m_display->dispatchEvents();

    QVERIFY(m_serverRemoteAccess->bound()); // we have one client now
    QSignalSpy bufferReadySpy(client->remoteAccess, &Clt::RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy.isValid());

    auto buf = new Srv::RemoteBufferHandle();
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->open();

    buf->setFd(tmpFile->handle());
    buf->setSize(50, 50);
    buf->setFormat(100500);
    buf->setStride(7800);

    m_serverRemoteAccess->sendBufferReady(m_serverOutput[0], buf);

    // Receive buffer.
    QVERIFY(bufferReadySpy.wait());
    auto rbuf = bufferReadySpy.takeFirst()[1].value<Clt::RemoteBuffer*>();

    QSignalSpy paramsObtainedSpy(rbuf, &Clt::RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy.isValid());

    // wait for params
    QVERIFY(paramsObtainedSpy.wait());

    // client fd is different, not subject to check
    QCOMPARE(rbuf->width(), 50u);
    QCOMPARE(rbuf->height(), 50u);
    QCOMPARE(rbuf->format(), 100500u);
    QCOMPARE(rbuf->stride(), 7800u);

    // release
    QSignalSpy bufferReleasedSpy(m_serverRemoteAccess, &Srv::RemoteAccessManager::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
    delete rbuf;
    QVERIFY(bufferReleasedSpy.wait());

    // cleanup
    delete buf;
    delete client;
    m_display->dispatchEvents();
    QVERIFY(!m_serverRemoteAccess->bound());
}

void RemoteAccessTest::testSendReleaseMultiple()
{
    // this test verifies that a buffer is sent to 2 clients and returned back

    // setup
    QVERIFY(!m_serverRemoteAccess->bound());
    auto *client1 = new MockupClient(this);
    auto *client2 = new MockupClient(this);
    client1->bindOutput(0);
    client2->bindOutput(0);
    m_display->dispatchEvents();
    QVERIFY(m_serverRemoteAccess->bound()); // now we have 2 clients

    QSignalSpy bufferReadySpy1(client1->remoteAccess, &Clt::RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy1.isValid());
    QSignalSpy bufferReadySpy2(client2->remoteAccess, &Clt::RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy2.isValid());

    auto buf = new Srv::RemoteBufferHandle();
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->open();

    buf->setFd(tmpFile->handle());
    buf->setSize(50, 50);
    buf->setFormat(100500);
    buf->setStride(7800);
    m_serverRemoteAccess->sendBufferReady(m_serverOutput[0], buf);

    // wait for event loop
    QVERIFY(bufferReadySpy1.wait());

    // receive buffer at client 1
    QCOMPARE(bufferReadySpy1.size(), 1);
    auto rbuf1 = bufferReadySpy1.takeFirst()[1].value<Clt::RemoteBuffer*>();
    QSignalSpy paramsObtainedSpy1(rbuf1, &Clt::RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy1.isValid());

    if (bufferReadySpy2.size() == 0) {
        QVERIFY(bufferReadySpy2.wait());
    }
    // receive buffer at client 2
    QCOMPARE(bufferReadySpy2.size(), 1);
    auto rbuf2 = bufferReadySpy2.takeFirst()[1].value<Clt::RemoteBuffer *>();
    QSignalSpy paramsObtainedSpy2(rbuf2, &Clt::RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy2.isValid());

    // wait for event loop
    QVERIFY(paramsObtainedSpy1.size() == 1 || paramsObtainedSpy1.wait());
    QCOMPARE(paramsObtainedSpy1.size(), 1);

    // TODO: This signal spy connection is still too racy and fails too often under high load on CI.
    //       For now disable the check and try to find a solution as part of issue #12.
#if 0
    QVERIFY(paramsObtainedSpy2.size() == 1 || paramsObtainedSpy2.wait());
    QCOMPARE(paramsObtainedSpy2.size(), 1);
#endif

    // release
    QSignalSpy bufferReleasedSpy(m_serverRemoteAccess, &Srv::RemoteAccessManager::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
    delete rbuf1;
    QVERIFY(!bufferReleasedSpy.wait(1000)); // one client released, second still holds buffer!

    delete rbuf2;
    QVERIFY(bufferReleasedSpy.wait()); // all clients released, buffer should be freed

    // cleanup
    delete buf;
    delete client1;
    delete client2;
    m_display->dispatchEvents();
    QVERIFY(!m_serverRemoteAccess->bound());
}

void RemoteAccessTest::testSendReleaseCrossScreen()
{
    // this test verifies that multiple buffers for multiple screens are sent to
    // multiple clients and returned back

    // setup
    QVERIFY(!m_serverRemoteAccess->bound());
    auto *client1 = new MockupClient(this);
    auto *client2 = new MockupClient(this);
    client1->bindOutput(1);
    client2->bindOutput(0);
    m_display->dispatchEvents();
    QVERIFY(m_serverRemoteAccess->bound()); // now we have 2 clients

    QSignalSpy bufferReadySpy1(client1->remoteAccess, &Clt::RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy1.isValid());
    QSignalSpy bufferReadySpy2(client2->remoteAccess, &Clt::RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy2.isValid());

    auto buf1 = new Srv::RemoteBufferHandle();
    QTemporaryFile *tmpFile1 = new QTemporaryFile(this);
    tmpFile1->open();

    auto buf2 = new Srv::RemoteBufferHandle();
    QTemporaryFile *tmpFile2 = new QTemporaryFile(this);
    tmpFile2->open();

    buf1->setFd(tmpFile1->handle());
    buf1->setSize(50, 50);
    buf1->setFormat(100500);
    buf1->setStride(7800);

    buf2->setFd(tmpFile2->handle());
    buf2->setSize(100, 100);
    buf2->setFormat(100500);
    buf2->setStride(7800);

    m_serverRemoteAccess->sendBufferReady(m_serverOutput[0], buf1);
    m_serverRemoteAccess->sendBufferReady(m_serverOutput[1], buf2);

    // wait for event loop
    QVERIFY(bufferReadySpy1.wait());
    if (bufferReadySpy2.size() == 0) {
        QVERIFY(bufferReadySpy2.wait());
    }

    // receive buffer at client 1
    QCOMPARE(bufferReadySpy1.size(), 1);
    auto rbuf1 = bufferReadySpy1.takeFirst()[1].value<Clt::RemoteBuffer *>();
    QSignalSpy paramsObtainedSpy1(rbuf1, &Clt::RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy1.isValid());

    // receive buffer at client 2
    QCOMPARE(bufferReadySpy2.size(), 1);
    auto rbuf2 = bufferReadySpy2.takeFirst()[1].value<Clt::RemoteBuffer *>();
    QSignalSpy paramsObtainedSpy2(rbuf2, &Clt::RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy2.isValid());

    // Wait for event loop.
    // Note: The parametersObtained signal might have been received before that (and by this
    //       the fd and so on already set. While clients can check this easily this still can
    //       be seen as an inconsistency in teh API.
    if (paramsObtainedSpy1.isEmpty() && rbuf1->fd() == 0) {
        QVERIFY(paramsObtainedSpy1.wait());
        QCOMPARE(paramsObtainedSpy1.size(), 1);
    }
    if (paramsObtainedSpy2.isEmpty() && rbuf2->fd() == 0) {
        QVERIFY(paramsObtainedSpy2.wait());
        QCOMPARE(paramsObtainedSpy2.size(), 1);
    }

    // release
    QSignalSpy bufferReleasedSpy(m_serverRemoteAccess, &Srv::RemoteAccessManager::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
    delete rbuf1;
    QVERIFY(bufferReleasedSpy.wait());

    delete rbuf2;
    QVERIFY(bufferReleasedSpy.wait());

    QCOMPARE(bufferReleasedSpy.size(), 2);

    // cleanup
    delete buf1;
    delete buf2;
    delete client1;
    delete client2;
    m_display->dispatchEvents();
    QVERIFY(!m_serverRemoteAccess->bound());
}

void RemoteAccessTest::testSendClientGone()
{
    // this test verifies that when buffer is sent and client is gone, server will release buffer
    // correctly
    QVERIFY(!m_serverRemoteAccess->bound());
    auto *client = new MockupClient(this);
    client->bindOutput(0);
    m_display->dispatchEvents();

    QVERIFY(m_serverRemoteAccess->bound()); // we have one client now
    QSignalSpy bufferReadySpy(client->remoteAccess, &Clt::RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy.isValid());

    auto buf = new Srv::RemoteBufferHandle();
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->open();

    buf->setFd(tmpFile->handle());
    buf->setSize(50, 50);
    buf->setFormat(100500);
    buf->setStride(7800);
    m_serverRemoteAccess->sendBufferReady(m_serverOutput[0], buf);

    // release forcefully
    QSignalSpy bufferReleasedSpy(m_serverRemoteAccess, &Srv::RemoteAccessManager::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());

    delete client;
    QVERIFY(bufferReleasedSpy.wait());

    // cleanup
    delete buf;
    m_display->dispatchEvents();
    QVERIFY(!m_serverRemoteAccess->bound());
}

void RemoteAccessTest::testSendReceiveClientGone()
{
    // this test verifies that when buffer is sent, received and client is gone,
    // both client and server will release buffer correctly
    QVERIFY(!m_serverRemoteAccess->bound());
    auto *client = new MockupClient(this);
    client->bindOutput(0);
    m_display->dispatchEvents();

    QVERIFY(m_serverRemoteAccess->bound()); // we have one client now
    QSignalSpy bufferReadySpy(client->remoteAccess, &Clt::RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy.isValid());

    auto buf = new Srv::RemoteBufferHandle();
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->open();

    buf->setFd(tmpFile->handle());
    buf->setSize(50, 50);
    buf->setFormat(100500);
    buf->setStride(7800);
    m_serverRemoteAccess->sendBufferReady(m_serverOutput[0], buf);

    // receive buffer
    QVERIFY(bufferReadySpy.wait());
    auto rbuf = bufferReadySpy.takeFirst()[1].value<Clt::RemoteBuffer *>();
    QSignalSpy paramsObtainedSpy(rbuf, &Clt::RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy.isValid());

    // wait for params
    QVERIFY(paramsObtainedSpy.wait());
    // client fd is different, not subject to check
    QCOMPARE(rbuf->width(), 50u);
    QCOMPARE(rbuf->height(), 50u);
    QCOMPARE(rbuf->format(), 100500u);
    QCOMPARE(rbuf->stride(), 7800u);

    // release forcefully
    QSignalSpy bufferReleasedSpy(m_serverRemoteAccess, &Srv::RemoteAccessManager::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
    delete client;
    QVERIFY(bufferReleasedSpy.wait());

    // cleanup
    delete buf;
    m_display->dispatchEvents();
    QVERIFY(!m_serverRemoteAccess->bound());
}

QTEST_GUILESS_MAIN(RemoteAccessTest)
#include "remote_access.moc"
