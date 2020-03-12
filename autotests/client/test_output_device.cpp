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
#include "../../src/server/output_device_v1_interface.h"
#include "../../src/client/registry.h"
#include "../../src/server/display.h"

#include <wayland-client-protocol.h>

#include <QtTest>

using namespace Wrapland::Client;
using namespace Wrapland::Server;

class TestOutputDevice : public QObject
{
    Q_OBJECT
public:
    explicit TestOutputDevice(QObject *parent = nullptr);
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
    Wrapland::Server::Display *m_display;
    Wrapland::Server::OutputDeviceV1Interface *m_serverOutputDevice;
    QByteArray m_edid;
    QString m_serialNumber;
    QString m_eidaId;

    Wrapland::Client::ConnectionThread *m_connection;
    Wrapland::Client::EventQueue *m_queue;
    QThread *m_thread;

};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-output-0");

TestOutputDevice::TestOutputDevice(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverOutputDevice(nullptr)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
    qRegisterMetaType<OutputDeviceV1::Enablement>();
}

void TestOutputDevice::init()
{
    using namespace Wrapland::Server;

    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_serverOutputDevice = m_display->createOutputDeviceV1(this);
    m_serverOutputDevice->setUuid("1337");


    OutputDeviceV1Interface::Mode m0;
    m0.id = 0;
    m0.size = QSize(800, 600);
    m0.flags = OutputDeviceV1Interface::ModeFlags(OutputDeviceV1Interface::ModeFlag::Preferred);
    m_serverOutputDevice->addMode(m0);

    OutputDeviceV1Interface::Mode m1;
    m1.id = 1;
    m1.size = QSize(1024, 768);
    m_serverOutputDevice->addMode(m1);

    OutputDeviceV1Interface::Mode m2;
    m2.id = 2;
    m2.size = QSize(1280, 1024);
    m2.refreshRate = 90000;
    m_serverOutputDevice->addMode(m2);

    m_serverOutputDevice->setMode(1);

    m_edid = QByteArray::fromBase64("\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x10\xAC\x16\xF0LLKA\x0E\x16\x01\x03\x80""4 x\xEA\x1E\xC5\xAEO4\xB1&\x0EPT\xA5K\x00\x81\x80\xA9@\xD1\x00qO\x01\x01\x01\x01\x01\x01\x01\x01(<\x80\xA0p\xB0#@0 6\x00\x06""D!\x00\x00\x1A\x00\x00\x00\xFF\x00""F525M245AK");
    m_serverOutputDevice->setEdid(m_edid);

    m_serialNumber = "23498723948723";
    m_serverOutputDevice->setSerialNumber(m_serialNumber);
    m_eidaId = "asdffoo";
    m_serverOutputDevice->setEisaId(m_eidaId);

    m_serverOutputDevice->create();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.wait());

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

    delete m_serverOutputDevice;
    m_serverOutputDevice = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestOutputDevice::testRegistry()
{
    m_serverOutputDevice->setGeometry(QRectF(QPointF(100, 50), QSizeF(400, 200)));
    m_serverOutputDevice->setPhysicalSize(QSize(200, 100));
    m_serverOutputDevice->broadcast();

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
    QCOMPARE(output.uuid(), QByteArray());
    QCOMPARE(output.geometry(), QRectF());
    QCOMPARE(output.manufacturer(), QString());
    QCOMPARE(output.model(), QString());
    QCOMPARE(output.physicalSize(), QSize());
    QCOMPARE(output.pixelSize(), QSize());
    QCOMPARE(output.refreshRate(), 0);
    QCOMPARE(output.transform(), OutputDeviceV1::Transform::Normal);
    QCOMPARE(output.enabled(), OutputDeviceV1::Enablement::Enabled);
    QCOMPARE(output.edid(), QByteArray());
    QCOMPARE(output.eisaId(), QString());
    QCOMPARE(output.serialNumber(), QString());

    QSignalSpy outputChanged(&output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(outputChanged.isValid());

    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());

    QVERIFY(outputChanged.wait());

    QCOMPARE(output.geometry(), QRectF(100, 50, 400, 200));
    QCOMPARE(output.manufacturer(), QStringLiteral("kwinft"));
    QCOMPARE(output.model(), QStringLiteral("none"));
    QCOMPARE(output.physicalSize(), QSize(200, 100));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);
    // for xwayland transform is normal
    QCOMPARE(output.transform(), Wrapland::Client::OutputDeviceV1::Transform::Normal);

    QCOMPARE(output.edid(), m_edid);
    QCOMPARE(output.enabled(), OutputDeviceV1::Enablement::Enabled);
    QCOMPARE(output.uuid(), QByteArray("1337"));
    QCOMPARE(output.serialNumber(), m_serialNumber);
    QCOMPARE(output.eisaId(), m_eidaId);
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
    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeAddedSpy.count(), 3);

    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().size, QSize(800, 600));
    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().flags, OutputDeviceV1::Mode::Flags(OutputDeviceV1::Mode::Flag::Preferred));
    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().output, QPointer<OutputDeviceV1>(&output));
    QVERIFY(modeAddedSpy.at(0).first().value<OutputDeviceV1::Mode>().id > -1);

    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().refreshRate, 90000);
    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().flags, OutputDeviceV1::Mode::Flags(OutputDeviceV1::Mode::Flag::None));
    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().output, QPointer<OutputDeviceV1>(&output));
    QVERIFY(modeAddedSpy.at(1).first().value<OutputDeviceV1::Mode>().id > -1);

    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>().flags, OutputDeviceV1::Mode::Flags(OutputDeviceV1::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDeviceV1::Mode>().output, QPointer<OutputDeviceV1>(&output));
    const QList<OutputDeviceV1::Mode> &modes = output.modes();
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
    m_serverOutputDevice->setMode(0);
    m_serverOutputDevice->broadcast();
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().flags, OutputDeviceV1::Mode::Flags());
    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().flags, OutputDeviceV1::Mode::Flags(OutputDeviceV1::Mode::Flag::Current | OutputDeviceV1::Mode::Flag::Preferred));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(800, 600));
    const QList<OutputDeviceV1::Mode> &modes2 = output.modes();
    QCOMPARE(modes2.at(0).size, QSize(1280, 1024));
    QCOMPARE(modes2.at(0).refreshRate, 90000);
    QCOMPARE(modes2.at(0).flags, OutputDeviceV1::Mode::Flag::None);
    QCOMPARE(modes2.at(1).size, QSize(1024, 768));
    QCOMPARE(modes2.at(1).refreshRate, 60000);
    QCOMPARE(modes2.at(1).flags, OutputDeviceV1::Mode::Flag::None);
    QCOMPARE(modes2.at(2).size, QSize(800, 600));
    QCOMPARE(modes2.at(2).refreshRate, 60000);
    QCOMPARE(modes2.at(2).flags, OutputDeviceV1::Mode::Flag::Current | OutputDeviceV1::Mode::Flag::Preferred);

    // change once more
    outputChanged.clear();
    modeChangedSpy.clear();
    m_serverOutputDevice->setMode(2);
    m_serverOutputDevice->broadcast();
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<OutputDeviceV1::Mode>().flags, OutputDeviceV1::Mode::Flags(OutputDeviceV1::Mode::Flag::Preferred));
    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().refreshRate, 90000);
    QCOMPARE(modeChangedSpy.last().first().value<OutputDeviceV1::Mode>().flags, OutputDeviceV1::Mode::Flags(OutputDeviceV1::Mode::Flag::Current));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(1280, 1024));
}

void TestOutputDevice::testTransform_data()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    QTest::addColumn<Wrapland::Client::OutputDeviceV1::Transform>("expected");
    QTest::addColumn<Wrapland::Server::OutputDeviceV1Interface::Transform>("actual");

    QTest::newRow("90")          << OutputDeviceV1::Transform::Rotated90  << OutputDeviceV1Interface::Transform::Rotated90;
    QTest::newRow("180")         << OutputDeviceV1::Transform::Rotated180 << OutputDeviceV1Interface::Transform::Rotated180;
    QTest::newRow("270")         << OutputDeviceV1::Transform::Rotated270 << OutputDeviceV1Interface::Transform::Rotated270;
    QTest::newRow("Flipped")     << OutputDeviceV1::Transform::Flipped    << OutputDeviceV1Interface::Transform::Flipped;
    QTest::newRow("Flipped 90")  << OutputDeviceV1::Transform::Flipped90  << OutputDeviceV1Interface::Transform::Flipped90;
    QTest::newRow("Flipped 180") << OutputDeviceV1::Transform::Flipped180 << OutputDeviceV1Interface::Transform::Flipped180;
    QTest::newRow("Flipped 280") << OutputDeviceV1::Transform::Flipped270 << OutputDeviceV1Interface::Transform::Flipped270;
}

void TestOutputDevice::testTransform()
{
    using namespace Wrapland::Client;
    using namespace Wrapland::Server;
    QFETCH(OutputDeviceV1Interface::Transform, actual);
    m_serverOutputDevice->setTransform(actual);
    m_serverOutputDevice->broadcast();

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

    Wrapland::Client::OutputDeviceV1 *output = registry.createOutputDeviceV1(announced.first().first().value<quint32>(), announced.first().last().value<quint32>(), &registry);
    QSignalSpy outputChanged(output, &Wrapland::Client::OutputDeviceV1::done);
    QVERIFY(outputChanged.isValid());
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QTEST(output->transform(), "expected");

    // change back to normal
    outputChanged.clear();
    m_serverOutputDevice->setTransform(OutputDeviceV1Interface::Transform::Normal);
    m_serverOutputDevice->broadcast();
    QVERIFY(outputChanged.wait());
    QCOMPARE(output->transform(), OutputDeviceV1::Transform::Normal);
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
    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.enabled(), OutputDeviceV1::Enablement::Enabled);

    QSignalSpy changed(&output, &Wrapland::Client::OutputDeviceV1::changed);
    QSignalSpy enabledChanged(&output, &Wrapland::Client::OutputDeviceV1::enabledChanged);
    QVERIFY(changed.isValid());
    QVERIFY(enabledChanged.isValid());

    m_serverOutputDevice->setEnabled(OutputDeviceV1Interface::Enablement::Disabled);
    m_serverOutputDevice->broadcast();
    QVERIFY(enabledChanged.wait());
    QCOMPARE(output.enabled(), OutputDeviceV1::Enablement::Disabled);
    if (changed.count() != enabledChanged.count()) {
        QVERIFY(changed.wait());
    }
    QCOMPARE(changed.count(), enabledChanged.count());

    m_serverOutputDevice->setEnabled(OutputDeviceV1Interface::Enablement::Enabled);
    m_serverOutputDevice->broadcast();
    QVERIFY(enabledChanged.wait());
    QCOMPARE(output.enabled(), OutputDeviceV1::Enablement::Enabled);
    if (changed.count() != enabledChanged.count()) {
        QVERIFY(changed.wait());
    }
    QCOMPARE(changed.count(), enabledChanged.count());
}

//void TestOutputDevice::testEdid()
//{
//    Wrapland::Client::Registry registry;
//    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
//    QVERIFY(interfacesAnnouncedSpy.isValid());
//    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceAnnounced);
//    registry.setEventQueue(m_queue);
//    registry.create(m_connection->display());
//    QVERIFY(registry.isValid());
//    registry.setup();
//    wl_display_flush(m_connection->display());
//    QVERIFY(interfacesAnnouncedSpy.wait());

//    Wrapland::Client::OutputDeviceV1 output;

//    QCOMPARE(output.edid(), QByteArray());

//    QSignalSpy outputChanged(&output, &Wrapland::Client::OutputDeviceV1::done);
//    QVERIFY(outputChanged.isValid());
//    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
//    wl_display_flush(m_connection->display());
//    QVERIFY(outputChanged.wait());
//    QCOMPARE(output.edid(), m_edid);
//}

//void TestOutputDevice::testId()
//{
//    Wrapland::Client::Registry registry;
//    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
//    QVERIFY(interfacesAnnouncedSpy.isValid());
//    QSignalSpy announced(&registry, &Wrapland::Client::Registry::outputDeviceAnnounced);
//    registry.setEventQueue(m_queue);
//    registry.create(m_connection->display());
//    QVERIFY(registry.isValid());
//    registry.setup();
//    wl_display_flush(m_connection->display());
//    QVERIFY(interfacesAnnouncedSpy.wait());

//    Wrapland::Client::OutputDeviceV1 output;
//    QSignalSpy outputChanged(&output, &Wrapland::Client::OutputDeviceV1::done);
//    QVERIFY(outputChanged.isValid());
//    output.setup(registry.bindOutputDeviceV1(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
//    wl_display_flush(m_connection->display());
//    QVERIFY(outputChanged.wait());

//    QCOMPARE(output.uuid(), QByteArray("1337"));

//    QSignalSpy idChanged(&output, &Wrapland::Client::OutputDeviceV1::uuidChanged);
//    QVERIFY(idChanged.isValid());

//    m_serverOutputDevice->setUuid("42");
//    QVERIFY(idChanged.wait());
//    QCOMPARE(idChanged.first().first().toByteArray(), QByteArray("42"));
//    idChanged.clear();
//    QCOMPARE(output.uuid(), QByteArray("42"));

//    m_serverOutputDevice->setUuid("4711");
//    QVERIFY(idChanged.wait());
//    QCOMPARE(idChanged.first().first().toByteArray(), QByteArray("4711"));
//    idChanged.clear();
//    QCOMPARE(output.uuid(), QByteArray("4711"));
//}

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
#include "test_output_device.moc"
