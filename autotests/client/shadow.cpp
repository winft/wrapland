/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
#include "../../src/client/registry.h"
#include "../../src/client/shadow.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/surface.h"

#include "../../server/buffer.h"
#include "../../server/client.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/shadow.h"
#include "../../server/surface.h"

class ShadowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateShadow();
    void testShadowElements();
    void testSurfaceDestroy();

private:
    Wrapland::Server::Display* m_display = nullptr;

    Wrapland::Client::ConnectionThread* m_connection = nullptr;
    Wrapland::Server::Compositor* m_serverCompositor = nullptr;
    Wrapland::Server::ShadowManager* m_shadowInterface = nullptr;
    QThread* m_thread = nullptr;
    Wrapland::Client::EventQueue* m_queue = nullptr;
    Wrapland::Client::ShmPool* m_shm = nullptr;
    Wrapland::Client::Compositor* m_compositor = nullptr;
    Wrapland::Client::ShadowManager* m_shadow = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-shadow-0");

void ShadowTest::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    m_display = new Wrapland::Server::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_display->createShm();
    m_serverCompositor = m_display->createCompositor(m_display);
    m_shadowInterface = m_display->createShadowManager(m_display);

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
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
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_shm = registry.createShmPool(
        registry.interface(Wrapland::Client::Registry::Interface::Shm).name,
        registry.interface(Wrapland::Client::Registry::Interface::Shm).version,
        this);
    QVERIFY(m_shm->isValid());
    m_compositor = registry.createCompositor(
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).name,
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).version,
        this);
    QVERIFY(m_compositor->isValid());
    m_shadow = registry.createShadowManager(
        registry.interface(Wrapland::Client::Registry::Interface::Shadow).name,
        registry.interface(Wrapland::Client::Registry::Interface::Shadow).version,
        this);
    QVERIFY(m_shadow->isValid());
}

void ShadowTest::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_shm)
    CLEANUP(m_compositor)
    CLEANUP(m_shadow)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    CLEANUP(m_serverCompositor)
    CLEANUP(m_shadowInterface)
    CLEANUP(m_display)

#undef CLEANUP
}

void ShadowTest::testCreateShadow()
{
    // this test verifies the basic shadow behavior, create for surface, commit it, etc.
    QSignalSpy serverSurfaceCreated(m_serverCompositor,
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface{m_compositor->createSurface()};
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    // a surface without anything should not have a Shadow
    QVERIFY(!serverSurface->state().shadow);
    QSignalSpy shadowChangedSpy(serverSurface, &Wrapland::Server::Surface::shadowChanged);
    QVERIFY(shadowChangedSpy.isValid());

    // let's create a shadow for the Surface
    std::unique_ptr<Wrapland::Client::Shadow> shadow(m_shadow->createShadow(surface.get()));
    // that should not have triggered the shadowChangedSpy)
    QVERIFY(!shadowChangedSpy.wait(100));

    // now let's commit the surface, that should trigger the shadow changed
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());
    QCOMPARE(shadowChangedSpy.count(), 1);

    // we didn't set anything on the shadow, so it should be all default values
    auto serverShadow = serverSurface->state().shadow;
    QVERIFY(serverShadow);
    QCOMPARE(serverShadow->offset(), QMarginsF());
    QVERIFY(!serverShadow->topLeft());
    QVERIFY(!serverShadow->top());
    QVERIFY(!serverShadow->topRight());
    QVERIFY(!serverShadow->right());
    QVERIFY(!serverShadow->bottomRight());
    QVERIFY(!serverShadow->bottom());
    QVERIFY(!serverShadow->bottomLeft());
    QVERIFY(!serverShadow->left());

    // now let's remove the shadow
    m_shadow->removeShadow(surface.get());
    // just removing should not remove it yet, surface needs to be committed
    QVERIFY(!shadowChangedSpy.wait(100));
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());
    QCOMPARE(shadowChangedSpy.count(), 2);
    QVERIFY(!serverSurface->state().shadow);
}

void ShadowTest::testShadowElements()
{

    // this test verifies that all shadow elements are correctly passed to the server
    // first create surface
    QSignalSpy serverSurfaceCreated(m_serverCompositor,
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface{m_compositor->createSurface()};
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    QSignalSpy shadowChangedSpy(serverSurface, &Wrapland::Server::Surface::shadowChanged);
    QVERIFY(shadowChangedSpy.isValid());
    // now create the shadow
    std::unique_ptr<Wrapland::Client::Shadow> shadow(m_shadow->createShadow(surface.get()));
    QImage topLeftImage(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    topLeftImage.fill(Qt::white);
    shadow->attachTopLeft(m_shm->createBuffer(topLeftImage));
    QImage topImage(QSize(11, 11), QImage::Format_ARGB32_Premultiplied);
    topImage.fill(Qt::black);
    shadow->attachTop(m_shm->createBuffer(topImage));
    QImage topRightImage(QSize(12, 12), QImage::Format_ARGB32_Premultiplied);
    topRightImage.fill(Qt::red);
    shadow->attachTopRight(m_shm->createBuffer(topRightImage));
    QImage rightImage(QSize(13, 13), QImage::Format_ARGB32_Premultiplied);
    rightImage.fill(Qt::darkRed);
    shadow->attachRight(m_shm->createBuffer(rightImage));
    QImage bottomRightImage(QSize(14, 14), QImage::Format_ARGB32_Premultiplied);
    bottomRightImage.fill(Qt::green);
    shadow->attachBottomRight(m_shm->createBuffer(bottomRightImage));
    QImage bottomImage(QSize(15, 15), QImage::Format_ARGB32_Premultiplied);
    bottomImage.fill(Qt::darkGreen);
    shadow->attachBottom(m_shm->createBuffer(bottomImage));
    QImage bottomLeftImage(QSize(16, 16), QImage::Format_ARGB32_Premultiplied);
    bottomLeftImage.fill(Qt::blue);
    shadow->attachBottomLeft(m_shm->createBuffer(bottomLeftImage));
    QImage leftImage(QSize(17, 17), QImage::Format_ARGB32_Premultiplied);
    leftImage.fill(Qt::darkBlue);
    shadow->attachLeft(m_shm->createBuffer(leftImage));
    shadow->setOffsets(QMarginsF(1, 2, 3, 4));
    shadow->commit();
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);

    QVERIFY(shadowChangedSpy.wait());
    auto serverShadow = serverSurface->state().shadow;
    QVERIFY(serverShadow);
    QCOMPARE(serverShadow->offset(), QMarginsF(1, 2, 3, 4));
    QCOMPARE(serverShadow->topLeft()->shmImage()->createQImage(), topLeftImage);
    QCOMPARE(serverShadow->top()->shmImage()->createQImage(), topImage);
    QCOMPARE(serverShadow->topRight()->shmImage()->createQImage(), topRightImage);
    QCOMPARE(serverShadow->right()->shmImage()->createQImage(), rightImage);
    QCOMPARE(serverShadow->bottomRight()->shmImage()->createQImage(), bottomRightImage);
    QCOMPARE(serverShadow->bottom()->shmImage()->createQImage(), bottomImage);
    QCOMPARE(serverShadow->bottomLeft()->shmImage()->createQImage(), bottomLeftImage);
    QCOMPARE(serverShadow->left()->shmImage()->createQImage(), leftImage);

    // try to destroy the buffer
    // first attach one buffer
    shadow->attachTopLeft(m_shm->createBuffer(topLeftImage));

    // We need to reference the shared_ptr buffer to guarantee receiving the destroyed signal.
    auto buf = serverShadow->topLeft();
    QSignalSpy destroyedSpy(serverShadow->topLeft().get(),
                            &Wrapland::Server::Buffer::resourceDestroyed);
    QVERIFY(destroyedSpy.isValid());

    delete m_shm;
    m_shm = nullptr;
    QVERIFY(destroyedSpy.wait());

    // now all buffers should be gone
    // TODO: does that need a signal?
    QVERIFY(!serverShadow->topLeft());
    QVERIFY(!serverShadow->top());
    QVERIFY(!serverShadow->topRight());
    QVERIFY(!serverShadow->right());
    QVERIFY(!serverShadow->bottomRight());
    QVERIFY(!serverShadow->bottom());
    QVERIFY(!serverShadow->bottomLeft());
    QVERIFY(!serverShadow->left());
}

void ShadowTest::testSurfaceDestroy()
{
    QSignalSpy serverSurfaceCreated(m_serverCompositor,
                                    SIGNAL(surfaceCreated(Wrapland::Server::Surface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    std::unique_ptr<Wrapland::Client::Surface> surface{m_compositor->createSurface()};
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface);

    QSignalSpy shadowChangedSpy(serverSurface, &Wrapland::Server::Surface::shadowChanged);
    QVERIFY(shadowChangedSpy.isValid());
    std::unique_ptr<Wrapland::Client::Shadow> shadow(m_shadow->createShadow(surface.get()));
    shadow->commit();
    surface->commit(Wrapland::Client::Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());
    auto serverShadow = serverSurface->state().shadow;
    QVERIFY(serverShadow);

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());
    QSignalSpy shadowDestroyedSpy(serverShadow.data(), &QObject::destroyed);
    QVERIFY(shadowDestroyedSpy.isValid());
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(shadowDestroyedSpy.isEmpty());
    // destroy the shadow
    shadow.reset();
    QVERIFY(shadowDestroyedSpy.wait());
    QCOMPARE(shadowDestroyedSpy.count(), 1);
}

QTEST_GUILESS_MAIN(ShadowTest)
#include "shadow.moc"
