/********************************************************************
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
#include "../../src/client/viewporter.h"
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/surface.h"

#include "../../server/buffer.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/surface.h"
#include "../../server/viewporter.h"
#include "../../server/wayland/client.h"

#include "../../tests/globals.h"

#include <QImage>
#include <QPainter>
#include <QtTest>
#include <wayland-client-protocol.h>
#include <wayland-viewporter-client-protocol.h>

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class TestViewporter : public QObject
{
    Q_OBJECT
public:
    explicit TestViewporter(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testViewportExists();
    void testWithoutBuffer();
    void testDestinationSize();
    void testSourceRectangle();
    void testDestinationSizeAndSourceRectangle();
    void testDataError_data();
    void testDataError();
    void testBufferSizeChange();
    void testDestinationSizeChange();
    void testNoSurface();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Clt::ConnectionThread* m_connection;
    Clt::Compositor* m_compositor;
    Clt::ShmPool* m_shm;
    Clt::EventQueue* m_queue;
    Clt::Viewporter* m_viewporter;
    QThread* m_thread;
};

constexpr auto socket_name{"kwin-test-viewporter-0"};

TestViewporter::TestViewporter(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_thread(nullptr)
{
}

void TestViewporter::init()
{
    qRegisterMetaType<Srv::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());
    server.globals.viewporter
        = std::make_unique<Wrapland::Server::Viewporter>(server.display.get());

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(establishedChanged(bool)));
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Clt::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Clt::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32, quint32)));
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32, quint32)));
    QSignalSpy viewporterSpy(&registry, SIGNAL(viewporterAnnounced(quint32, quint32)));
    QSignalSpy allAnnounced(&registry, SIGNAL(interfacesAnnounced()));
    QVERIFY(allAnnounced.isValid());
    QVERIFY(shmSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QVERIFY(!compositorSpy.isEmpty());
    QVERIFY(!shmSpy.isEmpty());
    QVERIFY(!viewporterSpy.isEmpty());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(
        shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());

    m_viewporter = registry.createViewporter(
        registry.interface(Clt::Registry::Interface::Viewporter).name,
        registry.interface(Clt::Registry::Interface::Viewporter).version,
        this);
    QVERIFY(m_viewporter->isValid());
}

void TestViewporter::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_viewporter) {
        delete m_viewporter;
        m_viewporter = nullptr;
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

void TestViewporter::testViewportExists()
{
    // This test verifies that setting the viewport for a surface twice results in a protocol error.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Srv::Surface* serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp1(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    // Create second viewport with error.
    QSignalSpy errorSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    std::unique_ptr<Clt::Viewport> vp2(m_viewporter->createViewport(s.get(), this));
    QVERIFY(errorSpy.wait());
}

void TestViewporter::testWithoutBuffer()
{
    // This test verifies that setting the viewport while buffer is zero does not change the size.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    QVERIFY(!serverSurface->state().buffer);

    // Change the destination size.
    QSignalSpy commit_spy(serverSurface, &Srv::Surface::committed);
    vp->setDestinationSize(QSize(400, 200));
    s->commit(Clt::Surface::CommitFlag::None);

    // Even though a new destination size is set if there is no buffer, the surface size has not
    // changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::size, false);
    QVERIFY(!serverSurface->size().isValid());

    // Now change the source rectangle.
    vp->setSourceRectangle(QRectF(QPointF(100, 50), QSizeF(400, 200)));
    s->commit(Clt::Surface::CommitFlag::None);

    // Even though a new source rectangle is set if there is no buffer, the surface size has not
    // changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::size, false);
    QVERIFY(!serverSurface->size().isValid());
}

void TestViewporter::testDestinationSize()
{
    // This test verifies setting the destination size is received by the server.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Srv::Surface* serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy commit_spy(serverSurface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);

    // Change the destination size.
    vp->setDestinationSize(QSize(400, 200));
    s->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 2);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QCOMPARE(serverSurface->size(), QSize(400, 200));

    // Destroy the viewport and check that the size is reset.
    vp.reset();
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 3);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QCOMPARE(serverSurface->size(), image.size());
}

void TestViewporter::testSourceRectangle()
{
    // This test verifies setting the source rectangle is received by the server.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Srv::Surface* serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy commit_spy(serverSurface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);

    // Change the source rectangle.
    QRectF const rect(QPointF(100, 50), QSizeF(400, 200));
    vp->setSourceRectangle(rect);
    s->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 2);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::source_rectangle);
    QCOMPARE(serverSurface->size(), rect.size());
    QCOMPARE(serverSurface->state().source_rectangle, rect);

    // Destroy the viewport and check that the size is reset.
    vp.reset();
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 3);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QCOMPARE(serverSurface->size(), image.size());
}

void TestViewporter::testDestinationSizeAndSourceRectangle()
{
    // This test verifies setting the destination size and source rectangle is received by the
    // server.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Srv::Surface* serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy commit_spy(serverSurface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);

    // Change the destination size.
    QSize const destinationSize = QSize(100, 50);
    vp->setDestinationSize(destinationSize);

    // Change the source rectangle.
    QRectF const rect(QPointF(100.1, 50.5), QSizeF(400.9, 200.8));
    vp->setSourceRectangle(rect);
    s->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 2);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::source_rectangle);
    QCOMPARE(serverSurface->size(), destinationSize);
    QVERIFY((serverSurface->state().source_rectangle.topLeft() - rect.topLeft()).manhattanLength()
            < 0.01);
    QVERIFY((serverSurface->state().source_rectangle.bottomRight() - rect.bottomRight())
                .manhattanLength()
            < 0.01);

    // Destroy the viewport and check that the size is reset.
    vp.reset();
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 3);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QCOMPARE(serverSurface->size(), image.size());
}

void TestViewporter::testDataError_data()
{
    QTest::addColumn<QSize>("destinationSize");
    QTest::addColumn<QRectF>("sourceRectangle");

    QTest::newRow("destination-size-bad-value1") << QSize(-1, 0) << QRectF(-1, -1, -1, -1);
    QTest::newRow("destination-size-bad-value2") << QSize(-1, -2) << QRectF(-1, -1, -1, -1);
    QTest::newRow("source-rectangle-bad-value1") << QSize(-1, -1) << QRectF(0, 0, 1, -1);
    QTest::newRow("source-rectangle-bad-value2") << QSize(-1, -1) << QRectF(0, 0, 1, 1.5);
    QTest::newRow("source-rectangle-out-of-buffer") << QSize(-1, -1) << QRectF(0, 0, 601, 400);
}

void TestViewporter::testDataError()
{
    // This test verifies setting the source rectangle is received by the server.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Srv::Surface* serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy commit_spy(serverSurface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);

    QFETCH(QSize, destinationSize);
    QFETCH(QRectF, sourceRectangle);

    QSignalSpy errorSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    QVERIFY(!m_connection->error());

    // Set the destination size.
    vp->setDestinationSize(destinationSize);

    // Set the source rectangle.
    vp->setSourceRectangle(sourceRectangle);
    s->commit();

    // One of these lead to an error.
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->error());
    // TODO: compare protocol error code
}

void TestViewporter::testBufferSizeChange()
{
    // This test verifies that changing the buffer size reevaluates the source rectangle contains
    // condition correctly.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Srv::Surface* serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy commit_spy(serverSurface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);

    // Change the source rectangle.
    QRectF const rect(QPointF(100, 200), QSizeF(500, 200));
    vp->setSourceRectangle(rect);
    s->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 2);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::source_rectangle);
    QCOMPARE(serverSurface->size(), rect.size());
    QCOMPARE(serverSurface->state().source_rectangle, rect);

    // Now change the buffer such that the source rectangle would not be contained anymore.
    QImage image2(QSize(599, 399), QImage::Format_ARGB32_Premultiplied);
    image2.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image2));
    s->damage(QRect(0, 0, 599, 399));

    // Change the source rectangle accordingly.
    QRectF const rect2(QPointF(100, 200), QSizeF(499, 199));
    vp->setSourceRectangle(rect2);
    s->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 3);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::source_rectangle);
    QCOMPARE(serverSurface->size(), rect2.size());
    QCOMPARE(serverSurface->state().source_rectangle, rect2);

    // Now change the buffer such that the source rectangle would not be contained anymore.
    QImage image3(QSize(598, 399), QImage::Format_ARGB32_Premultiplied);
    image3.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image3));

    // And try to commit without changing the source rectangle accordingly, what leads to an error.
    QSignalSpy errorSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    QVERIFY(!m_connection->error());

    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->error());
    // TODO: compare protocol error code
}

void TestViewporter::testDestinationSizeChange()
{
    // This test verifies that changing the destination size reevaluates the source rectangle and
    // its integer condition correctly.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Srv::Surface* serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy commit_spy(serverSurface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);

    // Set the destination size.
    QSize const destinationSize = QSize(100, 50);
    vp->setDestinationSize(destinationSize);

    // Change the source rectangle.
    QRectF const rect(QPointF(100.5, 200.3), QSizeF(200, 100));
    vp->setSourceRectangle(rect);
    s->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 2);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::source_rectangle);
    QCOMPARE(serverSurface->size(), destinationSize);
    QVERIFY((serverSurface->state().source_rectangle.topLeft() - rect.topLeft()).manhattanLength()
            < 0.01);
    QVERIFY((serverSurface->state().source_rectangle.bottomRight() - rect.bottomRight())
                .manhattanLength()
            < 0.01);

    // Unset the destination size.
    vp->setDestinationSize(QSize(-1, -1));
    s->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed to that of the source rectangle.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 3);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QCOMPARE(serverSurface->state().updates & Wrapland::Server::surface_change::source_rectangle,
             false);
    QCOMPARE(serverSurface->size(), rect.size());
    QVERIFY((serverSurface->state().source_rectangle.topLeft() - rect.topLeft()).manhattanLength()
            < 0.01);
    QVERIFY((serverSurface->state().source_rectangle.bottomRight() - rect.bottomRight())
                .manhattanLength()
            < 0.01);

    // Set the destination size again.
    vp->setDestinationSize(destinationSize);

    // Change the source rectangle to non-integer values.
    QRectF const rect2(QPointF(100.5, 200.3), QSizeF(200.5, 100.5));
    vp->setSourceRectangle(rect2);
    s->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed back.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 4);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::source_rectangle);
    QCOMPARE(serverSurface->size(), destinationSize);
    QVERIFY((serverSurface->state().source_rectangle.topLeft() - rect2.topLeft()).manhattanLength()
            < 0.01);
    QVERIFY((serverSurface->state().source_rectangle.bottomRight() - rect2.bottomRight())
                .manhattanLength()
            < 0.01);

    // And try to unset the destination size, what leads to an error.
    QSignalSpy errorSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    QVERIFY(!m_connection->error());

    vp->setDestinationSize(QSize(-1, -1));
    s->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->error());
    // TODO: compare protocol error code
}

void TestViewporter::testNoSurface()
{
    // This test verifies that setting the viewport for a surface twice results in a protocol error.

    // Create surface.
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Srv::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Clt::Surface> surface(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Srv::Surface* serverSurface = serverSurfaceCreated.first().first().value<Srv::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(server.globals.viewporter.get(),
                                     &Srv::Viewporter::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Clt::Viewport> vp(m_viewporter->createViewport(surface.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport = serverViewportCreated.first().first().value<Srv::Viewport*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy commit_spy(serverSurface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 600, 400));
    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);

    // Change the destination size.
    vp->setDestinationSize(QSize(400, 200));
    surface->commit(Clt::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(commit_spy.wait());
    QCOMPARE(commit_spy.count(), 2);
    QVERIFY(serverSurface->state().updates & Wrapland::Server::surface_change::size);
    QCOMPARE(serverSurface->size(), QSize(400, 200));

    // Now destroy the surface.
    surface.reset();

    // And try to set data with the viewport what leads to a protocol error.
    QSignalSpy errorSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    QVERIFY(!m_connection->error());

    vp->setDestinationSize(QSize(500, 300));
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->error());
    // TODO: compare protocol error code
}

QTEST_GUILESS_MAIN(TestViewporter)
#include "viewporter.moc"
