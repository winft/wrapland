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
#include "../../server/output_manager.h"

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

    void testSubpixel_data();
    void testSubpixel();

    void testTransform_data();
    void testTransform();

    void testDpms_data();
    void testDpms();

    void testDpmsRequestMode_data();
    void testDpmsRequestMode();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        std::unique_ptr<Wrapland::Server::output> output;
        std::unique_ptr<Wrapland::Server::output_manager> output_manager;
    } server;

    Clt::ConnectionThread* m_connection;
    Clt::EventQueue* m_queue;
    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-output-0"};

TestOutput::TestOutput(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_thread(nullptr)
{
}

void TestOutput::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    server.output_manager = std::make_unique<Wrapland::Server::output_manager>(*server.display);
    Srv::output_metadata meta{.name = "HDMI-A", .make = "Foocorp", .model = "Barmodel"};
    server.output = std::make_unique<Wrapland::Server::output>(meta, *server.output_manager);

    QCOMPARE(server.output->mode_size(), QSize());
    QCOMPARE(server.output->refresh_rate(), 60000);
    server.output->add_mode(Srv::output_mode{QSize(800, 600), 50000, true});
    QCOMPARE(server.output->mode_size(), QSize(800, 600));

    auto mode = Srv::output_mode{QSize(1024, 768)};
    server.output->add_mode(mode);

    server.output->add_mode(Srv::output_mode{QSize(1280, 1024), 90000});
    QCOMPARE(server.output->mode_size(), QSize(1280, 1024));

    server.output->set_mode(mode);
    QCOMPARE(server.output->mode_size(), QSize(1024, 768));
    QCOMPARE(server.output->refresh_rate(), 60000);

    QCOMPARE(server.output->dpms_supported(), false);
    QCOMPARE(server.output->dpms_mode(), Srv::output_dpms_mode::off);
    server.output->set_enabled(true);
    server.output->done();

    // setup connection
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(QString::fromStdString(socket_name));

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

    server = {};
}

void TestOutput::testRegistry()
{
    QCOMPARE(server.output->geometry().topLeft(), QPoint(0, 0));
    server.output->set_geometry(QRectF(QPoint(100, 50), QSize()));
    QCOMPARE(server.output->geometry().topLeft(), QPoint(100, 50));

    auto metadata = server.output->get_metadata();
    QCOMPARE(metadata.physical_size, QSize());

    metadata.physical_size = {200, 100};
    server.output->set_metadata(metadata);
    QCOMPARE(server.output->get_metadata().physical_size, QSize(200, 100));
    server.output->done();

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
    QCOMPARE(output.manufacturer(), QStringLiteral("Foocorp"));
    QCOMPARE(output.model(), QStringLiteral("Barmodel"));
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
    auto const serverModes = server.output->modes();
    QCOMPARE(serverModes.size(), 3);
    QCOMPARE(serverModes.at(0).size, QSize(800, 600));
    QCOMPARE(serverModes.at(1).size, QSize(1024, 768));
    QCOMPARE(serverModes.at(2).size, QSize(1280, 1024));
    QCOMPARE(serverModes.at(0).refresh_rate, 50000);
    QCOMPARE(serverModes.at(1).refresh_rate, 60000);
    QCOMPARE(serverModes.at(2).refresh_rate, 90000);
    QVERIFY(serverModes.at(0).preferred);
    QCOMPARE(serverModes.at(1).id, server.output->mode_id());
    QVERIFY(!serverModes.at(2).preferred);

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
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().refreshRate, 50000);
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
    QList<Output::Mode> const& modes = output.modes();
    QCOMPARE(modes.size(), 3);
    QCOMPARE(modes.at(0), modeAddedSpy.at(0).first().value<Output::Mode>());
    QCOMPARE(modes.at(1), modeAddedSpy.at(1).first().value<Output::Mode>());
    QCOMPARE(modes.at(2), modeAddedSpy.at(2).first().value<Output::Mode>());

    QCOMPARE(output.pixelSize(), QSize(1024, 768));

    // change the current mode
    outputChanged.clear();
    QSignalSpy modeChangedSpy(&output, &Clt::Output::modeChanged);
    QVERIFY(modeChangedSpy.isValid());

    QCOMPARE(server.output->mode_size(), QSize(1024, 768));

    // Setting a non-existing mode.
    QVERIFY(!server.output->set_mode(Srv::output_mode{QSize(800, 600)}));
    QCOMPARE(server.output->mode_size(), QSize(1024, 768));

    QVERIFY(server.output->set_mode(Srv::output_mode{QSize(800, 600), 50000}));
    QCOMPARE(server.output->mode_size(), QSize(800, 600));
    server.output->done();

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
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().refreshRate, 50000);
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().flags,
             Output::Mode::Flags(Output::Mode::Flag::Current | Output::Mode::Flag::Preferred));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(800, 600));
    QList<Output::Mode> const& modes2 = output.modes();
    QCOMPARE(modes2.at(0).size, QSize(1280, 1024));
    QCOMPARE(modes2.at(0).refreshRate, 90000);
    QCOMPARE(modes2.at(0).flags, Output::Mode::Flag::None);
    QCOMPARE(modes2.at(1).size, QSize(1024, 768));
    QCOMPARE(modes2.at(1).refreshRate, 60000);
    QCOMPARE(modes2.at(1).flags, Output::Mode::Flag::None);
    QCOMPARE(modes2.at(2).size, QSize(800, 600));
    QCOMPARE(modes2.at(2).refreshRate, 50000);
    QCOMPARE(modes2.at(2).flags, Output::Mode::Flag::Current | Output::Mode::Flag::Preferred);

    // change once more
    outputChanged.clear();
    modeChangedSpy.clear();
    server.output->set_mode(Srv::output_mode{QSize(1280, 1024), 90000});
    QCOMPARE(server.output->refresh_rate(), 90000);
    server.output->done();

    QVERIFY(modeChangedSpy.wait());
    if (modeChangedSpy.size() == 1) {
        QVERIFY(modeChangedSpy.wait());
    }
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().refreshRate, 50000);
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
    server.output->set_mode(Srv::output_mode{QSize(1280, 1024), 90000});
    server.output->set_geometry(QRectF(QPoint(0, 0), QSize(1280, 1024)));
    QCOMPARE(server.output->client_scale(), 1);
    server.output->set_geometry(QRectF(QPoint(0, 0), QSize(640, 512)));
    QCOMPARE(server.output->client_scale(), 2);
    server.output->done();

    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 2);

    // changing to same value should not trigger
    server.output->set_geometry(QRectF(QPoint(0, 0), QSize(640, 512)));
    QCOMPARE(server.output->client_scale(), 2);
    server.output->done();
    QVERIFY(!outputChanged.wait(500));
    QCOMPARE(output.scale(), 2);

    // changing to a different value with same scale should not trigger
    server.output->set_geometry(QRectF(QPoint(0, 0), QSize(800, 600)));
    QCOMPARE(server.output->client_scale(), 2);
    server.output->done();
    QVERIFY(!outputChanged.wait(500));
    QCOMPARE(output.scale(), 2);

    // change once more
    outputChanged.clear();
    QVERIFY(server.output->set_mode(Srv::output_mode{QSize(800, 600), 50000}));
    server.output->done();
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 1);

    // change once more
    outputChanged.clear();
    QVERIFY(server.output->set_mode(Srv::output_mode{QSize(1280, 1024), 90000}));
    server.output->set_geometry(QRectF(QPoint(100, 200), QSize(1280, 1025)));
    server.output->done();
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 1);
}

void TestOutput::testSubpixel_data()
{
    QTest::addColumn<Clt::Output::SubPixel>("expected");
    QTest::addColumn<Srv::output_subpixel>("actual");

    QTest::newRow("none") << Clt::Output::SubPixel::None << Srv::output_subpixel::none;
    QTest::newRow("horizontal/rgb")
        << Clt::Output::SubPixel::HorizontalRGB << Srv::output_subpixel::horizontal_rgb;
    QTest::newRow("horizontal/bgr")
        << Clt::Output::SubPixel::HorizontalBGR << Srv::output_subpixel::horizontal_bgr;
    QTest::newRow("vertical/rgb") << Clt::Output::SubPixel::VerticalRGB
                                  << Srv::output_subpixel::vertical_rgb;
    QTest::newRow("vertical/bgr") << Clt::Output::SubPixel::VerticalBGR
                                  << Srv::output_subpixel::vertical_bgr;
}

void TestOutput::testSubpixel()
{
    QFETCH(Srv::output_subpixel, actual);

    QCOMPARE(server.output->subpixel(), Srv::output_subpixel::unknown);
    server.output->set_subpixel(actual);
    QCOMPARE(server.output->subpixel(), actual);
    server.output->done();

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
    server.output->set_subpixel(Srv::output_subpixel::unknown);
    QCOMPARE(server.output->subpixel(), Srv::output_subpixel::unknown);
    server.output->done();

    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output.subPixel(), Clt::Output::SubPixel::Unknown);
}

void TestOutput::testTransform_data()
{
    QTest::addColumn<Clt::Output::Transform>("expected");
    QTest::addColumn<Srv::output_transform>("actual");

    QTest::newRow("90") << Clt::Output::Transform::Rotated90 << Srv::output_transform::rotated_90;
    QTest::newRow("180") << Clt::Output::Transform::Rotated180
                         << Srv::output_transform::rotated_180;
    QTest::newRow("270") << Clt::Output::Transform::Rotated270
                         << Srv::output_transform::rotated_270;
    QTest::newRow("Flipped") << Clt::Output::Transform::Flipped << Srv::output_transform::flipped;
    QTest::newRow("Flipped 90") << Clt::Output::Transform::Flipped90
                                << Srv::output_transform::flipped_90;
    QTest::newRow("Flipped 180") << Clt::Output::Transform::Flipped180
                                 << Srv::output_transform::flipped_180;
    QTest::newRow("Flipped 280") << Clt::Output::Transform::Flipped270
                                 << Srv::output_transform::flipped_270;
}

void TestOutput::testTransform()
{
    QFETCH(Srv::output_transform, actual);
    QCOMPARE(server.output->transform(), Srv::output_transform::normal);
    server.output->set_transform(actual);
    QCOMPARE(server.output->transform(), actual);
    server.output->done();

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
    server.output->set_transform(Srv::output_transform::normal);
    QCOMPARE(server.output->transform(), Srv::output_transform::normal);
    server.output->done();

    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output->transform(), Clt::Output::Transform::Normal);
}

void TestOutput::testDpms_data()
{
    QTest::addColumn<Clt::Dpms::Mode>("client_mode");
    QTest::addColumn<Srv::output_dpms_mode>("server_mode");

    QTest::newRow("Standby") << Clt::Dpms::Mode::Standby << Srv::output_dpms_mode::standby;
    QTest::newRow("Suspend") << Clt::Dpms::Mode::Suspend << Srv::output_dpms_mode::suspend;
    QTest::newRow("On") << Clt::Dpms::Mode::On << Srv::output_dpms_mode::on;
}

void TestOutput::testDpms()
{
    auto serverDpmsManager = std::make_unique<Wrapland::Server::DpmsManager>(server.display.get());

    // set Dpms on the Output
    QSignalSpy serverDpmsSupportedChangedSpy(server.output.get(),
                                             &Srv::output::dpms_supported_changed);
    QVERIFY(serverDpmsSupportedChangedSpy.isValid());
    QCOMPARE(server.output->dpms_supported(), false);
    server.output->set_dpms_supported(true);
    QCOMPARE(server.output->dpms_supported(), true);

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
    QSignalSpy serverDpmsModeChangedSpy(server.output.get(), &Srv::output::dpms_mode_changed);
    QVERIFY(serverDpmsModeChangedSpy.isValid());
    QSignalSpy clientDpmsModeChangedSpy(dpms, &Clt::Dpms::modeChanged);
    QVERIFY(clientDpmsModeChangedSpy.isValid());

    QCOMPARE(server.output->dpms_mode(), Srv::output_dpms_mode::off);
    QFETCH(Srv::output_dpms_mode, server_mode);
    server.output->set_dpms_mode(server_mode);
    QCOMPARE(server.output->dpms_mode(), server_mode);
    QCOMPARE(serverDpmsModeChangedSpy.count(), 1);

    QVERIFY(clientDpmsModeChangedSpy.wait());
    QCOMPARE(clientDpmsModeChangedSpy.count(), 1);
    QTEST(dpms->mode(), "client_mode");

    // Test supported changed
    QSignalSpy supportedChangedSpy(dpms, &Clt::Dpms::supportedChanged);
    QVERIFY(supportedChangedSpy.isValid());
    server.output->set_dpms_supported(false);
    QVERIFY(supportedChangedSpy.wait());
    QCOMPARE(supportedChangedSpy.count(), 1);
    QVERIFY(!dpms->isSupported());
    server.output->set_dpms_supported(true);
    QVERIFY(supportedChangedSpy.wait());
    QCOMPARE(supportedChangedSpy.count(), 2);
    QVERIFY(dpms->isSupported());

    // and switch back to off
    server.output->set_dpms_mode(Srv::output_dpms_mode::off);
    QVERIFY(clientDpmsModeChangedSpy.wait());
    QCOMPARE(clientDpmsModeChangedSpy.count(), 2);
    QCOMPARE(dpms->mode(), Clt::Dpms::Mode::Off);
}

void TestOutput::testDpmsRequestMode_data()
{
    QTest::addColumn<Clt::Dpms::Mode>("client_mode");
    QTest::addColumn<Srv::output_dpms_mode>("server_mode");

    QTest::newRow("Standby") << Clt::Dpms::Mode::Standby << Srv::output_dpms_mode::standby;
    QTest::newRow("Suspend") << Clt::Dpms::Mode::Suspend << Srv::output_dpms_mode::suspend;
    QTest::newRow("Off") << Clt::Dpms::Mode::Off << Srv::output_dpms_mode::off;
    QTest::newRow("On") << Clt::Dpms::Mode::On << Srv::output_dpms_mode::on;
}

void TestOutput::testDpmsRequestMode()
{
    // This test verifies that requesting a dpms change from client side emits the signal on
    // server side.

    // Setup code
    auto serverDpmsManager = std::make_unique<Wrapland::Server::DpmsManager>(server.display.get());

    // set Dpms on the Output
    QSignalSpy serverDpmsSupportedChangedSpy(server.output.get(),
                                             &Srv::output::dpms_supported_changed);
    QVERIFY(serverDpmsSupportedChangedSpy.isValid());
    QCOMPARE(server.output->dpms_supported(), false);
    server.output->set_dpms_supported(true);
    QCOMPARE(serverDpmsSupportedChangedSpy.count(), 1);
    QCOMPARE(server.output->dpms_supported(), true);

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
    QSignalSpy modeRequestedSpy(server.output.get(), &Srv::output::dpms_mode_requested);
    QVERIFY(modeRequestedSpy.isValid());

    QFETCH(Clt::Dpms::Mode, client_mode);
    dpms->requestMode(client_mode);
    QVERIFY(modeRequestedSpy.wait());
    QTEST(modeRequestedSpy.last().first().value<Srv::output_dpms_mode>(), "server_mode");
}

QTEST_GUILESS_MAIN(TestOutput)
#include "output.moc"
