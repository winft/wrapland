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
#include "../../src/client/wlr_output_manager_v1.h"
#include "../../src/client/wlr_output_configuration_v1.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/output_configuration_v1.h"
#include "../../server/output_device_v1.h"
#include "../../server/output_management_v1.h"

#include <QtTest>

#include <wayland-client-protocol.h>

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class TestWlrOutputManagement : public QObject
{
    Q_OBJECT
public:
    explicit TestWlrOutputManagement(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

private:
    Srv::D_isplay *m_display;
    Srv::OutputManagementV1 *m_outputManagementInterface;
    QList<Srv::OutputDeviceV1 *> m_serverOutputs;

    Clt::Registry *m_registry = nullptr;
    Clt::WlrOutputHeadV1 *m_outputHead = nullptr;
    Clt::WlrOutputManagerV1 *m_outputManager = nullptr;
    Clt::WlrOutputConfigurationV1 *m_outputConfiguration = nullptr;
    QList<Clt::WlrOutputHeadV1*> m_clientOutputs;
    QList<Srv::OutputDeviceV1::Mode> m_modes;

    Clt::ConnectionThread *m_connection = nullptr;
    Clt::EventQueue *m_queue = nullptr;
    QThread *m_thread;

    QSignalSpy *m_announcedSpy;
    QSignalSpy *m_omSpy;
    QSignalSpy *m_configSpy;
};

static const QString s_socketName = QStringLiteral("wrapland-test-output-0");

TestWlrOutputManagement::TestWlrOutputManagement(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_outputManagementInterface(nullptr)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
    , m_announcedSpy(nullptr)
{
    qRegisterMetaType<Srv::OutputConfigurationV1*>();
}

void TestWlrOutputManagement::init()
{
    m_display = new Srv::D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->createCompositor(this);

    auto outputDeviceInterface = m_display->createOutputDeviceV1(this);

    Srv::OutputDeviceV1::Mode m0;
    m0.id = 0;
    m0.size = QSize(800, 600);
    m0.flags = Srv::OutputDeviceV1::ModeFlags(
                Srv::OutputDeviceV1::ModeFlag::Preferred);
    outputDeviceInterface->addMode(m0);

    Srv::OutputDeviceV1::Mode m1;
    m1.id = 1;
    m1.size = QSize(1024, 768);
    outputDeviceInterface->addMode(m1);

    Srv::OutputDeviceV1::Mode m2;
    m2.id = 2;
    m2.size = QSize(1280, 1024);
    m2.refreshRate = 90000;
    outputDeviceInterface->addMode(m2);

    Srv::OutputDeviceV1::Mode m3;
    m3.id = 3;
    m3.size = QSize(1920, 1080);
    m3.flags = Srv::OutputDeviceV1::ModeFlags();
    m3.refreshRate = 100000;
    outputDeviceInterface->addMode(m3);

    m_modes << m0 << m1 << m2 << m3;

    outputDeviceInterface->setMode(1);
    outputDeviceInterface->setGeometry(QRectF(QPointF(0, 1920), QSizeF(1024, 768)));
    m_serverOutputs << outputDeviceInterface;

    m_outputManagementInterface = m_display->createOutputManagementV1(this);

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    m_connection->setSocketName(s_socketName);

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

    m_announcedSpy = new QSignalSpy(m_registry,
                                    &Clt::Registry::wlrOutputManagerV1Announced);
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

    if (m_outputManagementInterface) {
        delete m_outputManagementInterface;
        m_outputManagementInterface = nullptr;
    }
    delete m_display;
    m_display = nullptr;
    m_serverOutputs.clear();
}

QTEST_GUILESS_MAIN(TestWlrOutputManagement)
#include "test_wlr_output_management.moc"
