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
#include <QImage>
#include <QPainter>
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/idleinhibit.h"
#include "../../src/client/output.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/surface.h"

#include "../../server/buffer.h"
#include "../../server/client.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/idle_inhibit_v1.h"
#include "../../server/surface.h"

#include "../../tests/globals.h"
#include "../../tests/helpers.h"

#include <wayland-client-protocol.h>

class TestSurface : public QObject
{
    Q_OBJECT
public:
    explicit TestSurface(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testStaticAccessor();
    void testDamage();
    void testFrameCallback();
    void testAttachBuffer();
    void testMultipleSurfaces();
    void testOpaque();
    void testInput();
    void testScale();
    void testDestroy();
    void testUnmapOfNotMappedSurface();
    void testDamageTracking();
    void testSurfaceAt();
    void testDestroyAttachedBuffer();
    void testDestroyWithPendingCallback();
    void testDisconnect();
    void testOutput();
    void testInhibit();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::ShmPool* m_shm;
    Wrapland::Client::EventQueue* m_queue;
    Wrapland::Client::IdleInhibitManager* m_idleInhibitManager;
    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-wayland-surface-0"};

TestSurface::TestSurface(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_thread(nullptr)
{
}

void TestSurface::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.globals.output_manager
        = std::make_unique<Wrapland::Server::output_manager>(*server.display);
    server.display->createShm();
    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());
    server.globals.idle_inhibit_manager_v1
        = std::make_unique<Wrapland::Server::IdleInhibitManagerV1>(server.display.get());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    /*connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock,
    m_connection, [this]() { if (m_connection->display()) {
                wl_display_flush(m_connection->display());
            }
        }
    );*/

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Wrapland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Wrapland::Client::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32, quint32)));
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32, quint32)));
    QSignalSpy allAnnounced(&registry, SIGNAL(interfacesAnnounced()));
    QVERIFY(allAnnounced.isValid());
    QVERIFY(shmSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QVERIFY(!compositorSpy.isEmpty());
    QVERIFY(!shmSpy.isEmpty());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(
        shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());

    m_idleInhibitManager = registry.createIdleInhibitManager(
        registry.interface(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1)
            .name,
        registry.interface(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1)
            .version,
        this);
    QVERIFY(m_idleInhibitManager->isValid());
}

void TestSurface::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_idleInhibitManager) {
        delete m_idleInhibitManager;
        m_idleInhibitManager = nullptr;
    }
    if (m_shm) {
        delete m_shm;
        m_shm = nullptr;
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

    server = {};
}

void TestSurface::testStaticAccessor()
{
    // TODO: Does this test still make sense with the remodel? If yes, needs porting.
#if 0
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    QVERIFY(!Wrapland::Server::Surface::get(nullptr));
    QVERIFY(!Wrapland::Server::Surface::get(1, nullptr));
    QVERIFY(Wrapland::Client::Surface::all().isEmpty());
    auto s1 = m_compositor->createSurface();
    QVERIFY(s1->isValid());
    QCOMPARE(Wrapland::Client::Surface::all().count(), 1);
    QCOMPARE(Wrapland::Client::Surface::all().first(), s1);
    QCOMPARE(Wrapland::Client::Surface::get(*s1), s1);
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface1 = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface1);
    QCOMPARE(Wrapland::Server::Surface::get(serverSurface1->resource()), serverSurface1);
    QCOMPARE(Wrapland::Server::Surface::get(serverSurface1->id(), serverSurface1->client()),
             serverSurface1);

    QVERIFY(!s1->size().isValid());
    QSignalSpy sizeChangedSpy(s1, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeChangedSpy.isValid());
    const QSize testSize(200, 300);
    s1->setSize(testSize);
    QCOMPARE(s1->size(), testSize);
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(sizeChangedSpy.first().first().toSize(), testSize);

    // add another surface
    auto s2 = m_compositor->createSurface();
    QVERIFY(s2->isValid());
    QCOMPARE(Wrapland::Client::Surface::all().count(), 2);
    QCOMPARE(Wrapland::Client::Surface::all().first(), s1);
    QCOMPARE(Wrapland::Client::Surface::all().last(), s2);
    QCOMPARE(Wrapland::Client::Surface::get(*s1), s1);
    QCOMPARE(Wrapland::Client::Surface::get(*s2), s2);
    serverSurfaceCreated.clear();
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface2 = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface2);
    QCOMPARE(Wrapland::Server::Surface::get(serverSurface1->resource()), serverSurface1);
    QCOMPARE(Wrapland::Server::Surface::get(serverSurface1->id(), serverSurface1->client()),
             serverSurface1);
    QCOMPARE(Wrapland::Server::Surface::get(serverSurface2->resource()), serverSurface2);
    QCOMPARE(Wrapland::Server::Surface::get(serverSurface2->id(), serverSurface2->client()),
             serverSurface2);

    // delete s2 again
    delete s2;
    QCOMPARE(Wrapland::Client::Surface::all().count(), 1);
    QCOMPARE(Wrapland::Client::Surface::all().first(), s1);
    QCOMPARE(Wrapland::Client::Surface::get(*s1), s1);

    // and finally delete the last one
    delete s1;
    QVERIFY(Wrapland::Client::Surface::all().isEmpty());
    QVERIFY(!Wrapland::Client::Surface::get(nullptr));
    QSignalSpy unboundSpy(serverSurface1, &Wrapland::Server::Resource::unbound);
    QVERIFY(unboundSpy.isValid());
    QVERIFY(unboundSpy.wait());
    QVERIFY(!Wrapland::Server::Surface::get(nullptr));
    QVERIFY(!Wrapland::Server::Surface::get(1, nullptr));
#endif
}

void TestSurface::testDamage()
{
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s{m_compositor->createSurface()};
    s->setScale(2);
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->state().damage, QRegion());
    QVERIFY(!serverSurface->isMapped());

    QSignalSpy committedSpy(serverSurface, SIGNAL(committed()));

    // send damage without a buffer
    s->damage(QRect(0, 0, 100, 100));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(!serverSurface->isMapped());
    QCOMPARE(committedSpy.count(), 1);
    QVERIFY(serverSurface->state().damage.isEmpty());

    QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto b = m_shm->createBuffer(img);
    s->attachBuffer(b, QPoint(55, 55));
    s->damage(QRect(0, 0, 10, 10));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(serverSurface->state().offset,
             QPoint(55, 55)); // offset is surface local so scale doesn't change this
    QCOMPARE(serverSurface->state().damage, QRegion(0, 0, 5, 5)); // scale is 2
    QVERIFY(serverSurface->isMapped());
    QCOMPARE(committedSpy.count(), 2);

    // damage multiple times
    QRegion testRegion(5, 8, 3, 6);
    testRegion = testRegion.united(QRect(10, 11, 6, 1));
    img = QImage(QSize(40, 35), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damage(testRegion);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(serverSurface->state().damage, testRegion);
    QVERIFY(serverSurface->isMapped());
    QCOMPARE(committedSpy.count(), 3);

    // damage buffer
    const QRegion testRegion2(30, 40, 22, 4);
    const QRegion cmpRegion2(15, 20, 11, 2); // divided by scale factor
    img = QImage(QSize(80, 70), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damageBuffer(testRegion2);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(serverSurface->state().damage, cmpRegion2);
    QVERIFY(serverSurface->isMapped());

    // combined regular damage and damaged buffer
    const QRegion testRegion3 = testRegion.united(cmpRegion2);
    img = QImage(QSize(80, 70), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damage(testRegion);
    s->damageBuffer(testRegion2);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QVERIFY(serverSurface->state().damage != testRegion);
    QVERIFY(serverSurface->state().damage != testRegion2);
    QVERIFY(serverSurface->state().damage != cmpRegion2);
    QCOMPARE(serverSurface->state().damage, testRegion3);
    QVERIFY(serverSurface->isMapped());
}

void TestSurface::testFrameCallback()
{
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s{m_compositor->createSurface()};
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());

    QSignalSpy frameRenderedSpy(s.get(), SIGNAL(frameRendered()));
    QVERIFY(frameRenderedSpy.isValid());
    QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damage(QRect(0, 0, 10, 10));
    s->commit();
    QVERIFY(commit_spy.wait());
    serverSurface->frameRendered(10);
    QVERIFY(frameRenderedSpy.isEmpty());
    QVERIFY(frameRenderedSpy.wait());
    QVERIFY(!frameRenderedSpy.isEmpty());
}

void TestSurface::testAttachBuffer()
{
    // create the surface
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s{m_compositor->createSurface()};
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // create three images
    QImage black(24, 24, QImage::Format_RGB32);
    black.fill(Qt::black);
    QImage red(24, 24, QImage::Format_ARGB32); // Note - deliberately not premultiplied
    red.fill(QColor(255, 0, 0, 128));
    QImage blue(24, 24, QImage::Format_ARGB32_Premultiplied);
    blue.fill(QColor(0, 0, 255, 128));

    std::shared_ptr<Wrapland::Client::Buffer> blackBufferPtr = m_shm->createBuffer(black).lock();
    QVERIFY(blackBufferPtr);
    wl_buffer* blackBuffer = *(blackBufferPtr.get());
    std::shared_ptr<Wrapland::Client::Buffer> redBuffer = m_shm->createBuffer(red).lock();
    QVERIFY(redBuffer);
    std::shared_ptr<Wrapland::Client::Buffer> blueBuffer = m_shm->createBuffer(blue).lock();
    QVERIFY(blueBuffer);

    QCOMPARE(blueBuffer->format(), Wrapland::Client::Buffer::Format::ARGB32);
    QCOMPARE(blueBuffer->size(), blue.size());
    QVERIFY(!blueBuffer->isReleased());
    QVERIFY(!blueBuffer->isUsed());
    QCOMPARE(blueBuffer->stride(), blue.bytesPerLine());

    s->attachBuffer(redBuffer);
    s->attachBuffer(blackBuffer);
    s->damage(QRect(0, 0, 24, 24));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->isMapped());

    // now the ServerSurface should have the black image attached as a buffer
    auto buffer1 = serverSurface->state().buffer;
    QVERIFY(buffer1->shmBuffer());
    QCOMPARE(buffer1->shmImage()->createQImage(), black);
    QCOMPARE(buffer1->shmImage()->format(), Wrapland::Server::ShmImage::Format::xrgb8888);
    QCOMPARE(buffer1->shmImage()->createQImage().format(), QImage::Format_RGB32);
    buffer1.reset();

    // render another frame
    s->attachBuffer(redBuffer);
    s->damage(QRect(0, 0, 24, 24));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(!redBuffer->isReleased());
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->isMapped());

    auto buffer2 = serverSurface->state().buffer;
    QVERIFY(buffer2->shmBuffer());
    QCOMPARE(buffer2->shmImage()->createQImage().format(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(buffer2->shmImage()->createQImage().width(), 24);
    QCOMPARE(buffer2->shmImage()->createQImage().height(), 24);
    for (int i = 0; i < 24; ++i) {
        for (int j = 0; j < 24; ++j) {
            // it's premultiplied in the format
            QCOMPARE(buffer2->shmImage()->createQImage().pixel(i, j), qRgba(128, 0, 0, 128));
        }
    }
    QVERIFY(!redBuffer->isReleased());
    buffer2.reset();
    QVERIFY(!redBuffer->isReleased());

    // render another frame
    blueBuffer->setUsed(true);
    QVERIFY(blueBuffer->isUsed());
    s->attachBuffer(blueBuffer.get());
    s->damage(QRect(0, 0, 24, 24));
    QSignalSpy frameRenderedSpy(s.get(), SIGNAL(frameRendered()));
    QVERIFY(frameRenderedSpy.isValid());
    s->commit();
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->isMapped());
    buffer2.reset();

    // TODO: we should have a signal on when the Buffer gets released
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    if (!redBuffer->isReleased()) {
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    }
    QVERIFY(redBuffer->isReleased());

    auto buffer3 = serverSurface->state().buffer.get();
    QVERIFY(buffer3->shmBuffer());
    QCOMPARE(buffer3->shmImage()->createQImage().format(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(buffer3->shmImage()->createQImage().width(), 24);
    QCOMPARE(buffer3->shmImage()->createQImage().height(), 24);
    for (int i = 0; i < 24; ++i) {
        for (int j = 0; j < 24; ++j) {
            // it's premultiplied in the format
            QCOMPARE(buffer3->shmImage()->createQImage().pixel(i, j), qRgba(0, 0, 128, 128));
        }
    }

    serverSurface->frameRendered(1);
    QVERIFY(frameRenderedSpy.wait());

    // commit a different value shouldn't change our buffer
    QCOMPARE(serverSurface->state().buffer.get(), buffer3);
    QVERIFY(serverSurface->state().input.isNull());
    s->setInputRegion(m_compositor->createRegion(QRegion(0, 0, 24, 24)).get());
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(serverSurface->state().input, QRegion(0, 0, 24, 24));
    QCOMPARE(serverSurface->state().buffer.get(), buffer3);
    QVERIFY(serverSurface->state().damage.isEmpty());
    QVERIFY(serverSurface->isMapped());

    // clear the surface
    s->attachBuffer(blackBuffer);
    s->damage(QRect(0, 0, 1, 1));
    // TODO: better method
    s->attachBuffer((wl_buffer*)nullptr);
    s->damage(QRect(0, 0, 10, 10));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().damage.isEmpty());
    QVERIFY(!serverSurface->isMapped());
}

void TestSurface::testMultipleSurfaces()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32, quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(shmSpy.wait());

    ShmPool pool1;
    ShmPool pool2;
    pool1.setup(registry.bindShm(shmSpy.first().first().value<quint32>(),
                                 shmSpy.first().last().value<quint32>()));
    pool2.setup(registry.bindShm(shmSpy.first().first().value<quint32>(),
                                 shmSpy.first().last().value<quint32>()));
    QVERIFY(pool1.isValid());
    QVERIFY(pool2.isValid());

    // create the surfaces
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s1(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface1 = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface1);
    // second surface
    std::unique_ptr<Wrapland::Client::Surface> s2(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface2 = serverSurfaceCreated.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface2);
    //    QVERIFY(serverSurface1->resource() != serverSurface2->resource());

    // create two images
    QImage black(24, 24, QImage::Format_RGB32);
    black.fill(Qt::black);
    QImage red(24, 24, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 128));

    auto blackBuffer = pool1.createBuffer(black);
    auto redBuffer = pool2.createBuffer(red);

    s1->attachBuffer(blackBuffer);
    s1->damage(QRect(0, 0, 24, 24));
    s1->commit(Wrapland::Client::Surface::CommitFlag::None);
    QSignalSpy commit_spy1(serverSurface1, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy1.isValid());
    QVERIFY(commit_spy1.wait());

    // now the ServerSurface should have the black image attached as a buffer
    auto buffer1 = serverSurface1->state().buffer;
    QVERIFY(buffer1);
    QImage buffer1Data = buffer1->shmImage()->createQImage();
    QCOMPARE(buffer1Data, black);
    // accessing the same buffer is OK
    QImage buffer1Data2 = buffer1->shmImage()->createQImage();
    QCOMPARE(buffer1Data2, buffer1Data);
    buffer1Data = QImage();
    QVERIFY(buffer1Data.isNull());
    buffer1Data2 = QImage();
    QVERIFY(buffer1Data2.isNull());

    // attach a buffer for the other surface
    s2->attachBuffer(redBuffer);
    s2->damage(QRect(0, 0, 24, 24));
    s2->commit(Wrapland::Client::Surface::CommitFlag::None);
    QSignalSpy commit_spy2(serverSurface2, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy2.isValid());
    QVERIFY(commit_spy2.wait());

    auto buffer2 = serverSurface2->state().buffer;
    QVERIFY(buffer2);
    QImage buffer2Data = buffer2->shmImage()->createQImage();
    QCOMPARE(buffer2Data, red);

    // while buffer2 is accessed we cannot access buffer1
    auto buffer1ShmImage = buffer1->shmImage();
    QVERIFY(!buffer1ShmImage);

    // a deep copy can be kept around
    QImage deepCopy = buffer2Data.copy();
    QCOMPARE(deepCopy, red);
    buffer2Data = QImage();
    QVERIFY(buffer2Data.isNull());
    QCOMPARE(deepCopy, red);

    // now that buffer2Data is destroyed we can access buffer1 again
    buffer1ShmImage = buffer1->shmImage();
    QVERIFY(buffer1ShmImage);
    buffer1Data = buffer1ShmImage->createQImage();
    QVERIFY(!buffer1Data.isNull());
    QCOMPARE(buffer1Data, black);
}

void TestSurface::testOpaque()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s{m_compositor->createSurface()};
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());

    // by default there should be an empty opaque region
    QCOMPARE(serverSurface->state().opaque, QRegion());

    // let's install an opaque region
    s->setOpaqueRegion(m_compositor->createRegion(QRegion(0, 10, 20, 30)).get());
    // the region should only be applied after the surface got committed
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSurface->state().opaque, QRegion());
    QCOMPARE(commit_spy.count(), 0);

    // so let's commit to get the new region
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 1);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::opaque);
    QCOMPARE(serverSurface->state().opaque, QRegion(0, 10, 20, 30));

    // committing without setting a new region shouldn't change
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(commit_spy.count(), 2);
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::opaque, false);
    QCOMPARE(serverSurface->state().opaque, QRegion(0, 10, 20, 30));

    // let's change the opaque region
    s->setOpaqueRegion(m_compositor->createRegion(QRegion(10, 20, 30, 40)).get());
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 3);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::opaque);
    QCOMPARE(serverSurface->state().opaque, QRegion(10, 20, 30, 40));

    // and let's go back to an empty region
    s->setOpaqueRegion();
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 4);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::opaque);
    QCOMPARE(serverSurface->state().opaque, QRegion());
}

void TestSurface::testInput()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s{m_compositor->createSurface()};
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());

    // by default there should be an empty == infinite input region
    QCOMPARE(serverSurface->state().input, QRegion());
    QCOMPARE(serverSurface->state().input_is_infinite, true);

    // let's install an input region
    s->setInputRegion(m_compositor->createRegion(QRegion(0, 10, 20, 30)).get());
    // the region should only be applied after the surface got committed
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSurface->state().input, QRegion());
    QCOMPARE(serverSurface->state().input_is_infinite, true);
    QCOMPARE(commit_spy.count(), 0);

    // so let's commit to get the new region
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 1);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::input);
    QCOMPARE(serverSurface->state().input, QRegion(0, 10, 20, 30));
    QCOMPARE(serverSurface->state().input_is_infinite, false);

    // committing without setting a new region shouldn't change
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(commit_spy.count(), 2);
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::input, false);
    QCOMPARE(serverSurface->state().input, QRegion(0, 10, 20, 30));
    QCOMPARE(serverSurface->state().input_is_infinite, false);

    // let's change the input region
    s->setInputRegion(m_compositor->createRegion(QRegion(10, 20, 30, 40)).get());
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 3);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::input);
    QCOMPARE(serverSurface->state().input, QRegion(10, 20, 30, 40));
    QCOMPARE(serverSurface->state().input_is_infinite, false);

    // and let's go back to an empty region
    s->setInputRegion();
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 4);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::input);
    QCOMPARE(serverSurface->state().input, QRegion());
    QCOMPARE(serverSurface->state().input_is_infinite, true);
}

void TestSurface::testScale()
{
    // This test verifies that updating the scale factor is correctly passed to the Wayland server.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QCOMPARE(s->scale(), 1);
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->state().scale, 1);

    // let's change the scale factor
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);

    QVERIFY(commit_spy.isValid());
    s->setScale(2);
    QCOMPARE(s->scale(), 2);
    // needs a commit
    QVERIFY(!commit_spy.wait(100));
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::scale, false);

    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 1);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::scale);
    QCOMPARE(serverSurface->state().scale, 2);

    // even though we've changed the scale, if we don't have a buffer we
    // don't have a size. If we don't have a size it can't have changed
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::size, false);
    QVERIFY(!serverSurface->size().isValid());

    // let's try changing to same factor, should not emit changed on server
    s->setScale(2);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QEXPECT_FAIL("", "Scale update set also when factor stays same. Change behavior?", Continue);
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::scale, false);

    // but changing to a different value should still work
    s->setScale(4);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 3);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::scale);
    QCOMPARE(serverSurface->state().scale, 4);
    commit_spy.clear();

    // attach a buffer of 100x100, our scale is 4, so this should be a size of 25x25
    QImage red(100, 100, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 128));
    std::shared_ptr<Wrapland::Client::Buffer> redBuffer = m_shm->createBuffer(red).lock();
    QVERIFY(redBuffer);
    s->attachBuffer(redBuffer.get());
    s->damage(QRect(0, 0, 25, 25));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 1);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::scale, false);
    QCOMPARE(serverSurface->size(), QSize(25, 25));
    commit_spy.clear();

    // set the scale to 1, buffer is still 100x100 so size should change to 100x100
    s->setScale(1);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 1);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::scale);
    QCOMPARE(serverSurface->state().scale, 1);
    QCOMPARE(serverSurface->size(), QSize(100, 100));
    commit_spy.clear();

    // set scale and size in one commit, buffer is 50x50 at scale 2 so size should be 25x25
    QImage blue(50, 50, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 128));
    std::shared_ptr<Wrapland::Client::Buffer> blueBuffer = m_shm->createBuffer(blue).lock();
    QVERIFY(blueBuffer);
    s->attachBuffer(blueBuffer.get());
    s->setScale(2);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 1);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::scale);
    QCOMPARE(serverSurface->state().scale, 2);
    QCOMPARE(serverSurface->size(), QSize(25, 25));
}

void TestSurface::testDestroy()
{
    using namespace Wrapland::Client;
    std::unique_ptr<Wrapland::Client::Surface> s{m_compositor->createSurface()};

    connect(m_connection, &ConnectionThread::establishedChanged, s.get(), &Surface::release);
    connect(
        m_connection, &ConnectionThread::establishedChanged, m_compositor, &Compositor::release);
    connect(m_connection, &ConnectionThread::establishedChanged, m_shm, &ShmPool::release);
    connect(m_connection, &ConnectionThread::establishedChanged, m_queue, &EventQueue::release);
    connect(m_connection,
            &ConnectionThread::establishedChanged,
            m_idleInhibitManager,
            &IdleInhibitManager::release);
    QVERIFY(s->isValid());

    server = {};
    QTRY_VERIFY(!m_connection->established());

    // Now the Surface should be destroyed.
    QTRY_VERIFY(!s->isValid());

    // Calling destroy again should not fail.
    s->release();
}

void TestSurface::testUnmapOfNotMappedSurface()
{
    // this test verifies that a surface which doesn't have a buffer attached doesn't trigger the
    // unmapped signal

    // create surface
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();

    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);

    // let's map a null buffer and change scale to trigger a signal we can wait for
    s->attachBuffer(Wrapland::Client::Buffer::Ptr());
    s->setScale(2);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::scale);
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::mapped, false);
}

void TestSurface::testDamageTracking()
{
    // this tests the damage tracking feature
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    // create surface
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();

    // before first commit, the tracked damage should be empty
    QVERIFY(serverSurface->trackedDamage().isEmpty());

    // Now let's damage the surface
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 100, 100));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(serverSurface->trackedDamage(), QRegion(0, 0, 100, 100));
    QCOMPARE(serverSurface->state().damage, QRegion(0, 0, 100, 100));

    // resetting the tracked damage should empty it
    serverSurface->resetTrackedDamage();
    QVERIFY(serverSurface->trackedDamage().isEmpty());
    // but not affect the actual damage
    QCOMPARE(serverSurface->state().damage, QRegion(0, 0, 100, 100));

    // let's damage some parts of the surface
    QPainter p;
    p.begin(&image);
    p.fillRect(QRect(0, 0, 10, 10), Qt::blue);
    p.end();
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 10, 10));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(serverSurface->trackedDamage(), QRegion(0, 0, 10, 10));
    QCOMPARE(serverSurface->state().damage, QRegion(0, 0, 10, 10));

    // and damage some part completely not bounding to the current damage region
    p.begin(&image);
    p.fillRect(QRect(50, 40, 20, 30), Qt::blue);
    p.end();
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(50, 40, 20, 30));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(serverSurface->trackedDamage(), QRegion(0, 0, 10, 10).united(QRegion(50, 40, 20, 30)));
    QCOMPARE(serverSurface->trackedDamage().rectCount(), 2);
    QCOMPARE(serverSurface->state().damage, QRegion(50, 40, 20, 30));

    // now let's reset the tracked damage again
    serverSurface->resetTrackedDamage();
    QVERIFY(serverSurface->trackedDamage().isEmpty());
    // but not affect the actual damage
    QCOMPARE(serverSurface->state().damage, QRegion(50, 40, 20, 30));
}

void TestSurface::testSurfaceAt()
{
    // this test verifies that surfaceAt(const QPointF&) works as expected for the case of no
    // children
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    // create surface
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();

    // a newly created surface should not be mapped and not provide a surface at a position
    QVERIFY(!serverSurface->isMapped());
    QVERIFY(!Wrapland::Server::Test::surface_at(serverSurface, QPointF(0, 0)));

    // let's damage this surface
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 100, 100));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    // now the surface is mapped and surfaceAt should give the surface
    QVERIFY(serverSurface->isMapped());
    QCOMPARE(Wrapland::Server::Test::surface_at(serverSurface, QPointF(0, 0)), serverSurface);
    QCOMPARE(Wrapland::Server::Test::surface_at(serverSurface, QPointF(100, 100)), serverSurface);
    // outside the geometry it should not give a surface
    QVERIFY(!Wrapland::Server::Test::surface_at(serverSurface, QPointF(101, 101)));
    QVERIFY(!Wrapland::Server::Test::surface_at(serverSurface, QPointF(-1, -1)));
}

void TestSurface::testDestroyAttachedBuffer()
{
    // this test verifies that destroying of a buffer attached to a surface works

    // create surface
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();

    // let's damage this surface
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 100, 100));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().buffer);

    // attach another buffer
    image.fill(Qt::blue);
    s->attachBuffer(m_shm->createBuffer(image));
    m_connection->flush();

    // Let's try to destroy it
    auto currentBuffer = serverSurface->state().buffer;
    QSignalSpy destroySpy(currentBuffer.get(), &Wrapland::Server::Buffer::resourceDestroyed);
    QVERIFY(destroySpy.isValid());
    delete m_shm;
    m_shm = nullptr;
    QVERIFY(destroySpy.count() || destroySpy.wait());
    QVERIFY(!serverSurface->state().buffer);
}

void TestSurface::testDestroyWithPendingCallback()
{
    // this test tries to verify that destroying a surface with a pending callback works correctly
    // first create surface
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // now render to it
    QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damage(QRect(0, 0, 10, 10));
    // add some frame callbacks
    wl_callback* callbacks[1000];
    for (int i = 0; i < 1000; i++) {
        callbacks[i] = wl_surface_frame(*s);
    }
    s->commit(Wrapland::Client::Surface::CommitFlag::FrameCallback);
    QSignalSpy commit_spy(serverSurface, &Wrapland::Server::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QVERIFY(commit_spy.wait());

    // now try to destroy the Surface again
    QSignalSpy destroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    s.reset();
    QVERIFY(destroyedSpy.wait());

    for (int i = 0; i < 1000; i++) {
        wl_callback_destroy(callbacks[i]);
    }
}

void TestSurface::testDisconnect()
{
    // this test verifies that the server side correctly tears down the resources when the client
    // disconnects
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // destroy client
    QSignalSpy clientDisconnectedSpy(serverSurface->client(),
                                     &Wrapland::Server::Client::disconnected);
    QVERIFY(clientDisconnectedSpy.isValid());
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());

    s->release();
    m_shm->release();
    m_compositor->release();
    m_queue->release();
    m_idleInhibitManager->release();

    QCOMPARE(surfaceDestroyedSpy.count(), 0);

    QVERIFY(m_connection);
    m_connection->deleteLater();
    m_connection = nullptr;

    QVERIFY(clientDisconnectedSpy.wait());
    QCOMPARE(clientDisconnectedSpy.count(), 1);
    QTRY_COMPARE(surfaceDestroyedSpy.count(), 1);
}

void TestSurface::testOutput()
{
    // This test verifies that the enter/leave are sent correctly to the Client
    qRegisterMetaType<Wrapland::Client::Output*>();

    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    QVERIFY(s->outputs().isEmpty());
    QSignalSpy enteredSpy(s.get(), &Wrapland::Client::Surface::outputEntered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(s.get(), &Wrapland::Client::Surface::outputLeft);
    QVERIFY(leftSpy.isValid());
    // wait for the surface on the Server side
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->outputs(), std::vector<Wrapland::Server::WlOutput*>());

    // Create another registry to get notified about added outputs.
    Wrapland::Client::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy allAnnounced(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QSignalSpy outputAnnouncedSpy(&registry, &Wrapland::Client::Registry::outputAnnounced);
    QVERIFY(outputAnnouncedSpy.isValid());

    auto serverOutput = new Wrapland::Server::output(*server.globals.output_manager);
    serverOutput->add_mode({.size = QSize{1920, 1080}, .id = 0});
    auto state = serverOutput->get_state();
    state.enabled = true;
    serverOutput->set_state(state);
    serverOutput->done();

    QVERIFY(outputAnnouncedSpy.wait());
    std::unique_ptr<Wrapland::Client::Output> clientOutput(
        registry.createOutput(outputAnnouncedSpy.first().first().value<quint32>(),
                              outputAnnouncedSpy.first().last().value<quint32>()));
    QVERIFY(clientOutput->isValid());
    m_connection->flush();
    server.display->dispatchEvents();

    // Now enter it.
    std::vector<Wrapland::Server::WlOutput*> outputs;
    outputs.push_back(serverOutput->wayland_output());
    serverSurface->setOutputs(outputs);
    QCOMPARE(serverSurface->outputs(), outputs);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(enteredSpy.first().first().value<Wrapland::Client::Output*>(), clientOutput.get());
    QCOMPARE(s->outputs(), QVector<Wrapland::Client::Output*>{clientOutput.get()});

    // Adding to same should not trigger.
    serverSurface->setOutputs(std::vector<Wrapland::Server::output*>{serverOutput});

    // leave again
    serverSurface->setOutputs(std::vector<Wrapland::Server::output*>());
    QCOMPARE(serverSurface->outputs(), std::vector<Wrapland::Server::WlOutput*>());
    QVERIFY(leftSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(leftSpy.first().first().value<Wrapland::Client::Output*>(), clientOutput.get());
    QCOMPARE(s->outputs(), QVector<Wrapland::Client::Output*>());

    // Leave again should not trigger.
    serverSurface->setOutputs(std::vector<Wrapland::Server::output*>());

    // and enter again, just to verify
    serverSurface->setOutputs(std::vector<Wrapland::Server::output*>{serverOutput});
    QCOMPARE(serverSurface->outputs(),
             std::vector<Wrapland::Server::WlOutput*>{serverOutput->wayland_output()});
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);

    // Delete output client is on. Client should get an exit and be left on no outputs (which is
    // allowed).
    serverOutput->deleteLater();
    QVERIFY(leftSpy.wait());
    QCOMPARE(serverSurface->outputs().size(), 0);
}

void TestSurface::testInhibit()
{
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());
    // wait for the surface on the Server side
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->inhibitsIdle(), false);

    QSignalSpy inhibitsChangedSpy(serverSurface, &Wrapland::Server::Surface::inhibitsIdleChanged);
    QVERIFY(inhibitsChangedSpy.isValid());

    // now create an idle inhibition
    std::unique_ptr<Wrapland::Client::IdleInhibitor> inhibitor1(
        m_idleInhibitManager->createInhibitor(s.get()));
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), true);

    // creating a second idle inhibition should not trigger the signal
    std::unique_ptr<Wrapland::Client::IdleInhibitor> inhibitor2(
        m_idleInhibitManager->createInhibitor(s.get()));
    QVERIFY(!inhibitsChangedSpy.wait(500));
    QCOMPARE(serverSurface->inhibitsIdle(), true);

    // and also deleting the first inhibitor should not yet change the inhibition
    inhibitor1.reset();
    QVERIFY(!inhibitsChangedSpy.wait(500));
    QCOMPARE(serverSurface->inhibitsIdle(), true);

    // but deleting also the second inhibitor should trigger
    inhibitor2.reset();
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), false);
    QCOMPARE(inhibitsChangedSpy.count(), 2);

    // recreate inhibitor1 should inhibit again
    inhibitor1.reset(m_idleInhibitManager->createInhibitor(s.get()));
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), true);
    // and destroying should uninhibit
    inhibitor1.reset();
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), false);
    QCOMPARE(inhibitsChangedSpy.count(), 4);
}

QTEST_GUILESS_MAIN(TestSurface)
#include "surface.moc"
