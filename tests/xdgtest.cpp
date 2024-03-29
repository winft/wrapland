/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#include "../src/client/event_queue.h"
#include "../src/client/pointer.h"
#include "../src/client/registry.h"
#include "../src/client/seat.h"
#include "../src/client/shadow.h"
#include "../src/client/shell.h"
#include "../src/client/shm_pool.h"
#include "../src/client/surface.h"
#include "../src/client/xdg_shell.h"
// Qt
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QThread>

using namespace Wrapland::Client;

class XdgTest : public QObject
{
    Q_OBJECT
public:
    explicit XdgTest(QObject* parent = nullptr);
    virtual ~XdgTest();

    void init();

private:
    void setupRegistry(Registry* registry);
    void createPopup();
    void render();
    void renderPopup();
    QThread* m_connectionThread;
    ConnectionThread* m_connectionThreadObject;
    EventQueue* m_eventQueue = nullptr;
    Compositor* m_compositor = nullptr;
    ShmPool* m_shm = nullptr;
    Surface* m_surface = nullptr;
    XdgShell* m_xdgShell = nullptr;
    XdgShellToplevel* xdg_shell_toplevel = nullptr;
    Surface* m_popupSurface = nullptr;
    XdgShellPopup* m_xdgShellPopup = nullptr;
};

XdgTest::XdgTest(QObject* parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

XdgTest::~XdgTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void XdgTest::init()
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

void XdgTest::setupRegistry(Registry* registry)
{
    connect(registry,
            &Registry::compositorAnnounced,
            this,
            [this, registry](quint32 name, quint32 version) {
                m_compositor = registry->createCompositor(name, version, this);
            });
    connect(
        registry, &Registry::shmAnnounced, this, [this, registry](quint32 name, quint32 version) {
            m_shm = registry->createShmPool(name, version, this);
        });
    connect(registry,
            &Registry::xdgShellAnnounced,
            this,
            [this, registry](quint32 name, quint32 version) {
                m_xdgShell = registry->createXdgShell(name, version, this);
                m_xdgShell->setEventQueue(m_eventQueue);
            });
    connect(registry, &Registry::interfacesAnnounced, this, [this] {
        Q_ASSERT(m_compositor);
        Q_ASSERT(m_xdgShell);
        Q_ASSERT(m_shm);
        m_surface = m_compositor->createSurface(this);
        Q_ASSERT(m_surface);
        xdg_shell_toplevel = m_xdgShell->create_toplevel(m_surface, this);
        Q_ASSERT(xdg_shell_toplevel);
        connect(xdg_shell_toplevel, &XdgShellToplevel::configured, this, [this](auto serial) {
            xdg_shell_toplevel->ackConfigure(serial);
            render();
        });

        xdg_shell_toplevel->setTitle(QStringLiteral("Test Window"));

        m_surface->commit();
    });
    connect(registry, &Registry::seatAnnounced, this, [this, registry](quint32 name) {
        Seat* s = registry->createSeat(name, 2, this);
        connect(s, &Seat::hasPointerChanged, this, [this, s](bool has) {
            if (!has) {
                return;
            }
            Pointer* p = s->createPointer(this);
            connect(
                p,
                &Pointer::buttonStateChanged,
                this,
                [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState state) {
                    Q_UNUSED(button)
                    Q_UNUSED(serial)
                    Q_UNUSED(time)
                    if (state == Pointer::ButtonState::Released) {
                        if (m_popupSurface) {
                            m_popupSurface->deleteLater();
                            m_popupSurface = nullptr;
                        } else {
                            createPopup();
                        }
                    }
                });
        });
    });

    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void XdgTest::createPopup()
{
    if (m_popupSurface) {
        m_popupSurface->deleteLater();
    }

    m_popupSurface = m_compositor->createSurface(this);

    xdg_shell_positioner_data pos_data;
    pos_data.size = QSize(200, 200);
    pos_data.anchor.rect = QRect(50, 50, 400, 400);
    pos_data.anchor.edge = Qt::BottomEdge | Qt::RightEdge;
    pos_data.gravity = Qt::BottomEdge;
    pos_data.constraint_adjustments
        = xdg_shell_constraint_adjustment::flip_x | xdg_shell_constraint_adjustment::slide_y;

    m_xdgShellPopup
        = m_xdgShell->create_popup(m_popupSurface, xdg_shell_toplevel, pos_data, m_popupSurface);
    renderPopup();
}

void XdgTest::render()
{
    QSize const& size = xdg_shell_toplevel->get_configure_data().size.isValid()
        ? xdg_shell_toplevel->get_configure_data().size
        : QSize(500, 500);
    auto buffer = m_shm->getBuffer(size, size.width() * 4).lock();
    buffer->setUsed(true);
    QImage image(
        buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 255, 255, 255));
    // draw a red rectangle indicating the anchor of the top level
    QPainter painter(&image);
    painter.setBrush(Qt::red);
    painter.setPen(Qt::black);
    painter.drawRect(50, 50, 400, 400);

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), size));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

void XdgTest::renderPopup()
{
    QSize size(200, 200);
    auto buffer = m_shm->getBuffer(size, size.width() * 4).lock();
    buffer->setUsed(true);
    QImage image(
        buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(0, 0, 255, 255));

    m_popupSurface->attachBuffer(*buffer);
    m_popupSurface->damage(QRect(QPoint(0, 0), size));
    m_popupSurface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    XdgTest client;
    client.init();

    return app.exec();
}

#include "xdgtest.moc"
