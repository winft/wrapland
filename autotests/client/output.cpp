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

#include "../../src/client/connection_thread.h"
#include "../../src/client/dpms.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/output.h"
#include "../../src/client/registry.h"

#include "../../server/display.h"
#include "../../server/dpms.h"
#include "../../server/wl_output.h"

#include <wayland-client-protocol.h>

namespace Srv = Wrapland::Server;
namespace Clt = Wrapland::Client;

class TestOutput : public QObject
{
    Q_OBJECT
public:
    explicit TestOutput(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testRegistry();
    void testModeChanges();
    void testScaleChange();

    void testSubPixel_data();
    void testSubPixel();

    void testTransform_data();
    void testTransform();

    void testDpms_data();
    void testDpms();

    void testDpmsRequestMode_data();
    void testDpmsRequestMode();

private:
    Srv::Display* m_display;
    Srv::WlOutput* m_serverOutput;
    Clt::ConnectionThread* m_connection;
    Clt::EventQueue* m_queue;
    QThread* m_thread;
};

static const std::string s_socketName = "wrapland-test-output-0";

TestOutput::TestOutput(QObject* parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverOutput(nullptr)
    , m_connection(nullptr)
    , m_thread(nullptr)
{
}

void TestOutput::init()
{
    m_display = new Srv::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->running());

    m_serverOutput = m_display->createOutput(this);
    QCOMPARE(m_serverOutput->pixelSize(), QSize());
    QCOMPARE(m_serverOutput->refreshRate(), 60000);
    m_serverOutput->addMode(QSize(800, 600),
                            Srv::WlOutput::ModeFlags(Srv::WlOutput::ModeFlag::Preferred));
    QCOMPARE(m_serverOutput->pixelSize(), QSize(800, 600));
    m_serverOutput->addMode(QSize(1024, 768));
    m_serverOutput->addMode(QSize(1280, 1024), Srv::WlOutput::ModeFlags(), 90000);
    QCOMPARE(m_serverOutput->pixelSize(), QSize(800, 600));
    m_serverOutput->setCurrentMode(QSize(1024, 768));
    QCOMPARE(m_serverOutput->pixelSize(), QSize(1024, 768));
    QCOMPARE(m_serverOutput->refreshRate(), 60000);

    QCOMPARE(m_serverOutput->isDpmsSupported(), false);
    QCOMPARE(m_serverOutput->dpmsMode(), Srv::WlOutput::DpmsMode::Off);

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(QString::fromStdString(s_socketName));

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
}

void TestOutput::cleanup()
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

    delete m_serverOutput;
    m_serverOutput = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestOutput::testRegistry()
{
    QSignalSpy globalPositionChangedSpy(m_serverOutput, &Srv::WlOutput::globalPositionChanged);
    QVERIFY(globalPositionChangedSpy.isValid());
    QCOMPARE(m_serverOutput->globalPosition(), QPoint(0, 0));
    m_serverOutput->setGlobalPosition(QPoint(100, 50));
    QCOMPARE(m_serverOutput->globalPosition(), QPoint(100, 50));
    QCOMPARE(globalPositionChangedSpy.count(), 1);
    // changing again should not trigger signal
    m_serverOutput->setGlobalPosition(QPoint(100, 50));
    QCOMPARE(globalPositionChangedSpy.count(), 1);

    QSignalSpy physicalSizeChangedSpy(m_serverOutput, &Srv::WlOutput::physicalSizeChanged);
    QVERIFY(physicalSizeChangedSpy.isValid());
    QCOMPARE(m_serverOutput->physicalSize(), QSize());
    m_serverOutput->setPhysicalSize(QSize(200, 100));
    QCOMPARE(m_serverOutput->physicalSize(), QSize(200, 100));
    QCOMPARE(physicalSizeChangedSpy.count(), 1);
    // changing again should not trigger signal
    m_serverOutput->setPhysicalSize(QSize(200, 100));
    QCOMPARE(physicalSizeChangedSpy.count(), 1);

    Clt::Registry registry;
    QSignalSpy announced(&registry, &Clt::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    Clt::Output output;
    QVERIFY(!output.isValid());
    QCOMPARE(output.geometry(), QRect());
    QCOMPARE(output.globalPosition(), QPoint());
    QCOMPARE(output.manufacturer(), QString());
    QCOMPARE(output.model(), QString());
    QCOMPARE(output.physicalSize(), QSize());
    QCOMPARE(output.pixelSize(), QSize());
    QCOMPARE(output.refreshRate(), 0);
    QCOMPARE(output.scale(), 1);
    QCOMPARE(output.subPixel(), Clt::Output::SubPixel::Unknown);
    QCOMPARE(output.transform(), Clt::Output::Transform::Normal);

    QSignalSpy outputChanged(&output, &Clt::Output::changed);
    QVERIFY(outputChanged.isValid());

    auto o = registry.bindOutput(announced.first().first().value<quint32>(),
                                 announced.first().last().value<quint32>());
    QVERIFY(!Clt::Output::get(o));
    output.setup(o);
    QCOMPARE(Clt::Output::get(o), &output);
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.geometry(), QRect(100, 50, 1024, 768));
    QCOMPARE(output.globalPosition(), QPoint(100, 50));
    QCOMPARE(output.manufacturer(), QStringLiteral("org.kde.kwin"));
    QCOMPARE(output.model(), QStringLiteral("none"));
    QCOMPARE(output.physicalSize(), QSize(200, 100));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);
    QCOMPARE(output.scale(), 1);
    // for xwayland output it's unknown
    QCOMPARE(output.subPixel(), Clt::Output::SubPixel::Unknown);
    // for xwayland transform is normal
    QCOMPARE(output.transform(), Clt::Output::Transform::Normal);
}

void TestOutput::testModeChanges()
{
    // verify the server modes
    const auto serverModes = m_serverOutput->modes();
    QCOMPARE(serverModes.size(), 3);
    QCOMPARE(serverModes.at(0).size, QSize(800, 600));
    QCOMPARE(serverModes.at(1).size, QSize(1024, 768));
    QCOMPARE(serverModes.at(2).size, QSize(1280, 1024));
    QCOMPARE(serverModes.at(0).refreshRate, 60000);
    QCOMPARE(serverModes.at(1).refreshRate, 60000);
    QCOMPARE(serverModes.at(2).refreshRate, 90000);
    QCOMPARE(serverModes.at(0).flags, Srv::WlOutput::ModeFlags(Srv::WlOutput::ModeFlag::Preferred));
    QCOMPARE(serverModes.at(1).flags, Srv::WlOutput::ModeFlags(Srv::WlOutput::ModeFlag::Current));
    QCOMPARE(serverModes.at(2).flags, Srv::WlOutput::ModeFlags());

    using namespace Clt;
    Clt::Registry registry;
    QSignalSpy announced(&registry, &Clt::Registry::outputAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    Clt::Output output;
    QSignalSpy outputChanged(&output, &Clt::Output::changed);
    QVERIFY(outputChanged.isValid());

    QSignalSpy modeAddedSpy(&output, &Clt::Output::modeAdded);
    QVERIFY(modeAddedSpy.isValid());

    output.setup(registry.bindOutput(announced.first().first().value<quint32>(),
                                     announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(modeAddedSpy.count(), 3);
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().size, QSize(800, 600));
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().flags,
             Output::Mode::Flags(Output::Mode::Flag::Preferred));
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().output, QPointer<Output>(&output));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().refreshRate, 90000);
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().flags,
             Output::Mode::Flags(Output::Mode::Flag::None));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().output, QPointer<Output>(&output));
    QCOMPARE(modeAddedSpy.at(2).first().value<Output::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeAddedSpy.at(2).first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(2).first().value<Output::Mode>().flags,
             Output::Mode::Flags(Output::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(2).first().value<Output::Mode>().output, QPointer<Output>(&output));
    const QList<Output::Mode>& modes = output.modes();
    QCOMPARE(modes.size(), 3);
    QCOMPARE(modes.at(0), modeAddedSpy.at(0).first().value<Output::Mode>());
    QCOMPARE(modes.at(1), modeAddedSpy.at(1).first().value<Output::Mode>());
    QCOMPARE(modes.at(2), modeAddedSpy.at(2).first().value<Output::Mode>());

    QCOMPARE(output.pixelSize(), QSize(1024, 768));

    // change the current mode
    outputChanged.clear();
    QSignalSpy modeChangedSpy(&output, &Clt::Output::modeChanged);
    QVERIFY(modeChangedSpy.isValid());
    m_serverOutput->setCurrentMode(QSize(800, 600));
    QVERIFY(modeChangedSpy.wait());
    if (modeChangedSpy.size() == 1) {
        QVERIFY(modeChangedSpy.wait());
    }
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().flags, Output::Mode::Flags());
    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().flags,
             Output::Mode::Flags(Output::Mode::Flag::Current | Output::Mode::Flag::Preferred));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(800, 600));
    const QList<Output::Mode>& modes2 = output.modes();
    QCOMPARE(modes2.at(0).size, QSize(1280, 1024));
    QCOMPARE(modes2.at(0).refreshRate, 90000);
    QCOMPARE(modes2.at(0).flags, Output::Mode::Flag::None);
    QCOMPARE(modes2.at(1).size, QSize(1024, 768));
    QCOMPARE(modes2.at(1).refreshRate, 60000);
    QCOMPARE(modes2.at(1).flags, Output::Mode::Flag::None);
    QCOMPARE(modes2.at(2).size, QSize(800, 600));
    QCOMPARE(modes2.at(2).refreshRate, 60000);
    QCOMPARE(modes2.at(2).flags, Output::Mode::Flag::Current | Output::Mode::Flag::Preferred);

    // change once more
    outputChanged.clear();
    modeChangedSpy.clear();
    m_serverOutput->setCurrentMode(QSize(1280, 1024), 90000);
    QCOMPARE(m_serverOutput->refreshRate(), 90000);
    QVERIFY(modeChangedSpy.wait());
    if (modeChangedSpy.size() == 1) {
        QVERIFY(modeChangedSpy.wait());
    }
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().flags,
             Output::Mode::Flags(Output::Mode::Flag::Preferred));
    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().refreshRate, 90000);
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().flags,
             Output::Mode::Flags(Output::Mode::Flag::Current));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(1280, 1024));
}

void TestOutput::testScaleChange()
{
    Clt::Registry registry;
    QSignalSpy announced(&registry, &Clt::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    Clt::Output output;
    QSignalSpy outputChanged(&output, &Clt::Output::changed);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(),
                                     announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 1);

    // change the scale
    outputChanged.clear();
    QCOMPARE(m_serverOutput->scale(), 1);
    QSignalSpy serverScaleChanged(m_serverOutput, &Srv::WlOutput::scaleChanged);
    QVERIFY(serverScaleChanged.isValid());
    m_serverOutput->setScale(2);
    QCOMPARE(m_serverOutput->scale(), 2);
    QCOMPARE(serverScaleChanged.count(), 1);

    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 2);

    // changing to same value should not trigger
    m_serverOutput->setScale(2);
    QCOMPARE(serverScaleChanged.count(), 1);
    QVERIFY(!outputChanged.wait(100));

    // change once more
    outputChanged.clear();
    m_serverOutput->setScale(4);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 4);
}

void TestOutput::testSubPixel_data()
{
    QTest::addColumn<Clt::Output::SubPixel>("expected");
    QTest::addColumn<Srv::WlOutput::SubPixel>("actual");

    QTest::newRow("none") << Clt::Output::SubPixel::None << Srv::WlOutput::SubPixel::None;
    QTest::newRow("horizontal/rgb")
        << Clt::Output::SubPixel::HorizontalRGB << Srv::WlOutput::SubPixel::HorizontalRGB;
    QTest::newRow("horizontal/bgr")
        << Clt::Output::SubPixel::HorizontalBGR << Srv::WlOutput::SubPixel::HorizontalBGR;
    QTest::newRow("vertical/rgb") << Clt::Output::SubPixel::VerticalRGB
                                  << Srv::WlOutput::SubPixel::VerticalRGB;
    QTest::newRow("vertical/bgr") << Clt::Output::SubPixel::VerticalBGR
                                  << Srv::WlOutput::SubPixel::VerticalBGR;
}

void TestOutput::testSubPixel()
{
    QFETCH(Srv::WlOutput::SubPixel, actual);

    QCOMPARE(m_serverOutput->subPixel(), Srv::WlOutput::SubPixel::Unknown);
    QSignalSpy serverSubPixelChangedSpy(m_serverOutput, &Srv::WlOutput::subPixelChanged);
    QVERIFY(serverSubPixelChangedSpy.isValid());
    m_serverOutput->setSubPixel(actual);
    QCOMPARE(m_serverOutput->subPixel(), actual);
    QCOMPARE(serverSubPixelChangedSpy.count(), 1);
    // changing to same value should not trigger the signal
    m_serverOutput->setSubPixel(actual);
    QCOMPARE(serverSubPixelChangedSpy.count(), 1);

    Clt::Registry registry;
    QSignalSpy announced(&registry, &Clt::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    Clt::Output output;
    QSignalSpy outputChanged(&output, &Clt::Output::changed);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(),
                                     announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }

    QTEST(output.subPixel(), "expected");

    // change back to unknown
    outputChanged.clear();
    m_serverOutput->setSubPixel(Srv::WlOutput::SubPixel::Unknown);
    QCOMPARE(m_serverOutput->subPixel(), Srv::WlOutput::SubPixel::Unknown);
    QCOMPARE(serverSubPixelChangedSpy.count(), 2);
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output.subPixel(), Clt::Output::SubPixel::Unknown);
}

void TestOutput::testTransform_data()
{
    QTest::addColumn<Clt::Output::Transform>("expected");
    QTest::addColumn<Srv::WlOutput::Transform>("actual");

    QTest::newRow("90") << Clt::Output::Transform::Rotated90 << Srv::WlOutput::Transform::Rotated90;
    QTest::newRow("180") << Clt::Output::Transform::Rotated180
                         << Srv::WlOutput::Transform::Rotated180;
    QTest::newRow("270") << Clt::Output::Transform::Rotated270
                         << Srv::WlOutput::Transform::Rotated270;
    QTest::newRow("Flipped") << Clt::Output::Transform::Flipped
                             << Srv::WlOutput::Transform::Flipped;
    QTest::newRow("Flipped 90") << Clt::Output::Transform::Flipped90
                                << Srv::WlOutput::Transform::Flipped90;
    QTest::newRow("Flipped 180") << Clt::Output::Transform::Flipped180
                                 << Srv::WlOutput::Transform::Flipped180;
    QTest::newRow("Flipped 280") << Clt::Output::Transform::Flipped270
                                 << Srv::WlOutput::Transform::Flipped270;
}

void TestOutput::testTransform()
{
    QFETCH(Srv::WlOutput::Transform, actual);
    QCOMPARE(m_serverOutput->transform(), Srv::WlOutput::Transform::Normal);
    QSignalSpy serverTransformChangedSpy(m_serverOutput, &Srv::WlOutput::transformChanged);
    QVERIFY(serverTransformChangedSpy.isValid());
    m_serverOutput->setTransform(actual);
    QCOMPARE(m_serverOutput->transform(), actual);
    QCOMPARE(serverTransformChangedSpy.count(), 1);
    // changing to same should not trigger signal
    m_serverOutput->setTransform(actual);
    QCOMPARE(serverTransformChangedSpy.count(), 1);

    Clt::Registry registry;
    QSignalSpy announced(&registry, &Clt::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    auto output = registry.createOutput(announced.first().first().value<quint32>(),
                                        announced.first().last().value<quint32>(),
                                        &registry);
    QSignalSpy outputChanged(output, &Clt::Output::changed);
    QVERIFY(outputChanged.isValid());
    wl_display_flush(m_connection->display());
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }

    QTEST(output->transform(), "expected");

    // change back to normal
    outputChanged.clear();
    m_serverOutput->setTransform(Srv::WlOutput::Transform::Normal);
    QCOMPARE(m_serverOutput->transform(), Srv::WlOutput::Transform::Normal);
    QCOMPARE(serverTransformChangedSpy.count(), 2);
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output->transform(), Clt::Output::Transform::Normal);
}

void TestOutput::testDpms_data()
{
    QTest::addColumn<Clt::Dpms::Mode>("client");
    QTest::addColumn<Srv::WlOutput::DpmsMode>("server");

    QTest::newRow("Standby") << Clt::Dpms::Mode::Standby << Srv::WlOutput::DpmsMode::Standby;
    QTest::newRow("Suspend") << Clt::Dpms::Mode::Suspend << Srv::WlOutput::DpmsMode::Suspend;
    QTest::newRow("On") << Clt::Dpms::Mode::On << Srv::WlOutput::DpmsMode::On;
}

void TestOutput::testDpms()
{
    std::unique_ptr<Srv::DpmsManager> serverDpmsManager{m_display->createDpmsManager()};

    // set Dpms on the Output
    QSignalSpy serverDpmsSupportedChangedSpy(m_serverOutput, &Srv::WlOutput::dpmsSupportedChanged);
    QVERIFY(serverDpmsSupportedChangedSpy.isValid());
    QCOMPARE(m_serverOutput->isDpmsSupported(), false);
    m_serverOutput->setDpmsSupported(true);
    QCOMPARE(serverDpmsSupportedChangedSpy.count(), 1);
    QCOMPARE(m_serverOutput->isDpmsSupported(), true);

    Clt::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy announced(&registry, &Clt::Registry::interfacesAnnounced);
    QVERIFY(announced.isValid());

    QSignalSpy dpmsAnnouncedSpy(&registry, &Clt::Registry::dpmsAnnounced);
    QVERIFY(dpmsAnnouncedSpy.isValid());

    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    m_connection->flush();

    QVERIFY(announced.wait());
    QCOMPARE(dpmsAnnouncedSpy.count(), 1);

    auto output
        = registry.createOutput(registry.interface(Clt::Registry::Interface::Output).name,
                                registry.interface(Clt::Registry::Interface::Output).version,
                                &registry);

    auto dpmsManager = registry.createDpmsManager(dpmsAnnouncedSpy.first().first().value<quint32>(),
                                                  dpmsAnnouncedSpy.first().last().value<quint32>(),
                                                  &registry);
    QVERIFY(dpmsManager->isValid());

    auto* dpms = dpmsManager->getDpms(output, &registry);

    QSignalSpy clientDpmsSupportedChangedSpy(dpms, &Clt::Dpms::supportedChanged);
    QVERIFY(clientDpmsSupportedChangedSpy.isValid());
    QVERIFY(dpms->isValid());

    QCOMPARE(dpms->isSupported(), false);
    QCOMPARE(dpms->mode(), Clt::Dpms::Mode::On);

    m_connection->flush();
    QVERIFY(clientDpmsSupportedChangedSpy.wait());
    QCOMPARE(clientDpmsSupportedChangedSpy.count(), 1);
    QCOMPARE(dpms->isSupported(), true);

    // and let's change to suspend
    QSignalSpy serverDpmsModeChangedSpy(m_serverOutput, &Srv::WlOutput::dpmsModeChanged);
    QVERIFY(serverDpmsModeChangedSpy.isValid());
    QSignalSpy clientDpmsModeChangedSpy(dpms, &Clt::Dpms::modeChanged);
    QVERIFY(clientDpmsModeChangedSpy.isValid());

    QCOMPARE(m_serverOutput->dpmsMode(), Srv::WlOutput::DpmsMode::Off);
    QFETCH(Srv::WlOutput::DpmsMode, server);
    m_serverOutput->setDpmsMode(server);
    QCOMPARE(m_serverOutput->dpmsMode(), server);
    QCOMPARE(serverDpmsModeChangedSpy.count(), 1);

    QVERIFY(clientDpmsModeChangedSpy.wait());
    QCOMPARE(clientDpmsModeChangedSpy.count(), 1);
    QTEST(dpms->mode(), "client");

    // Test supported changed
    QSignalSpy supportedChangedSpy(dpms, &Clt::Dpms::supportedChanged);
    QVERIFY(supportedChangedSpy.isValid());
    m_serverOutput->setDpmsSupported(false);
    QVERIFY(supportedChangedSpy.wait());
    QCOMPARE(supportedChangedSpy.count(), 1);
    QVERIFY(!dpms->isSupported());
    m_serverOutput->setDpmsSupported(true);
    QVERIFY(supportedChangedSpy.wait());
    QCOMPARE(supportedChangedSpy.count(), 2);
    QVERIFY(dpms->isSupported());

    // and switch back to off
    m_serverOutput->setDpmsMode(Srv::WlOutput::DpmsMode::Off);
    QVERIFY(clientDpmsModeChangedSpy.wait());
    QCOMPARE(clientDpmsModeChangedSpy.count(), 2);
    QCOMPARE(dpms->mode(), Clt::Dpms::Mode::Off);
}

void TestOutput::testDpmsRequestMode_data()
{
    QTest::addColumn<Clt::Dpms::Mode>("client");
    QTest::addColumn<Srv::WlOutput::DpmsMode>("server");

    QTest::newRow("Standby") << Clt::Dpms::Mode::Standby << Srv::WlOutput::DpmsMode::Standby;
    QTest::newRow("Suspend") << Clt::Dpms::Mode::Suspend << Srv::WlOutput::DpmsMode::Suspend;
    QTest::newRow("Off") << Clt::Dpms::Mode::Off << Srv::WlOutput::DpmsMode::Off;
    QTest::newRow("On") << Clt::Dpms::Mode::On << Srv::WlOutput::DpmsMode::On;
}

void TestOutput::testDpmsRequestMode()
{
    // This test verifies that requesting a dpms change from client side emits the signal on
    // server side.

    // Setup code
    std::unique_ptr<Srv::DpmsManager> serverDpmsManager{m_display->createDpmsManager()};

    // set Dpms on the Output
    QSignalSpy serverDpmsSupportedChangedSpy(m_serverOutput, &Srv::WlOutput::dpmsSupportedChanged);
    QVERIFY(serverDpmsSupportedChangedSpy.isValid());
    QCOMPARE(m_serverOutput->isDpmsSupported(), false);
    m_serverOutput->setDpmsSupported(true);
    QCOMPARE(serverDpmsSupportedChangedSpy.count(), 1);
    QCOMPARE(m_serverOutput->isDpmsSupported(), true);

    Clt::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy announced(&registry, &Clt::Registry::interfacesAnnounced);
    QVERIFY(announced.isValid());
    QSignalSpy dpmsAnnouncedSpy(&registry, &Clt::Registry::dpmsAnnounced);
    QVERIFY(dpmsAnnouncedSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    m_connection->flush();
    QVERIFY(announced.wait());
    QCOMPARE(dpmsAnnouncedSpy.count(), 1);

    auto output
        = registry.createOutput(registry.interface(Clt::Registry::Interface::Output).name,
                                registry.interface(Clt::Registry::Interface::Output).version,
                                &registry);

    auto* dpmsManager
        = registry.createDpmsManager(dpmsAnnouncedSpy.first().first().value<quint32>(),
                                     dpmsAnnouncedSpy.first().last().value<quint32>(),
                                     &registry);
    QVERIFY(dpmsManager->isValid());

    auto* dpms = dpmsManager->getDpms(output, &registry);
    // and test request mode
    QSignalSpy modeRequestedSpy(m_serverOutput, &Srv::WlOutput::dpmsModeRequested);
    QVERIFY(modeRequestedSpy.isValid());

    QFETCH(Clt::Dpms::Mode, client);
    dpms->requestMode(client);
    QVERIFY(modeRequestedSpy.wait());
    QTEST(modeRequestedSpy.last().first().value<Srv::WlOutput::DpmsMode>(), "server");
}

QTEST_GUILESS_MAIN(TestOutput)
#include "output.moc"
