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
// Qt
#include <QtTest>
#include <QImage>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/surface.h"
#include "../../src/client/registry.h"
#include "../../src/client/shm_pool.h"

#include "../../server/buffer.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/surface.h"

class TestShmPool : public QObject
{
    Q_OBJECT
public:
    explicit TestShmPool(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateBufferNullImage();
    void testCreateBufferNullSize();
    void testCreateBufferInvalidSize();
    void testCreateBufferFromImage();
    void testCreateBufferFromImageWithAlpha();
    void testCreateBufferFromData();
    void testReuseBuffer();
    void testDestroy();

private:
    Wrapland::Server::D_isplay *m_display;
    Wrapland::Server::Compositor *m_serverCompositor;
    Wrapland::Client::ConnectionThread *m_connection;
    Wrapland::Client::Compositor *m_compositor;
    Wrapland::Client::ShmPool *m_shmPool;
    Wrapland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-surface-0");

TestShmPool::TestShmPool(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverCompositor(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_shmPool(nullptr)
    , m_thread(nullptr)
{
}

void TestShmPool::init()
{
    m_display = new Wrapland::Server::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Wrapland::Client::EventQueue(this);
    m_queue->setup(m_connection);

    Wrapland::Client::Registry registry;
    registry.setEventQueue(m_queue);

    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    // here we need a shm pool
    m_display->createShm();

    QVERIFY(shmSpy.wait());
    m_shmPool = registry.createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
}

void TestShmPool::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_shmPool) {
        delete m_shmPool;
        m_shmPool = nullptr;
    }
    delete m_queue;
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

void TestShmPool::testCreateBufferNullImage()
{
    QVERIFY(m_shmPool->isValid());
    QImage img;
    QVERIFY(img.isNull());
    QVERIFY(!m_shmPool->createBuffer(img).lock());
}

void TestShmPool::testCreateBufferNullSize()
{
    QVERIFY(m_shmPool->isValid());
    QSize size(0, 0);
    QVERIFY(size.isNull());
    QVERIFY(!m_shmPool->createBuffer(size, 0, nullptr).lock());
}

void TestShmPool::testCreateBufferInvalidSize()
{
    QVERIFY(m_shmPool->isValid());
    QSize size;
    QVERIFY(!size.isValid());
    QVERIFY(!m_shmPool->createBuffer(size, 0, nullptr).lock());
}

void TestShmPool::testCreateBufferFromImage()
{
    QVERIFY(m_shmPool->isValid());
    QImage img(24, 24, QImage::Format_RGB32);
    img.fill(Qt::black);
    QVERIFY(!img.isNull());
    auto buffer = m_shmPool->createBuffer(img).lock();
    QVERIFY(buffer);
    QCOMPARE(buffer->size(), img.size());
    QImage img2(buffer->address(), img.width(), img.height(), QImage::Format_RGB32);
    QCOMPARE(img2, img);
}

void TestShmPool::testCreateBufferFromImageWithAlpha()
{
    QVERIFY(m_shmPool->isValid());
    QImage img(24, 24, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(255,0,0,100)); //red with alpha
    QVERIFY(!img.isNull());
    auto buffer = m_shmPool->createBuffer(img).lock();
    QVERIFY(buffer);
    QCOMPARE(buffer->size(), img.size());
    QImage img2(buffer->address(), img.width(), img.height(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(img2, img);
}

void TestShmPool::testCreateBufferFromData()
{
    QVERIFY(m_shmPool->isValid());
    QImage img(24, 24, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QVERIFY(!img.isNull());
    auto buffer = m_shmPool->createBuffer(img.size(), img.bytesPerLine(), img.constBits()).lock();
    QVERIFY(buffer);
    QCOMPARE(buffer->size(), img.size());
    QImage img2(buffer->address(), img.width(), img.height(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(img2, img);
}

void TestShmPool::testReuseBuffer()
{
    QVERIFY(m_shmPool->isValid());
    QImage img(24, 24, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QVERIFY(!img.isNull());
    auto buffer = m_shmPool->createBuffer(img).lock();
    QVERIFY(buffer);
    buffer->setReleased(true);
    buffer->setUsed(false);

    // same image should get the same buffer
    auto buffer2 = m_shmPool->createBuffer(img).lock();
    QCOMPARE(buffer, buffer2);
    buffer2->setReleased(true);
    buffer2->setUsed(false);

    // image with different size should get us a new buffer
    auto buffer3 = m_shmPool->getBuffer(QSize(10, 10), 8).lock();
    QVERIFY(buffer3 != buffer2);

    // image with a different format should get us a new buffer
    QImage img2(24, 24, QImage::Format_RGB32);
    img2.fill(Qt::black);
    QVERIFY(!img2.isNull());
    auto buffer4 = m_shmPool->createBuffer(img2).lock();
    QVERIFY(buffer4);
    QVERIFY(buffer4 != buffer2);
    QVERIFY(buffer4 != buffer3);
}

void TestShmPool::testDestroy()
{
    using namespace Wrapland::Client;
    connect(m_connection, &ConnectionThread::establishedChanged, m_shmPool, &ShmPool::release);
    QVERIFY(m_shmPool->isValid());

    // let's create one Buffer
    m_shmPool->getBuffer(QSize(10, 10), 8);

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!m_connection->established());

    // Now the pool should be destroyed.
    QTRY_VERIFY(!m_shmPool->isValid());

    // Calling release again should not fail.
    m_shmPool->release();
}

QTEST_GUILESS_MAIN(TestShmPool)
#include "shm_pool.moc"
