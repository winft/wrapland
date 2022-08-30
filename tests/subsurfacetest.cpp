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
#include "../src/client/compositor.h"
#include "../src/client/connection_thread.h"
#include "../src/client/datadevice.h"
#include "../src/client/datadevicemanager.h"
#include "../src/client/dataoffer.h"
#include "../src/client/event_queue.h"
#include "../src/client/keyboard.h"
#include "../src/client/registry.h"
#include "../src/client/seat.h"
#include "../src/client/shell.h"
#include "../src/client/shm_pool.h"
#include "../src/client/subcompositor.h"
#include "../src/client/surface.h"
// Qt
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMimeType>
#include <QThread>
#include <QTimer>
// system
#include <unistd.h>

#include <linux/input.h>

using namespace Wrapland::Client;

class SubSurfaceTest : public QObject
{
    Q_OBJECT
public:
    explicit SubSurfaceTest(QObject* parent = nullptr);
    virtual ~SubSurfaceTest();

    void init();

private:
    void setupRegistry(Registry* registry);
    void render();
    QThread* m_connectionThread;
    ConnectionThread* m_connectionThreadObject;
    EventQueue* m_eventQueue = nullptr;
    Compositor* m_compositor = nullptr;
    Seat* m_seat = nullptr;
    Shell* m_shell = nullptr;
    ShellSurface* m_shellSurface = nullptr;
    ShmPool* m_shm = nullptr;
    Surface* m_surface = nullptr;
    SubCompositor* m_subCompositor = nullptr;
};

SubSurfaceTest::SubSurfaceTest(QObject* parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

SubSurfaceTest::~SubSurfaceTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void SubSurfaceTest::init()
{
    connect(
        m_connectionThreadObject,
        &ConnectionThread::establishedChanged,
        this,
        [this](bool established) {
            if (!established) {
                return;
            }
            m_eventQueue = new EventQueue(this);
            m_eventQueue->setup(m_connectionThreadObject);

            Registry* registry = new Registry(this);
            setupRegistry(registry);
        },
        Qt::QueuedConnection);
    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();

    m_connectionThreadObject->establishConnection();
}

void SubSurfaceTest::setupRegistry(Registry* registry)
{
    connect(registry,
            &Registry::compositorAnnounced,
            this,
            [this, registry](quint32 name, quint32 version) {
                m_compositor = registry->createCompositor(name, version, this);
            });
    connect(
        registry, &Registry::shellAnnounced, this, [this, registry](quint32 name, quint32 version) {
            m_shell = registry->createShell(name, version, this);
        });
    connect(
        registry, &Registry::shmAnnounced, this, [this, registry](quint32 name, quint32 version) {
            m_shm = registry->createShmPool(name, version, this);
        });
    connect(
        registry, &Registry::seatAnnounced, this, [this, registry](quint32 name, quint32 version) {
            m_seat = registry->createSeat(name, version, this);
        });
    connect(registry, &Registry::interfacesAnnounced, this, [this, registry] {
        Q_ASSERT(m_compositor);
        Q_ASSERT(m_seat);
        Q_ASSERT(m_shell);
        Q_ASSERT(m_shm);
        m_surface = m_compositor->createSurface(this);
        Q_ASSERT(m_surface);
        m_shellSurface = m_shell->createSurface(m_surface, this);
        Q_ASSERT(m_shellSurface);
        m_shellSurface->setToplevel();
        connect(m_shellSurface, &ShellSurface::sizeChanged, this, &SubSurfaceTest::render);

        auto subInterface = registry->interface(Registry::Interface::SubCompositor);
        if (subInterface.name != 0) {
            m_subCompositor
                = registry->createSubCompositor(subInterface.name, subInterface.version, this);
            Q_ASSERT(m_subCompositor);
            // create the sub surface
            auto surface = m_compositor->createSurface(this);
            Q_ASSERT(surface);
            auto subsurface = m_subCompositor->createSubSurface(surface, m_surface, this);
            Q_ASSERT(subsurface);
            QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::red);
            surface->attachBuffer(m_shm->createBuffer(image));
            surface->damage(QRect(0, 0, 100, 100));
            surface->commit(Surface::CommitFlag::None);
            // and another sub-surface to the sub-surface
            auto surface2 = m_compositor->createSurface(this);
            Q_ASSERT(surface2);
            auto subsurface2 = m_subCompositor->createSubSurface(surface2, surface, this);
            Q_ASSERT(subsurface2);
            QImage green(QSize(50, 50), QImage::Format_ARGB32_Premultiplied);
            green.fill(Qt::green);
            surface2->attachBuffer(m_shm->createBuffer(green));
            surface2->damage(QRect(0, 0, 50, 50));
            surface2->commit(Surface::CommitFlag::None);
            QTimer* timer = new QTimer(this);
            connect(timer, &QTimer::timeout, surface2, [surface2, this] {
                QImage yellow(QSize(50, 50), QImage::Format_ARGB32_Premultiplied);
                yellow.fill(Qt::yellow);
                surface2->attachBuffer(m_shm->createBuffer(yellow));
                surface2->damage(QRect(0, 0, 50, 50));
                surface2->commit(Surface::CommitFlag::None);
                m_surface->commit(Surface::CommitFlag::None);
            });
            timer->setSingleShot(true);
            timer->start(5000);
        }
        render();
    });
    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void SubSurfaceTest::render()
{
    QSize const& size = m_shellSurface->size().isValid() ? m_shellSurface->size() : QSize(200, 200);
    auto buffer = m_shm->getBuffer(size, size.width() * 4).lock();
    buffer->setUsed(true);
    QImage image(
        buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::blue);

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), size));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    SubSurfaceTest client;
    client.init();

    return app.exec();
}

#include "subsurfacetest.moc"
