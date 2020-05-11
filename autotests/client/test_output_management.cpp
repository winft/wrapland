/********************************************************************
Copyright © 2014 Martin Gräßlin <mgraesslin@kde.org>
Copyright © 2015 Sebastian Kügler <sebas@kde.org>
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
#include "../../src/client/output_device_v1.h"
#include "../../src/client/output_configuration_v1.h"
#include "../../src/client/output_management_v1.h"
#include "../../src/client/output.h"
#include "../../src/client/registry.h"

#include "../../server/display.h"
#include "../../server/compositor.h"
#include "../../server/output_changeset_v1.h"
#include "../../server/output_configuration_v1.h"
#include "../../server/output_device_v1.h"
#include "../../server/output_management_v1.h"

#include <QtTest>

#include <wayland-client-protocol.h>

namespace Srv = Wrapland::Server;
namespace Clt = Wrapland::Client;

class TestOutputManagement : public QObject
{
    Q_OBJECT
public:
    explicit TestOutputManagement(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();
    void createConfig();

    void testBasicMemoryManagement();
    void testMultipleSettings();
    void testConfigFailed();
    void testApplied();
    void testFailed();

    void testExampleConfig();
//    void testScale();

    void testRemoval();

private:
    void createOutputDevices();
    void testEnable();
    void applyPendingChanges(Srv::OutputConfigurationV1 *configurationInterface);

    Srv::D_isplay *m_display;
    Srv::OutputManagementV1 *m_outputManagementInterface;
    QList<Srv::OutputDeviceV1 *> m_serverOutputs;

    Clt::Registry *m_registry = nullptr;
    Clt::OutputDeviceV1 *m_outputDevice = nullptr;
    Clt::OutputManagementV1 *m_outputManagement = nullptr;
    Clt::OutputConfigurationV1 *m_outputConfiguration = nullptr;
    QList<Clt::OutputDeviceV1 *> m_clientOutputs;
    QList<Srv::OutputDeviceV1::Mode> m_modes;

    Clt::ConnectionThread *m_connection = nullptr;
    Clt::EventQueue *m_queue = nullptr;
    QThread *m_thread;

    QSignalSpy *m_announcedSpy;
    QSignalSpy *m_omSpy;
    QSignalSpy *m_configSpy;
};

static const QString s_socketName = QStringLiteral("wrapland-test-output-0");

TestOutputManagement::TestOutputManagement(QObject *parent)
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

void TestOutputManagement::init()
{
    using namespace Wrapland::Server;

    delete m_display;
    m_display = new D_isplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_display->createCompositor(this);

    auto outputDeviceInterface = m_display->createOutputDeviceV1(this);

    Srv::OutputDeviceV1::Mode m0;
    m0.id = 0;
    m0.size = QSize(800, 600);
    m0.flags = Srv::OutputDeviceV1::ModeFlags(Srv::OutputDeviceV1::ModeFlag::Preferred);
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
    outputDeviceInterface->done();
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
                                    &Clt::Registry::outputManagementV1Announced);
    m_omSpy = new QSignalSpy(m_registry, &Clt::Registry::outputDeviceV1Announced);

    QVERIFY(m_announcedSpy->isValid());
    QVERIFY(m_omSpy->isValid());

    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setEventQueue(m_queue);
    m_registry->setup();
    wl_display_flush(m_connection->display());

    QVERIFY(m_announcedSpy->wait());
    QCOMPARE(m_announcedSpy->count(), 1);

    m_outputManagement =
            m_registry->createOutputManagementV1(m_announcedSpy->first().first().value<quint32>(),
                                                 m_announcedSpy->first().last().value<quint32>());
    createOutputDevices();
}

void TestOutputManagement::cleanup()
{
    if (m_outputConfiguration) {
        delete m_outputConfiguration;
        m_outputConfiguration = nullptr;
    }
    delete m_outputDevice;
    m_clientOutputs.clear();
    if (m_outputManagement) {
        delete m_outputManagement;
        m_outputManagement = nullptr;
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

void TestOutputManagement::createConfig()
{
    m_outputConfiguration = m_outputManagement->createConfiguration();
}

void TestOutputManagement::applyPendingChanges(Srv::OutputConfigurationV1 *configurationInterface)
{
    auto changes = configurationInterface->changes();
    for (auto outputdevice: changes.keys()) {
        auto c = changes[outputdevice];
        if (c->enabledChanged()) {
            outputdevice->setEnabled(c->enabled());
        }
        if (c->modeChanged()) {
            outputdevice->setMode(c->mode());
        }
        if (c->transformChanged()) {
            outputdevice->setTransform(c->transform());
        }
        if (c->geometryChanged()) {
            outputdevice->setGeometry(c->geometry());
        }
    }
}

void TestOutputManagement::createOutputDevices()
{
    QCOMPARE(m_omSpy->count(), 1);
    QCOMPARE(m_registry->interfaces(Clt::Registry::Interface::OutputDeviceV1).count(),
             m_serverOutputs.count());

    auto output = new Clt::OutputDeviceV1();
    QVERIFY(!output->isValid());
    QCOMPARE(output->geometry(), QRect());
    QCOMPARE(output->geometry(), QRectF());
    QCOMPARE(output->manufacturer(), QString());
    QCOMPARE(output->model(), QString());
    QCOMPARE(output->physicalSize(), QSize());
    QCOMPARE(output->pixelSize(), QSize());
    QCOMPARE(output->refreshRate(), 0);
    QCOMPARE(output->transform(), Clt::OutputDeviceV1::Transform::Normal);
    QCOMPARE(output->enabled(), Clt::OutputDeviceV1::Enablement::Enabled);
    QCOMPARE(output->edid(), QByteArray());
    QCOMPARE(output->uuid(), QByteArray());

    QSignalSpy outputChanged(output, &Clt::OutputDeviceV1::changed);
    QVERIFY(outputChanged.isValid());

    output->setup(m_registry->bindOutputDeviceV1(m_omSpy->first().first().value<quint32>(), m_omSpy->first().last().value<quint32>()));
    wl_display_flush(m_connection->display());

    QVERIFY(outputChanged.wait());
    QCOMPARE(output->geometry().topLeft(), QPointF(0, 1920));
    QCOMPARE(output->enabled(), Clt::OutputDeviceV1::Enablement::Enabled);

    m_clientOutputs << output;
    m_outputDevice = output;

    QVERIFY(m_outputManagement->isValid());
}

void TestOutputManagement::testBasicMemoryManagement()
{
    createConfig();

    QSignalSpy serverApplySpy(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested);
    Srv::OutputConfigurationV1 *configurationInterface = nullptr;
    connect(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested, [=, &configurationInterface](Srv::OutputConfigurationV1 *c) {
        configurationInterface = c;
    });
    m_outputConfiguration->apply();

    QVERIFY(serverApplySpy.wait());
    QVERIFY(configurationInterface);

    QSignalSpy interfaceDeletedSpy(configurationInterface, &Srv::OutputConfigurationV1::resourceDestroyed);

    delete m_outputConfiguration;
    m_outputConfiguration = nullptr;

    QVERIFY(interfaceDeletedSpy.wait());
}

void TestOutputManagement::testRemoval()
{
    QSignalSpy outputManagementRemovedSpy(m_registry,
                                          &Clt::Registry::outputManagementV1Removed);
    QVERIFY(outputManagementRemovedSpy.isValid());

    delete m_outputManagementInterface;
    m_outputManagementInterface = nullptr;
    QVERIFY(outputManagementRemovedSpy.wait(200));
    QCOMPARE(outputManagementRemovedSpy.first().first(), m_announcedSpy->first().first());
    QVERIFY(!m_registry->hasInterface(Clt::Registry::Interface::OutputManagementV1));
    QVERIFY(m_registry->interfaces(Clt::Registry::Interface::OutputManagementV1).isEmpty());
}

void TestOutputManagement::testApplied()
{
    createConfig();
    QVERIFY(m_outputConfiguration->isValid());
    QSignalSpy appliedSpy(m_outputConfiguration, &Clt::OutputConfigurationV1::applied);

    connect(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested,
            [=](Srv::OutputConfigurationV1 *configurationInterface) {
                configurationInterface->setApplied();
        });
    m_outputConfiguration->apply();
    QVERIFY(appliedSpy.wait(200));
    QCOMPARE(appliedSpy.count(), 1);
}

void TestOutputManagement::testFailed()
{
    createConfig();
    QVERIFY(m_outputConfiguration->isValid());
    QSignalSpy failedSpy(m_outputConfiguration, &Clt::OutputConfigurationV1::failed);

    connect(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested,
            [=](Srv::OutputConfigurationV1 *configurationInterface) {
                configurationInterface->setFailed();
        });
    m_outputConfiguration->apply();
    QVERIFY(failedSpy.wait(200));
    QCOMPARE(failedSpy.count(), 1);

}

void TestOutputManagement::testEnable()
{
    createConfig();
    auto config = m_outputConfiguration;
    QVERIFY(config->isValid());

    Clt::OutputDeviceV1 *output = m_clientOutputs.first();
    QCOMPARE(output->enabled(), Clt::OutputDeviceV1::Enablement::Enabled);

    QSignalSpy enabledChanged(output, &Clt::OutputDeviceV1::enabledChanged);
    QVERIFY(enabledChanged.isValid());

    config->setEnabled(output, Clt::OutputDeviceV1::Enablement::Disabled);

    QVERIFY(!enabledChanged.wait(200));

    QCOMPARE(enabledChanged.count(), 0);

    // Reset
    config->setEnabled(output, Clt::OutputDeviceV1::Enablement::Disabled);
    config->apply();
}



void TestOutputManagement::testMultipleSettings()
{
    createConfig();
    auto config = m_outputConfiguration;
    QVERIFY(config->isValid());

    Clt::OutputDeviceV1 *output = m_clientOutputs.first();
    QSignalSpy outputChangedSpy(output, &Clt::OutputDeviceV1::changed);

    Srv::OutputConfigurationV1 *configurationInterface;
    connect(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested, [=, &configurationInterface](Srv::OutputConfigurationV1 *c) {
        applyPendingChanges(c);
        configurationInterface = c;
    });
    QSignalSpy serverApplySpy(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested);
    QVERIFY(serverApplySpy.isValid());

    config->setMode(output, m_modes.first().id);
    config->setTransform(output, Clt::OutputDeviceV1::Transform::Rotated90);
    config->setGeometry(output, QRectF(QPoint(13, 37), output->geometry().size() / 2.0));
    config->setEnabled(output, Clt::OutputDeviceV1::Enablement::Disabled);
    config->apply();

    QVERIFY(serverApplySpy.wait(200));
    QCOMPARE(serverApplySpy.count(), 1);

    configurationInterface->setApplied();

    QSignalSpy configAppliedSpy(config, &Clt::OutputConfigurationV1::applied);
    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));
    QCOMPARE(configAppliedSpy.count(), 1);
    QCOMPARE(outputChangedSpy.count(), 0);

    config->setMode(output, m_modes.at(1).id);
    config->setTransform(output, Clt::OutputDeviceV1::Transform::Normal);
    config->setGeometry(output, QRectF(QPoint(0, 1920), output->geometry().size()));
    config->setEnabled(output, Clt::OutputDeviceV1::Enablement::Enabled);
    config->apply();

    QVERIFY(serverApplySpy.wait(200));
    QCOMPARE(serverApplySpy.count(), 2);

    configurationInterface->setApplied();

    QVERIFY(configAppliedSpy.wait(200));
    QCOMPARE(configAppliedSpy.count(), 2);
    QCOMPARE(outputChangedSpy.count(), 0);

}

void TestOutputManagement::testConfigFailed()
{
    createConfig();
    auto config = m_outputConfiguration;
    Clt::OutputDeviceV1 *output = m_clientOutputs.first();

    QVERIFY(config->isValid());
    QVERIFY(output->isValid());

    QSignalSpy serverApplySpy(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested);
    QVERIFY(serverApplySpy.isValid());
    QSignalSpy outputChangedSpy(output, &Clt::OutputDeviceV1::changed);
    QVERIFY(outputChangedSpy.isValid());
    QSignalSpy configAppliedSpy(config, &Clt::OutputConfigurationV1::applied);
    QVERIFY(configAppliedSpy.isValid());
    QSignalSpy configFailedSpy(config, &Clt::OutputConfigurationV1::failed);
    QVERIFY(configFailedSpy.isValid());

    config->setMode(output, m_modes.last().id);
    config->setTransform(output, Clt::OutputDeviceV1::Transform::Normal);
    config->setGeometry(output, QRectF(QPoint(-1, -1), output->geometry().size()));

    config->apply();

    connect(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested, [=](Srv::OutputConfigurationV1 *c) {
        c->setFailed();
    });

    QVERIFY(serverApplySpy.wait(200));

    // Artificially make the server fail to apply the settings
    // Make sure the applied signal never comes, and that failed has been received
    QVERIFY(!configAppliedSpy.wait(200));
    QCOMPARE(configFailedSpy.count(), 1);
    QCOMPARE(configAppliedSpy.count(), 0);
}

void TestOutputManagement::testExampleConfig()
{
    createConfig();
    auto config = m_outputConfiguration;
    Clt::OutputDeviceV1 *output = m_clientOutputs.first();

    config->setMode(output, m_clientOutputs.first()->modes().last().id);
    config->setTransform(output, Clt::OutputDeviceV1::Transform::Normal);
//    config->setGeometry(output, QRectF(QPoint(-1, -1), output->geometry().size()));

    QSignalSpy configAppliedSpy(config, &Clt::OutputConfigurationV1::applied);
    connect(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested, [=](Srv::OutputConfigurationV1 *c) {
        c->setApplied();
    });
    config->apply();

    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));

}

//void TestOutputManagement::testScale()
//{
//    createConfig();

//    auto config = m_outputConfiguration;
//    Clt::OutputDeviceV1 *output = m_clientOutputs.first();

//    config->setScaleF(output, 2.3);
//    config->apply();

//    QSignalSpy configAppliedSpy(config, &OutputConfiguration::applied);
//    connect(m_outputManagementInterface, &Srv::OutputManagementV1::configurationChangeRequested, [=](Srv::OutputConfigurationV1 *c) {
//        applyPendingChanges(c);
//        c->setApplied();
//    });
//    QVERIFY(configAppliedSpy.isValid());
//    QVERIFY(configAppliedSpy.wait(200));

//    QCOMPARE(wl_fixed_from_double(output->scaleF()), wl_fixed_from_double(2.3));
//}


QTEST_GUILESS_MAIN(TestOutputManagement)
#include "test_output_management.moc"
