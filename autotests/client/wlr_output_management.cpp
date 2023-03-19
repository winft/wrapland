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
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/output.h"
#include "../../src/client/registry.h"
#include "../../src/client/wlr_output_configuration_v1.h"
#include "../../src/client/wlr_output_manager_v1.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/output.h"
#include "../../server/output_configuration_v1.h"
#include "../../server/output_management_v1.h"

#include "../../tests/globals.h"

#include <QtTest>
#include <wayland-client-protocol.h>

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class TestWlrOutputManagement : public QObject
{
    Q_OBJECT
public:
    explicit TestWlrOutputManagement(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        std::unique_ptr<Wrapland::Server::output> output;
    } server;

    Clt::Registry* m_registry = nullptr;
    Clt::WlrOutputHeadV1* m_outputHead = nullptr;
    Clt::WlrOutputManagerV1* m_outputManager = nullptr;
    Clt::WlrOutputConfigurationV1* m_outputConfiguration = nullptr;
    QList<Clt::WlrOutputHeadV1*> m_clientOutputs;
    QList<Srv::output_mode> m_modes;

    Clt::ConnectionThread* m_connection = nullptr;
    Clt::EventQueue* m_queue = nullptr;
    QThread* m_thread;

    QSignalSpy* m_announcedSpy;
    QSignalSpy* m_omSpy;
    QSignalSpy* m_configSpy;
};

constexpr auto socket_name{"wrapland-test-output-0"};

TestWlrOutputManagement::TestWlrOutputManagement(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
    , m_announcedSpy(nullptr)
{
    qRegisterMetaType<Srv::OutputConfigurationV1*>();
}

void TestWlrOutputManagement::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.output = std::make_unique<Wrapland::Server::output>(server.display.get());

    Srv::output_mode m0;
    m0.id = 0;
    m0.size = QSize(800, 600);
    m0.preferred = true;
    server.output->add_mode(m0);

    Srv::output_mode m1;
    m1.id = 1;
    m1.size = QSize(1024, 768);
    server.output->add_mode(m1);

    Srv::output_mode m2;
    m2.id = 2;
    m2.size = QSize(1280, 1024);
    m2.refresh_rate = 90000;
    server.output->add_mode(m2);

    Srv::output_mode m3;
    m3.id = 3;
    m3.size = QSize(1920, 1080);
    m3.refresh_rate = 100000;
    server.output->add_mode(m3);

    m_modes << m0 << m1 << m2 << m3;

    server.output->set_mode(1);
    server.output->set_geometry(QRectF(QPointF(0, 1920), QSizeF(1024, 768)));

    server.globals.output_management_v1
        = std::make_unique<Wrapland::Server::OutputManagementV1>(server.display.get());

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
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

    m_registry = new Clt::Registry();

    m_announcedSpy = new QSignalSpy(m_registry, &Clt::Registry::wlrOutputManagerV1Announced);
    m_omSpy = new QSignalSpy(m_registry, &Clt::Registry::outputDeviceV1Announced);

    QVERIFY(m_announcedSpy->isValid());
    QVERIFY(m_omSpy->isValid());

    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setEventQueue(m_queue);
    m_registry->setup();
    wl_display_flush(m_connection->display());

    // We currently don't have a server implementation for the wlr_output_management_unstable_v1
    // protocol.
    QVERIFY(!m_announcedSpy->wait(500));
    QCOMPARE(m_announcedSpy->count(), 0);
}

void TestWlrOutputManagement::cleanup()
{
    if (m_outputConfiguration) {
        delete m_outputConfiguration;
        m_outputConfiguration = nullptr;
    }
    delete m_outputHead;
    m_clientOutputs.clear();
    if (m_outputManager) {
        delete m_outputManager;
        m_outputManager = nullptr;
    }

    delete m_announcedSpy;
    delete m_omSpy;

    if (m_registry) {
        delete m_registry;
        m_registry = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
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

    server = {};
}

QTEST_GUILESS_MAIN(TestWlrOutputManagement)
#include "wlr_output_management.moc"
