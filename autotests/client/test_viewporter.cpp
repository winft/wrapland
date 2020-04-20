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
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/surface.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/viewporter.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/surface.h"

#include "../../src/server/buffer_interface.h"
#include "../../src/server/viewporter_interface.h"

#include <wayland-client-protocol.h>
#include <wayland-viewporter-client-protocol.h>

#include <QtTest>
#include <QImage>
#include <QPainter>

class TestViewporter : public QObject
{
    Q_OBJECT
public:
    explicit TestViewporter(QObject *parent = nullptr);
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
    Wrapland::Server::D_isplay *m_display;
    Wrapland::Server::Compositor *m_serverCompositor;
    Wrapland::Server::ViewporterInterface *m_viewporterInterface;
    Wrapland::Client::ConnectionThread *m_connection;
    Wrapland::Client::Compositor *m_compositor;
    Wrapland::Client::ShmPool *m_shm;
    Wrapland::Client::EventQueue *m_queue;
    Wrapland::Client::Viewporter *m_viewporter;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-viewporter-0");

TestViewporter::TestViewporter(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverCompositor(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_thread(nullptr)
{
}

void TestViewporter::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    m_display = new Wrapland::Server::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    m_display->createShm();

    m_serverCompositor = m_display->createCompositor(m_display);
    QVERIFY(m_serverCompositor);

    m_viewporterInterface = m_display->createViewporterInterface(m_display);
    QVERIFY(m_viewporterInterface);
    m_viewporterInterface->create();
    QVERIFY(m_viewporterInterface->isValid());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(establishedChanged(bool)));
    m_connection->setSocketName(s_socketName);

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
    registry.setEventQueue(m_queue);
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    QSignalSpy viewporterSpy(&registry, SIGNAL(viewporterAnnounced(quint32,quint32)));
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
                                             compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(shmSpy.first().first().value<quint32>(),
                                   shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());

    m_viewporter = registry.createViewporter(
                registry.interface(Wrapland::Client::Registry::Interface::Viewporter).name,
                registry.interface(Wrapland::Client::Registry::Interface::Viewporter).version,
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

    delete m_serverCompositor;
    m_serverCompositor = nullptr;

    delete m_viewporterInterface;
    m_viewporterInterface = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestViewporter::testViewportExists()
{
    // This test verifies that setting the viewport for a surface twice results in a protocol error.

    // Create surface.
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Wrapland::Server::Surface *serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp1(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    // Create second viewport with error.
    QSignalSpy errorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp2(m_viewporter->createViewport(s.get(), this));
    QVERIFY(errorSpy.wait());
}

void TestViewporter::testWithoutBuffer()
{
    // This test verifies that setting the viewport while buffer is zero does not change the size.

    // Create surface.
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    QVERIFY(!serverSurface->buffer());

    // Change the destination size.
    QSignalSpy sizeChangedSpy(serverSurface, &Wrapland::Server::Surface::sizeChanged);
    vp->setDestinationSize(QSize(400, 200));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // Even though a new destination size is set if there is no buffer, the surface size has not
    // changed.
    QVERIFY(!sizeChangedSpy.wait(100));
    QCOMPARE(sizeChangedSpy.count(), 0);
    QVERIFY(!serverSurface->size().isValid());

    // Now change the source rectangle.
    vp->setSourceRectangle(QRectF(QPointF(100, 50), QSizeF(400, 200)));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // Even though a new source rectangle is set if there is no buffer, the surface size has not
    // changed.
    QVERIFY(!sizeChangedSpy.wait(100));
    QCOMPARE(sizeChangedSpy.count(), 0);
    QVERIFY(!serverSurface->size().isValid());
}

void TestViewporter::testDestinationSize()
{
    // This test verifies setting the destination size is received by the server.

    // Create surface.
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Wrapland::Server::Surface *serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy sizeChangedSpy(serverSurface, &Wrapland::Server::Surface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    // Change the destination size.
    vp->setDestinationSize(QSize(400, 200));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 2);
    QCOMPARE(serverSurface->size(), QSize(400, 200));

    // Destroy the viewport and check that the size is reset.
    vp.reset();
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(serverSurface->size(), image.size());
}

void TestViewporter::testSourceRectangle()
{
    // This test verifies setting the source rectangle is received by the server.

    // Create surface.
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Wrapland::Server::Surface *serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy sizeChangedSpy(serverSurface, &Wrapland::Server::Surface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    // Change the source rectangle.
    const QRectF rect(QPointF(100, 50), QSizeF(400, 200));
    QSignalSpy sourceRectangleChangedSpy(serverSurface, &Wrapland::Server::Surface::sourceRectangleChanged);
    vp->setSourceRectangle(rect);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 2);
    if (sourceRectangleChangedSpy.count() == 0) {
        QVERIFY(sourceRectangleChangedSpy.wait());
    }
    QCOMPARE(sourceRectangleChangedSpy.count(), 1);
    QCOMPARE(serverSurface->size(), rect.size());
    QCOMPARE(serverSurface->sourceRectangle(), rect);

    // Destroy the viewport and check that the size is reset.
    vp.reset();
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(serverSurface->size(), image.size());
}

void TestViewporter::testDestinationSizeAndSourceRectangle()
{
    // This test verifies setting the destination size and source rectangle is received by the
    // server.

    // Create surface.
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Wrapland::Server::Surface *serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy sizeChangedSpy(serverSurface, &Wrapland::Server::Surface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    // Change the destination size.
    const QSize destinationSize = QSize(100, 50);
    vp->setDestinationSize(destinationSize);

    // Change the source rectangle.
    const QRectF rect(QPointF(100.1, 50.5), QSizeF(400.9, 200.8));
    QSignalSpy sourceRectangleChangedSpy(serverSurface, &Wrapland::Server::Surface::sourceRectangleChanged);
    vp->setSourceRectangle(rect);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 2);
    if (sourceRectangleChangedSpy.count() == 0) {
        QVERIFY(sourceRectangleChangedSpy.wait());
    }
    QCOMPARE(sourceRectangleChangedSpy.count(), 1);
    QCOMPARE(serverSurface->size(), destinationSize);
    QVERIFY((serverSurface->sourceRectangle().topLeft()
                - rect.topLeft()).manhattanLength() < 0.01);
    QVERIFY((serverSurface->sourceRectangle().bottomRight()
                - rect.bottomRight()).manhattanLength() < 0.01);

    // Destroy the viewport and check that the size is reset.
    vp.reset();
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 3);
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
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Wrapland::Server::Surface *serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy sizeChangedSpy(serverSurface, &Wrapland::Server::Surface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    QFETCH(QSize, destinationSize);
    QFETCH(QRectF, sourceRectangle);

    QSignalSpy errorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
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
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Wrapland::Server::Surface *serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy sizeChangedSpy(serverSurface, &Wrapland::Server::Surface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    // Change the source rectangle.
    const QRectF rect(QPointF(100, 200), QSizeF(500, 200));
    QSignalSpy sourceRectangleChangedSpy(serverSurface, &Wrapland::Server::Surface::sourceRectangleChanged);
    vp->setSourceRectangle(rect);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 2);
    if (sourceRectangleChangedSpy.count() == 0) {
        QVERIFY(sourceRectangleChangedSpy.wait());
    }
    QCOMPARE(sourceRectangleChangedSpy.count(), 1);
    QCOMPARE(serverSurface->size(), rect.size());
    QCOMPARE(serverSurface->sourceRectangle(), rect);

    // Now change the buffer such that the source rectangle would not be contained anymore.
    QImage image2(QSize(599, 399), QImage::Format_ARGB32_Premultiplied);
    image2.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image2));
    s->damage(QRect(0, 0, 599, 399));

    // Change the source rectangle accordingly.
    const QRectF rect2(QPointF(100, 200), QSizeF(499, 199));
    vp->setSourceRectangle(rect2);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 3);
    if (sourceRectangleChangedSpy.count() == 1) {
        QVERIFY(sourceRectangleChangedSpy.wait());
    }
    QCOMPARE(sourceRectangleChangedSpy.count(), 2);
    QCOMPARE(serverSurface->size(), rect2.size());
    QCOMPARE(serverSurface->sourceRectangle(), rect2);

    // Now change the buffer such that the source rectangle would not be contained anymore.
    QImage image3(QSize(598, 399), QImage::Format_ARGB32_Premultiplied);
    image3.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image3));

    // And try to commit without changing the source rectangle accordingly, what leads to an error.
    QSignalSpy errorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    QVERIFY(!m_connection->error());

    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->error());
    // TODO: compare protocol error code
}

void TestViewporter::testDestinationSizeChange()
{
    // This test verifies that changing the destination size reevaluates the source rectangle and
    // its integer condition correctly.

    // Create surface.
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Wrapland::Server::Surface *serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp(m_viewporter->createViewport(s.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy sizeChangedSpy(serverSurface, &Wrapland::Server::Surface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 600, 400));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    // Set the destination size.
    const QSize destinationSize = QSize(100, 50);
    vp->setDestinationSize(destinationSize);

    // Change the source rectangle.
    const QRectF rect(QPointF(100.5, 200.3), QSizeF(200, 100));
    QSignalSpy sourceRectangleChangedSpy(serverSurface, &Wrapland::Server::Surface::sourceRectangleChanged);
    vp->setSourceRectangle(rect);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 2);
    if (sourceRectangleChangedSpy.count() == 0) {
        QVERIFY(sourceRectangleChangedSpy.wait());
    }
    QCOMPARE(sourceRectangleChangedSpy.count(), 1);
    QCOMPARE(serverSurface->size(), destinationSize);
    QVERIFY((serverSurface->sourceRectangle().topLeft()
                - rect.topLeft()).manhattanLength() < 0.01);
    QVERIFY((serverSurface->sourceRectangle().bottomRight()
                - rect.bottomRight()).manhattanLength() < 0.01);

    // Unset the destination size.
    vp->setDestinationSize(QSize(-1, -1));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed to that of the source rectangle.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 3);
    if (sourceRectangleChangedSpy.count() == 1) {
        QVERIFY(!sourceRectangleChangedSpy.wait(100));
    }
    QCOMPARE(sourceRectangleChangedSpy.count(), 1);
    QCOMPARE(serverSurface->size(), rect.size());
    QVERIFY((serverSurface->sourceRectangle().topLeft()
                - rect.topLeft()).manhattanLength() < 0.01);
    QVERIFY((serverSurface->sourceRectangle().bottomRight()
                - rect.bottomRight()).manhattanLength() < 0.01);

    // Set the destination size again.
    vp->setDestinationSize(destinationSize);

    // Change the source rectangle to non-integer values.
    const QRectF rect2(QPointF(100.5, 200.3), QSizeF(200.5, 100.5));
    vp->setSourceRectangle(rect2);
    s->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed back.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 4);
    if (sourceRectangleChangedSpy.count() == 1) {
        QVERIFY(sourceRectangleChangedSpy.wait());
    }
    QCOMPARE(sourceRectangleChangedSpy.count(), 2);
    QCOMPARE(serverSurface->size(), destinationSize);
    QVERIFY((serverSurface->sourceRectangle().topLeft()
                - rect2.topLeft()).manhattanLength() < 0.01);
    QVERIFY((serverSurface->sourceRectangle().bottomRight()
                - rect2.bottomRight()).manhattanLength() < 0.01);

    // And try to unset the destination size, what leads to an error.
    QSignalSpy errorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    QVERIFY(!m_connection->error());

    vp->setDestinationSize(QSize(-1, -1));
    s->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->error());
    // TODO: compare protocol error code
}

void TestViewporter::testNoSurface()
{
    // This test verifies that setting the viewport for a surface twice results in a protocol error.

    // Create surface.
    QSignalSpy serverSurfaceCreated(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    std::unique_ptr<Wrapland::Client::Surface> surface(m_compositor->createSurface());

    QVERIFY(serverSurfaceCreated.wait());
    Wrapland::Server::Surface *serverSurface
            = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // Create viewport.
    QSignalSpy serverViewportCreated(m_viewporterInterface, &Wrapland::Server::ViewporterInterface::viewportCreated);
    QVERIFY(serverViewportCreated.isValid());
    std::unique_ptr<Wrapland::Client::Viewport> vp(m_viewporter->createViewport(surface.get(), this));

    QVERIFY(serverViewportCreated.wait());
    auto serverViewport
            = serverViewportCreated.first().first().value<Wrapland::Server::ViewportInterface*>();
    QVERIFY(serverViewport);

    // Add a buffer.
    QSignalSpy sizeChangedSpy(serverSurface, &Wrapland::Server::Surface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QImage image(QSize(600, 400), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 600, 400));
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    // Change the destination size.
    vp->setDestinationSize(QSize(400, 200));
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    // The size of the surface has changed.
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 2);
    QCOMPARE(serverSurface->size(), QSize(400, 200));

    // Now destroy the surface.
    surface.reset();

    // And try to set data with the viewport what leads to a protocol error.
    QSignalSpy errorSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(errorSpy.isValid());
    QVERIFY(!m_connection->error());

    vp->setDestinationSize(QSize(500, 300));
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->error());
    // TODO: compare protocol error code
}

QTEST_GUILESS_MAIN(TestViewporter)
#include "test_viewporter.moc"
