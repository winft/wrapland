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
#include "../../server/wlr_output_configuration_head_v1.h"
#include "../../server/wlr_output_configuration_v1.h"
#include "../../server/wlr_output_manager_v1.h"

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

    void test_add_remove_output();
    void test_properties();
    void test_configuration();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        std::vector<Srv::output_mode> modes;
    } server;

    struct {
        Clt::ConnectionThread* connection{nullptr};
        Clt::EventQueue* queue{nullptr};
        QThread* thread{nullptr};

        Clt::Registry* registry{nullptr};
        Clt::WlrOutputManagerV1* output_manager{nullptr};
        std::vector<Clt::WlrOutputHeadV1*> heads;

        std::unique_ptr<QSignalSpy> wl_output_spy;
        std::unique_ptr<QSignalSpy> wlr_head_spy;
        std::unique_ptr<QSignalSpy> wlr_done_spy;
    } client;
};

constexpr auto socket_name{"wrapland-test-output-0"};

TestWlrOutputManagement::TestWlrOutputManagement(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<Srv::wlr_output_configuration_v1*>();
}

void TestWlrOutputManagement::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.globals.output_manager
        = std::make_unique<Wrapland::Server::output_manager>(*server.display);
    server.globals.output_manager->create_wlr_manager_v1();

    // setup connection
    client.connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(client.connection, &Clt::ConnectionThread::establishedChanged);
    client.connection->setSocketName(socket_name);

    client.thread = new QThread(this);
    client.connection->moveToThread(client.thread);
    client.thread->start();

    client.connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    client.queue = new Clt::EventQueue(this);
    QVERIFY(!client.queue->isValid());
    client.queue->setup(client.connection);
    QVERIFY(client.queue->isValid());

    client.registry = new Clt::Registry();

    QSignalSpy output_manager_spy(client.registry, &Clt::Registry::wlrOutputManagerV1Announced);
    QVERIFY(output_manager_spy.isValid());
    client.wl_output_spy
        = std::make_unique<QSignalSpy>(client.registry, &Clt::Registry::outputAnnounced);
    QVERIFY(client.wl_output_spy->isValid());

    client.registry->create(client.connection->display());
    QVERIFY(client.registry->isValid());
    client.registry->setEventQueue(client.queue);
    client.registry->setup();
    wl_display_flush(client.connection->display());

    QVERIFY(output_manager_spy.wait());
    client.output_manager = client.registry->createWlrOutputManagerV1(
        output_manager_spy.first().first().value<quint32>(),
        output_manager_spy.first().last().value<quint32>(),
        this);
    client.wlr_head_spy
        = std::make_unique<QSignalSpy>(client.output_manager, &Clt::WlrOutputManagerV1::head);
    QVERIFY(client.wlr_head_spy->isValid());
    client.wlr_done_spy
        = std::make_unique<QSignalSpy>(client.output_manager, &Clt::WlrOutputManagerV1::done);
    QVERIFY(client.wlr_done_spy->isValid());

    auto first_output = std::make_unique<Wrapland::Server::output>(*server.globals.output_manager);

    Srv::output_mode m0;
    m0.id = 0;
    m0.size = QSize(800, 600);
    m0.preferred = true;
    first_output->add_mode(m0);

    Srv::output_mode m1;
    m1.id = 1;
    m1.size = QSize(1024, 768);
    first_output->add_mode(m1);

    Srv::output_mode m2;
    m2.id = 2;
    m2.size = QSize(1280, 1024);
    m2.refresh_rate = 90000;
    first_output->add_mode(m2);

    Srv::output_mode m3;
    m3.id = 3;
    m3.size = QSize(1920, 1080);
    m3.refresh_rate = 100000;
    first_output->add_mode(m3);

    server.modes = {m0, m1, m2, m3};

    auto state = first_output->get_state();
    state.mode = m1;
    state.geometry = QRectF(QPointF(0, 1920), QSizeF(1024, 768));
    state.adaptive_sync = true;
    first_output->set_state(state);
    server.globals.outputs.push_back(std::move(first_output));
    server.globals.output_manager->commit_changes();

    QVERIFY(client.wlr_head_spy->wait());
}

void TestWlrOutputManagement::cleanup()
{
    client.heads.clear();
    if (client.output_manager) {
        delete client.output_manager;
        client.output_manager = nullptr;
    }

    if (client.registry) {
        delete client.registry;
        client.registry = nullptr;
    }
    if (client.queue) {
        delete client.queue;
        client.queue = nullptr;
    }
    if (client.connection) {
        client.connection->deleteLater();
        client.connection = nullptr;
    }
    if (client.thread) {
        client.thread->quit();
        client.thread->wait();
        delete client.thread;
        client.thread = nullptr;
    }

    server = {};
}

void TestWlrOutputManagement::test_add_remove_output()
{
    auto wlr_head1 = client.wlr_head_spy->back().front().value<Clt::WlrOutputHeadV1*>();
    QVERIFY(wlr_head1);

    QSignalSpy head_changed_spy(wlr_head1, &Clt::WlrOutputHeadV1::changed);
    QVERIFY(head_changed_spy.isValid());

    // The first output is not yet enabled. Do this now, so a wl_output is created.
    auto state = server.globals.outputs.front()->get_state();
    state.enabled = true;
    server.globals.outputs.front()->set_state(state);
    server.globals.output_manager->commit_changes();

    QVERIFY(client.wlr_done_spy->wait());
    QCOMPARE(client.wl_output_spy->size(), 1);

    // The wlr head receives the changed properties. Client-side no atomic changes at the moment.
    QTRY_COMPARE(head_changed_spy.size(), 6);

    // Add one more output which is already enabled.
    server.globals.outputs.emplace_back(
        std::make_unique<Wrapland::Server::output>(*server.globals.output_manager));
    auto server_output = server.globals.outputs.back().get();

    server_output->add_mode(server.modes.at(0));
    server_output->add_mode(server.modes.at(1));

    state = server_output->get_state();
    state.mode = server.modes.at(0);
    state.enabled = true;
    state.geometry = QRectF(QPointF(1920, 1920), QSizeF(800, 600));
    server_output->set_state(state);
    server.globals.output_manager->commit_changes();

    QVERIFY(client.wlr_done_spy->wait());
    QCOMPARE(client.wlr_head_spy->size(), 2);
    QCOMPARE(client.wl_output_spy->size(), 2);

    auto wlr_head2 = client.wlr_head_spy->back().front().value<Clt::WlrOutputHeadV1*>();
    QVERIFY(wlr_head2);
    QVERIFY(wlr_head1 != wlr_head2);
    QVERIFY(wlr_head2->enabled());

    // Remove first output.
    QSignalSpy head_removed_spy(wlr_head1, &Clt::WlrOutputHeadV1::removed);
    QVERIFY(head_removed_spy.isValid());

    server.globals.outputs.pop_front();

    // The finished signal is sent before commit_changes is called on the manager.
    // TODO(romangg): Should we instead ensure that this happens together?
    QVERIFY(head_removed_spy.wait());
    server.globals.output_manager->commit_changes();
    QVERIFY(client.wlr_done_spy->wait());

    server.globals.outputs.pop_back();
    QVERIFY(server.globals.outputs.empty());
    server.globals.output_manager->commit_changes();
    QVERIFY(client.wlr_done_spy->wait());
}

void TestWlrOutputManagement::test_properties()
{
    auto wlr_head1 = client.wlr_head_spy->back().front().value<Clt::WlrOutputHeadV1*>();
    QVERIFY(wlr_head1);

    QCOMPARE(wlr_head1->name(), "Unknown");
    QCOMPARE(wlr_head1->description(), "Unknown");
    QCOMPARE(wlr_head1->make(), "");
    QCOMPARE(wlr_head1->model(), "");
    QCOMPARE(wlr_head1->serialNumber(), "");
    QVERIFY(!wlr_head1->enabled());
    QCOMPARE(wlr_head1->modes().size(), 4);
    QVERIFY(!wlr_head1->currentMode());
    QCOMPARE(wlr_head1->position(), QPoint(0, 0));
    QCOMPARE(wlr_head1->transform(), Clt::WlrOutputHeadV1::Transform::Normal);
    QCOMPARE(wlr_head1->scale(), 1);
    QVERIFY(!wlr_head1->adaptive_sync());

    // The first output is not yet enabled. Do this now. All data is sent.
    auto state = server.globals.outputs.front()->get_state();
    state.enabled = true;
    server.globals.outputs.front()->set_state(state);
    server.globals.output_manager->commit_changes();

    QVERIFY(client.wlr_done_spy->wait());
    QCOMPARE(client.wl_output_spy->size(), 1);

    QCOMPARE(wlr_head1->name(), "Unknown");
    QCOMPARE(wlr_head1->description(), "Unknown");
    QCOMPARE(wlr_head1->make(), "");
    QCOMPARE(wlr_head1->model(), "");
    QCOMPARE(wlr_head1->serialNumber(), "");
    QVERIFY(wlr_head1->enabled());
    QCOMPARE(wlr_head1->modes().size(), 4);
    QVERIFY(wlr_head1->currentMode());
    QCOMPARE(wlr_head1->currentMode()->size(), server.modes.at(1).size);
    QCOMPARE(wlr_head1->currentMode()->refresh(), server.modes.at(1).refresh_rate);
    QVERIFY(!wlr_head1->currentMode()->preferred());
    QCOMPARE(wlr_head1->position(), QPoint(0, 1920));
    QCOMPARE(wlr_head1->transform(), Clt::WlrOutputHeadV1::Transform::Normal);
    QCOMPARE(wlr_head1->scale(), 1);
    QVERIFY(wlr_head1->adaptive_sync());

    // Add one more output which is already enabled.
    Srv::output_metadata meta{
        .name = "HDMI-A", .make = "Foocorp", .model = "Barmodel", .serial_number = "1234"};
    server.globals.outputs.emplace_back(
        std::make_unique<Wrapland::Server::output>(meta, *server.globals.output_manager));
    auto server_output = server.globals.outputs.back().get();

    server_output->add_mode(server.modes.at(0));
    server_output->add_mode(server.modes.at(1));

    state = server_output->get_state();
    state.mode = server.modes.at(0);
    state.enabled = true;
    state.geometry = QRectF(QPointF(1920, 1920), QSizeF(400, 300));
    state.transform = Srv::output_transform::flipped_180;
    state.adaptive_sync = true;
    server_output->set_state(state);
    server.globals.output_manager->commit_changes();

    QVERIFY(client.wlr_done_spy->wait());
    QCOMPARE(client.wlr_head_spy->size(), 2);
    QCOMPARE(client.wl_output_spy->size(), 2);

    auto wlr_head2 = client.wlr_head_spy->back().front().value<Clt::WlrOutputHeadV1*>();
    QVERIFY(wlr_head2);
    QVERIFY(wlr_head1 != wlr_head2);

    QCOMPARE(wlr_head2->name(), meta.name.c_str());
    QCOMPARE(wlr_head2->description(), "Foocorp Barmodel (HDMI-A)");
    QCOMPARE(wlr_head2->make(), meta.make.c_str());
    QCOMPARE(wlr_head2->model(), meta.model.c_str());
    QCOMPARE(wlr_head2->serialNumber(), meta.serial_number.c_str());
    QVERIFY(wlr_head2->enabled());
    QCOMPARE(wlr_head2->modes().size(), 2);
    QVERIFY(wlr_head2->currentMode());
    QCOMPARE(wlr_head2->currentMode()->size(), server.modes.at(0).size);
    QCOMPARE(wlr_head2->currentMode()->refresh(), server.modes.at(0).refresh_rate);
    QVERIFY(wlr_head2->currentMode()->preferred());
    QCOMPARE(wlr_head2->position(), QPoint(1920, 1920));
    QCOMPARE(wlr_head2->transform(), Clt::WlrOutputHeadV1::Transform::Flipped180);
    QCOMPARE(wlr_head2->scale(), 1);
    QCOMPARE(wlr_head2->viewport(), QSize(400, 300));
    QVERIFY(wlr_head2->adaptive_sync());
}

void TestWlrOutputManagement::test_configuration()
{
    auto wlr_head1 = client.wlr_head_spy->back().front().value<Clt::WlrOutputHeadV1*>();
    QVERIFY(wlr_head1);

    // The first output is not yet enabled. Do this now. All data is sent.
    auto state = server.globals.outputs.front()->get_state();
    state.enabled = true;
    server.globals.outputs.front()->set_state(state);
    server.globals.output_manager->commit_changes();

    QVERIFY(client.wlr_done_spy->wait());
    QCOMPARE(client.wl_output_spy->size(), 1);
    QVERIFY(wlr_head1->enabled());

    // Add one more output which is already enabled.
    Srv::output_metadata meta{
        .name = "HDMI-A", .make = "Foocorp", .model = "Barmodel", .serial_number = "1234"};
    server.globals.outputs.emplace_back(
        std::make_unique<Wrapland::Server::output>(meta, *server.globals.output_manager));
    auto server_output = server.globals.outputs.back().get();

    server_output->add_mode(server.modes.at(0));
    server_output->add_mode(server.modes.at(1));

    state = server_output->get_state();
    state.mode = server.modes.at(0);
    state.enabled = true;
    state.geometry = QRectF(QPointF(1920, 1920), QSizeF(400, 300));
    state.transform = Srv::output_transform::flipped_180;
    server_output->set_state(state);
    server.globals.output_manager->commit_changes();

    QVERIFY(client.wlr_done_spy->wait());
    QCOMPARE(client.wlr_head_spy->size(), 2);
    QCOMPARE(client.wl_output_spy->size(), 2);

    auto wlr_head2 = client.wlr_head_spy->back().front().value<Clt::WlrOutputHeadV1*>();
    QVERIFY(wlr_head2);
    QVERIFY(wlr_head1 != wlr_head2);
    QVERIFY(wlr_head2->enabled());
    QCOMPARE(wlr_head2->modes().size(), 2);
    QVERIFY(wlr_head2->currentMode());
    QCOMPARE(wlr_head2->currentMode(), wlr_head2->modes().at(0));
    QCOMPARE(wlr_head2->currentMode()->size(), server.modes.at(0).size);
    QCOMPARE(wlr_head2->currentMode()->refresh(), server.modes.at(0).refresh_rate);
    QVERIFY(wlr_head2->currentMode()->preferred());
    QCOMPARE(wlr_head2->position(), QPoint(1920, 1920));
    QCOMPARE(wlr_head2->transform(), Clt::WlrOutputHeadV1::Transform::Flipped180);
    QCOMPARE(wlr_head2->scale(), 1);
    QCOMPARE(wlr_head2->viewport(), QSize(400, 300));
    QVERIFY(!wlr_head2->adaptive_sync());

    auto config = std::unique_ptr<Clt::WlrOutputConfigurationV1>{
        client.output_manager->createConfiguration()};
    QVERIFY(config->isValid());

    config->setEnabled(wlr_head1, false);
    config->setEnabled(wlr_head2, true);

    config->setMode(wlr_head2, wlr_head2->modes().at(1));
    config->setPosition(wlr_head2, QPoint(0, 0));
    config->setTransform(wlr_head2, Clt::WlrOutputHeadV1::Transform::Rotated90);
    config->set_viewport(wlr_head2, QSize(768, 1024));
    config->set_adaptive_sync(wlr_head2, true);
    config->apply();

    QSignalSpy config_apply_spy(server.globals.output_manager->wlr_manager_v1.get(),
                                &Srv::wlr_output_manager_v1::apply_config);
    QVERIFY(config_apply_spy.isValid());
    QVERIFY(config_apply_spy.wait());

    auto server_config = config_apply_spy.back().front().value<Srv::wlr_output_configuration_v1*>();
    QVERIFY(server_config);

    auto enabled_heads = server_config->enabled_heads();
    QCOMPARE(enabled_heads.size(), 1);

    auto config_head = enabled_heads.at(0);
    QCOMPARE(&config_head->get_output(), server.globals.outputs.at(1).get());
    QCOMPARE(config_head->get_state().mode, server.modes.at(1));
    QCOMPARE(config_head->get_state().geometry, QRect(QPoint(0, 0), QSize(768, 1024)));
    QCOMPARE(config_head->get_state().transform, Srv::output_transform::rotated_90);
    QVERIFY(config_head->get_state().adaptive_sync);

    server_config->send_succeeded();

    QSignalSpy config_succeeded_spy(config.get(), &Clt::WlrOutputConfigurationV1::succeeded);
    QVERIFY(config_succeeded_spy.isValid());
    QVERIFY(config_succeeded_spy.wait());
}

QTEST_GUILESS_MAIN(TestWlrOutputManagement)
#include "wlr_output_management.moc"
