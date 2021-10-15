/********************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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
#include <wayland-client-protocol.h>

#include <QHash>
#include <QtTest>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../../src/client/buffer.h"
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/linux_dmabuf_v1.h"
#include "../../src/client/registry.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/linux_dmabuf_v1.h"

class DmabufImpl : public Wrapland::Server::LinuxDmabufV1::Impl
{
public:
    using Plane = Wrapland::Server::LinuxDmabufV1::Plane;
    using Flags = Wrapland::Server::LinuxDmabufV1::Flags;

    DmabufImpl();
    ~DmabufImpl() = default;

    Wrapland::Server::LinuxDmabufBufferV1* importBuffer(const QVector<Plane>& planes,
                                                        uint32_t format,
                                                        const QSize& size,
                                                        Flags flags) override;
    bool bufferAlwaysFail;
};

DmabufImpl::DmabufImpl()
    : Wrapland::Server::LinuxDmabufV1::Impl()
{
    bufferAlwaysFail = false;
}

Wrapland::Server::LinuxDmabufBufferV1*
DmabufImpl::importBuffer([[maybe_unused]] const QVector<Plane>& planes,
                         uint32_t format,
                         const QSize& size,
                         [[maybe_unused]] Flags flags)
{
    if (!bufferAlwaysFail) {
        return new Wrapland::Server::LinuxDmabufBufferV1(format, size);
    } else {
        return nullptr;
    }
}

class TestLinuxDmabuf : public QObject
{
    Q_OBJECT
public:
    explicit TestLinuxDmabuf(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();
    void testModifier();
    void testCreateBufferFail();
    void testCreateBufferSucess();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::LinuxDmabufV1* m_dmabuf;
    Wrapland::Client::EventQueue* m_queue;
    QHash<uint32_t, QSet<uint64_t>> modifiers;
    QThread* m_thread;
    DmabufImpl* m_bufferImpl;
};

constexpr auto socket_name{"wrapland-test-wayland-dmabuf-0"};

TestLinuxDmabuf::TestLinuxDmabuf(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_dmabuf(nullptr)
    , m_thread(nullptr)
{
}

void TestLinuxDmabuf::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    // Setup connection.
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(socket_name);

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

    QSignalSpy dmabufSpy(&registry, &Wrapland::Client::Registry::LinuxDmabufV1Announced);

    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    modifiers[1212].insert(12);

    server.globals.linux_dmabuf_v1 = server.display->createLinuxDmabuf();

    m_bufferImpl = new DmabufImpl();
    server.globals.linux_dmabuf_v1->setImpl(m_bufferImpl);
    QVERIFY(dmabufSpy.wait());

    m_dmabuf = registry.createLinuxDmabufV1(dmabufSpy.first().first().value<quint32>(),
                                            dmabufSpy.first().last().value<quint32>(),
                                            this);
    QVERIFY(m_dmabuf->isValid());
}

void TestLinuxDmabuf::cleanup()
{
    delete m_dmabuf;
    m_dmabuf = nullptr;

    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }

    delete m_bufferImpl;
    m_bufferImpl = nullptr;

    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    delete m_queue;
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    server = {};
}

void TestLinuxDmabuf::testModifier()
{
    QSignalSpy ModifierSpy(m_dmabuf, &Wrapland::Client::LinuxDmabufV1::supportedFormatsChanged);
    server.globals.linux_dmabuf_v1->setSupportedFormatsWithModifiers(modifiers);
    auto paramV1 = m_dmabuf->createParamsV1();
    QVERIFY(paramV1->isValid());
    QVERIFY(ModifierSpy.wait());

    auto receivedFormat = m_dmabuf->supportedFormats();
    QVERIFY(receivedFormat.contains(1212));
    QVERIFY(receivedFormat[1212].modifier_hi == 0);
    QVERIFY(receivedFormat[1212].modifier_lo == 12);

    delete paramV1;
}
void TestLinuxDmabuf::testCreateBufferFail()
{
    m_bufferImpl->bufferAlwaysFail = true;
    auto* paramV1 = m_dmabuf->createParamsV1();
    QVERIFY(paramV1->isValid());

    QSignalSpy CreateBufferFailSpy(paramV1, &Wrapland::Client::ParamsV1::createFail);
    QVERIFY(CreateBufferFailSpy.isValid());

    int fd = memfd_create("AutotestBuffercreatesucces", 0);
    ftruncate(fd, 500);
    paramV1->addDmabuf(fd, 0, 0, 0, 0, 12);
    paramV1->createDmabuf(32, 32, 64, 0);

    QVERIFY(CreateBufferFailSpy.wait());

    delete paramV1;
}

void TestLinuxDmabuf::testCreateBufferSucess()
{
    server.globals.linux_dmabuf_v1->setSupportedFormatsWithModifiers(modifiers);
    auto paramV1 = m_dmabuf->createParamsV1();
    QVERIFY(paramV1->isValid());

    QSignalSpy CreateBufferSuccessSpy(paramV1, &Wrapland::Client::ParamsV1::createSuccess);
    QVERIFY(CreateBufferSuccessSpy.isValid());

    // Format 64 = Sucess
    int fd = memfd_create("AutotestBuffercreatesucces", 0);
    ftruncate(fd, 500);
    paramV1->addDmabuf(fd, 0, 0, 0, 0, 12);
    paramV1->createDmabuf(32, 32, 64, 0);

    QVERIFY(CreateBufferSuccessSpy.wait());

    wl_buffer* buf = paramV1->getBuffer();
    free(buf);
    delete paramV1;
}

QTEST_GUILESS_MAIN(TestLinuxDmabuf)
#include "linux_dmabuf.moc"
