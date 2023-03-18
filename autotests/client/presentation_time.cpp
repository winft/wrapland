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
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/output.h"
#include "../../src/client/presentation_time.h"
#include "../../src/client/registry.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/surface.h"

#include "../../server/buffer.h"
#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/presentation_time.h"
#include "../../server/surface.h"

using namespace Wrapland;

class TestPresentationTime : public QObject
{
    Q_OBJECT
public:
    explicit TestPresentationTime(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testClockId();
    void testPresented();
    void testDiscarded();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Client::ConnectionThread* m_connection;
    Client::EventQueue* m_queue;
    Client::Compositor* m_compositor;
    Client::ShmPool* m_shm;
    Client::PresentationManager* m_presentation;
    Client::Output* m_output;

    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-wayland-compositor-0"};

TestPresentationTime::TestPresentationTime(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_thread(nullptr)
{
}

void TestPresentationTime::init()
{
    qRegisterMetaType<Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();

    server.globals.compositor = server.display->createCompositor();
    server.globals.presentation_manager = server.display->createPresentationManager();

    server.globals.outputs.push_back(std::make_unique<Server::output>(server.display.get()));
    server.globals.outputs.back()->set_enabled(true);
    server.globals.outputs.back()->done();

    // Setup connection.
    m_connection = new Client::ConnectionThread;
    QSignalSpy establishedSpy(m_connection, &Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(establishedSpy.wait());

    m_queue = new Client::EventQueue(this);
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    QSignalSpy clientConnectedSpy(server.display.get(), &Server::Display::clientConnected);
    QVERIFY(clientConnectedSpy.isValid());

    Client::Registry registry;
    QSignalSpy compositorSpy(&registry, &Client::Registry::compositorAnnounced);
    QSignalSpy shmSpy(&registry, &Client::Registry::shmAnnounced);
    QSignalSpy presentationSpy(&registry, &Client::Registry::presentationManagerAnnounced);
    QSignalSpy outputSpy(&registry, &Client::Registry::outputAnnounced);
    QSignalSpy allAnnounced(&registry, &Client::Registry::interfacesAnnounced);
    QVERIFY(compositorSpy.isValid());
    QVERIFY(shmSpy.isValid());
    QVERIFY(presentationSpy.isValid());
    QVERIFY(outputSpy.isValid());
    QVERIFY(allAnnounced.isValid());

    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(allAnnounced.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);
    QVERIFY(m_compositor->isValid());

    m_shm = registry.createShmPool(
        shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());

    m_presentation
        = registry.createPresentationManager(presentationSpy.first().first().value<quint32>(),
                                             presentationSpy.first().last().value<quint32>(),
                                             this);
    QVERIFY(m_presentation->isValid());

    m_output = registry.createOutput(outputSpy.first().first().value<quint32>(),
                                     outputSpy.first().last().value<quint32>(),
                                     this);
    QVERIFY(m_output->isValid());

    QVERIFY(clientConnectedSpy.wait());
}

void TestPresentationTime::cleanup()
{
    delete m_output;
    m_output = nullptr;
    delete m_presentation;
    m_presentation = nullptr;
    delete m_shm;
    m_shm = nullptr;
    delete m_compositor;
    m_compositor = nullptr;
    delete m_queue;
    m_queue = nullptr;

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

void TestPresentationTime::testClockId()
{
    Client::Registry registry;
    Client::EventQueue queue;
    queue.setup(m_connection);

    QSignalSpy presentationSpy(&registry, &Client::Registry::presentationManagerAnnounced);
    QVERIFY(presentationSpy.isValid());

    registry.setEventQueue(&queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    server.globals.presentation_manager->setClockId(2);
    QCOMPARE(server.globals.presentation_manager->clockId(), 2);

    QVERIFY(presentationSpy.wait());
    std::unique_ptr<Client::PresentationManager> presentation{
        registry.createPresentationManager(presentationSpy.first().first().value<quint32>(),
                                           presentationSpy.first().last().value<quint32>())};
    QVERIFY(presentation->isValid());

    QSignalSpy clockSpy(presentation.get(), &Client::PresentationManager::clockIdChanged);
    QVERIFY(clockSpy.isValid());

    QVERIFY(clockSpy.wait());
    QCOMPARE(clockSpy.count(), 1);
    QCOMPARE(presentation->clockId(), server.globals.presentation_manager->clockId());

    server.globals.presentation_manager->setClockId(3);
    QCOMPARE(server.globals.presentation_manager->clockId(), 3);

    QVERIFY(clockSpy.wait());
    QCOMPARE(clockSpy.count(), 2);
    QCOMPARE(presentation->clockId(), server.globals.presentation_manager->clockId());
}

void TestPresentationTime::testPresented()
{
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    auto surface = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Server::Surface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->state().damage, QRegion());
    QVERIFY(!serverSurface->isMapped());

    auto feedback = m_presentation->createFeedback(surface);

    QSignalSpy committedSpy(serverSurface, &Server::Surface::committed);
    QVERIFY(committedSpy.isValid());

    QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto buffer = m_shm->createBuffer(img);

    surface->attachBuffer(buffer);
    surface->damage(QRect(0, 0, 10, 10));
    surface->commit(Client::Surface::CommitFlag::None);

    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.count(), 1);

    serverSurface->setOutputs({server.globals.outputs.back().get()});
    auto id = serverSurface->lockPresentation(server.globals.outputs.back().get());
    QVERIFY(id);

    QSignalSpy presentedSpy(feedback, &Client::PresentationFeedback::presented);
    QVERIFY(presentedSpy.isValid());

    // Send feedback with some test values.
    serverSurface->presentationFeedback(id,
                                        1,
                                        2,
                                        3,
                                        4,
                                        5,
                                        6,
                                        Server::Surface::PresentationKind::Vsync
                                            | Server::Surface::PresentationKind::ZeroCopy);

    QVERIFY(presentedSpy.wait());
    QCOMPARE(presentedSpy.count(), 1);

    QCOMPARE(feedback->syncOutput(), m_output);

    QCOMPARE(feedback->tvSecHi(), 1);
    QCOMPARE(feedback->tvSecLo(), 2);
    QCOMPARE(feedback->tvNsec(), 3);
    QCOMPARE(feedback->refresh(), 4);
    QCOMPARE(feedback->seqHi(), 5);
    QCOMPARE(feedback->seqLo(), 6);
    QCOMPARE(feedback->flags(),
             Client::PresentationFeedback::Kind::Vsync
                 | Client::PresentationFeedback::Kind::ZeroCopy);

    delete feedback;
    delete surface;
}

void TestPresentationTime::testDiscarded()
{
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    auto surface = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<Server::Surface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->state().damage, QRegion());
    QVERIFY(!serverSurface->isMapped());

    auto feedback = m_presentation->createFeedback(surface);

    QSignalSpy committedSpy(serverSurface, &Server::Surface::committed);
    QVERIFY(committedSpy.isValid());

    QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto buffer = m_shm->createBuffer(img);

    surface->attachBuffer(buffer);
    surface->damage(QRect(0, 0, 10, 10));
    surface->commit(Client::Surface::CommitFlag::None);

    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.count(), 1);

    serverSurface->setOutputs({server.globals.outputs.back().get()});
    auto id = serverSurface->lockPresentation(server.globals.outputs.back().get());
    QVERIFY(id);

    QSignalSpy presentedSpy(feedback, &Client::PresentationFeedback::presented);
    QVERIFY(presentedSpy.isValid());
    QSignalSpy discardedSpy(feedback, &Client::PresentationFeedback::discarded);
    QVERIFY(discardedSpy.isValid());

    // Send feedback with some test values.
    serverSurface->presentationDiscarded(id);

    QVERIFY(discardedSpy.wait());
    QCOMPARE(discardedSpy.count(), 1);
    QCOMPARE(presentedSpy.count(), 0);

    QCOMPARE(feedback->syncOutput(), nullptr);

    delete feedback;
    delete surface;
}

QTEST_GUILESS_MAIN(TestPresentationTime)
#include "presentation_time.moc"
