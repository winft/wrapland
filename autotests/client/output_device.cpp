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
#include "../../src/client/registry.h"

#include "../../server/display.h"
#include "../../server/globals.h"

#include <wayland-client-protocol.h>

#include <QtTest>

namespace Srv = Wrapland::Server;
namespace Clt = Wrapland::Client;

class TestOutputDevice : public QObject
{
    Q_OBJECT
public:
    explicit TestOutputDevice(QObject* parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void testRegistry();

    void testEnabled();
    void testModeChanges();

    void testTransform_data();
    void testTransform();

    void testDone();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Output* output{nullptr};
    } server;

    std::string m_name = "HDMI-A";
    std::string m_make = "Foocorp";
    std::string m_model = "Barmodel";
    std::string m_description;
    QString m_serialNumber;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::EventQueue* m_queue;
    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-wayland-output-0"};

TestOutputDevice::TestOutputDevice(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestOutputDevice::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    server.globals.outputs.push_back(
        std::make_unique<Wrapland::Server::Output>(server.display.get()));
    server.output = server.globals.outputs.back().get();

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

    server.output->set_mode(1);

    server.output->set_name(m_name);
    server.output->set_make(m_make);
    server.output->set_model(m_model);

    server.output->generate_description();
    m_description = server.output->description();
    QCOMPARE(m_description, m_make + " " + m_model + " (" + m_name + ")");

    m_serialNumber = "23498723948723";
    server.output->set_serial_number(m_serialNumber.toStdString());

    server.output->set_enabled(true);
    server.output->done();

    // setup connection
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
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());
}

void TestOutputDevice::cleanup()
{
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

void TestOutputDevice::testRegistry()
{
    server.output->set_geometry(QRectF(QPointF(100, 50), QSizeF(400, 200)));
    server.output->set_physical_size(QSize(200, 100));
    server.output->done();

    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceV1Announced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    Wrapland::Client::OutputDeviceV1 output;
    QVERIFY(!output.isValid());
    QCOMPARE(output.geometry(), QRectF());
    QCOMPARE(output.make(), QString());
    QCOMPARE(output.model(), QString());
    QCOMPARE(output.physicalSize(), QSize());
    QCOMPARE(output.pixelSize(), QSize());
    QCOMPARE(output.refreshRate(), 0);
    QCOMPARE(output.transform(), Clt::OutputDeviceV1::Transform::Normal);
    QCOMPARE(output.enabled(), Clt::OutputDeviceV1::Enablement::Enabled);
    QCOMPARE(output.serialNumber(), QString());

    QSignalSpy outputChanged(&output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(outputChanged.isValid());

    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(),
                                             announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());

    QVERIFY(outputChanged.wait());

    QCOMPARE(output.geometry(), QRectF(100, 50, 400, 200));
    QCOMPARE(output.physicalSize(), QSize(200, 100));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);
    // for xwayland transform is normal
    QCOMPARE(output.transform(), Wrapland::Client::OutputDeviceV1::Transform::Normal);

    QCOMPARE(output.enabled(), Clt::OutputDeviceV1::Enablement::Enabled);
    QCOMPARE(output.name(), QString::fromStdString(m_name));
    QCOMPARE(output.make(), QString::fromStdString(m_make));
    QCOMPARE(output.model(), QString::fromStdString(m_model));
    QCOMPARE(output.description(), QString::fromStdString(m_description));
    QCOMPARE(output.serialNumber(), m_serialNumber);
}

void TestOutputDevice::testModeChanges()
{
    using namespace Wrapland::Client;
    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceV1Announced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    Wrapland::Client::OutputDeviceV1 output;
    QSignalSpy outputChanged(&output, &Wrapland::Client::OutputDeviceV1::changed);
    QVERIFY(outputChanged.isValid());
    QSignalSpy modeAddedSpy(&output, &Wrapland::Client::OutputDeviceV1::modeAdded);
    QVERIFY(modeAddedSpy.isValid());
    QSignalSpy doneSpy(&output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(doneSpy.isValid());
    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(),
                                             announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeAddedSpy.count(), 3);

    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().size, QSize(800, 600));
    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().preferred, true);
    QVERIFY(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().id > -1);

    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().refreshRate, 90000);
    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().preferred, false);
    QVERIFY(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().id > -1);

    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>().id, output.currentMode().id);

    auto modes = output.modes();
    QVERIFY(modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>().id > -1);
    QCOMPARE(modes.size(), 3);
    QCOMPARE(modes.at(0), modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>());
    QCOMPARE(modes.at(1), modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>());
    QCOMPARE(modes.at(2), modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>());

    QCOMPARE(output.pixelSize(), QSize(1024, 768));

    // change the current mode
    outputChanged.clear();
    QSignalSpy modeChangedSpy(&output, &Wrapland::Client::OutputDeviceV1::modeChanged);
    QVERIFY(modeChangedSpy.isValid());
    server.output->set_mode(0);
    server.output->done();
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeChangedSpy.size(), 1);

    // the one which got the current flag
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().preferred, true);
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().id,
             output.currentMode().id);
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(800, 600));

    auto modes2 = output.modes();
    QCOMPARE(modes2.at(0).size, QSize(800, 600));
    QCOMPARE(modes2.at(0).refreshRate, 60000);
    QCOMPARE(modes2.at(0).preferred, true);
    QCOMPARE(modes2.at(0).id, output.currentMode().id);
    QCOMPARE(modes2.at(1).size, QSize(1280, 1024));
    QCOMPARE(modes2.at(1).refreshRate, 90000);
    QCOMPARE(modes2.at(1).preferred, false);
    QVERIFY(modes2.at(1).id != output.currentMode().id);
    QCOMPARE(modes2.at(2).size, QSize(1024, 768));
    QCOMPARE(modes2.at(2).refreshRate, 60000);
    QCOMPARE(modes2.at(2).preferred, false);
    QVERIFY(modes2.at(2).id != output.currentMode().id);

    // change once more
    outputChanged.clear();
    modeChangedSpy.clear();
    server.output->set_mode(2);
    server.output->done();
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeChangedSpy.size(), 1);

    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().refreshRate, 90000);
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().preferred, false);
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().id,
             output.currentMode().id);
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(1280, 1024));
}

void TestOutputDevice::testTransform_data()
{
    QTest::addColumn<Clt::OutputDeviceV1::Transform>("expected");
    QTest::addColumn<Srv::output_transform>("actual");

    QTest::newRow("90") << Clt::OutputDeviceV1::Transform::Rotated90
                        << Srv::output_transform::rotated_90;
    QTest::newRow("180") << Clt::OutputDeviceV1::Transform::Rotated180
                         << Srv::output_transform::rotated_180;
    QTest::newRow("270") << Clt::OutputDeviceV1::Transform::Rotated270
                         << Srv::output_transform::rotated_270;
    QTest::newRow("Flipped") << Clt::OutputDeviceV1::Transform::Flipped
                             << Srv::output_transform::flipped;
    QTest::newRow("Flipped 90") << Clt::OutputDeviceV1::Transform::Flipped90
                                << Srv::output_transform::flipped_90;
    QTest::newRow("Flipped 180") << Clt::OutputDeviceV1::Transform::Flipped180
                                 << Srv::output_transform::flipped_180;
    QTest::newRow("Flipped 280") << Clt::OutputDeviceV1::Transform::Flipped270
                                 << Srv::output_transform::flipped_270;
}

void TestOutputDevice::testTransform()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    QFETCH(Srv::output_transform, actual);
    server.output->set_transform(actual);
    server.output->done();

    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceV1Announced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    Wrapland::Client::OutputDeviceV1* output
        = registry.createOutputDeviceV1(announced.first().first().value<quint32>(),
                                        announced.first().last().value<quint32>(),
                                        &registry);
    QSignalSpy outputChanged(output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(outputChanged.isValid());
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QTEST(output->transform(), "expected");

    // change back to normal
    outputChanged.clear();
    server.output->set_transform(Srv::output_transform::normal);
    server.output->done();
    QVERIFY(outputChanged.wait());
    QCOMPARE(output->transform(), Clt::OutputDeviceV1::Transform::Normal);
}

void TestOutputDevice::testEnabled()
{
    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceV1Announced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    Wrapland::Client::OutputDeviceV1 output;
    QSignalSpy outputChanged(&output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(),
                                             announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.enabled(), Clt::OutputDeviceV1::Enablement::Enabled);

    QSignalSpy changed(&output, &Wrapland::Client::OutputDeviceV1::changed);
    QSignalSpy enabledChanged(&output, &Wrapland::Client::OutputDeviceV1::enabledChanged);
    QVERIFY(changed.isValid());
    QVERIFY(enabledChanged.isValid());

    server.output->set_enabled(false);
    server.output->done();
    QVERIFY(enabledChanged.wait());
    QCOMPARE(output.enabled(), Clt::OutputDeviceV1::Enablement::Disabled);
    if (changed.count() != enabledChanged.count()) {
        QVERIFY(changed.wait());
    }
    QCOMPARE(changed.count(), enabledChanged.count());

    server.output->set_enabled(true);
    server.output->done();
    QVERIFY(enabledChanged.wait());
    QCOMPARE(output.enabled(), Clt::OutputDeviceV1::Enablement::Enabled);
    if (changed.count() != enabledChanged.count()) {
        QVERIFY(changed.wait());
    }
    QCOMPARE(changed.count(), enabledChanged.count());
}
#if 0
void TestOutputDevice::testEdid()
{
    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    Wrapland::Client::OutputDeviceV1 output;

    QCOMPARE(output.edid(), QByteArray());

    QSignalSpy outputChanged(&output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(),
                                             announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.edid(), m_edid);
}

void TestOutputDevice::testId()
{
    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    Wrapland::Client::OutputDeviceV1 output;
    QSignalSpy outputChanged(&output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(),
                                             announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.uuid(), QByteArray("1337"));

    QSignalSpy idChanged(&output, &Wrapland::Client::OutputDeviceV1::uuidChanged);
    QVERIFY(idChanged.isValid());

    server.output->setUuid("42");
    QVERIFY(idChanged.wait());
    QCOMPARE(idChanged.first().first().toByteArray(), QByteArray("42"));
    idChanged.clear();
    QCOMPARE(output.uuid(), QByteArray("42"));

    server.output->setUuid("4711");
    QVERIFY(idChanged.wait());
    QCOMPARE(idChanged.first().first().toByteArray(), QByteArray("4711"));
    idChanged.clear();
    QCOMPARE(output.uuid(), QByteArray("4711"));
}
#endif
void TestOutputDevice::testDone()
{
    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceV1Announced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    Wrapland::Client::OutputDeviceV1 output;
    QSignalSpy outputDone(&output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(outputDone.isValid());
    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(),
                                             announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputDone.wait());
}

QTEST_GUILESS_MAIN(TestOutputDevice)
#include "output_device.moc"
