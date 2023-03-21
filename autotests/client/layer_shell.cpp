/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/layer_shell_v1.h"
#include "../../src/client/output.h"
#include "../../src/client/registry.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/surface.h"
#include "../../src/client/xdg_shell.h"

#include "../../server/buffer.h"
#include "../../server/client.h"
#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/layer_shell_v1.h"
#include "../../server/surface.h"
#include "../../server/xdg_shell.h"

#include "../../tests/globals.h"

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

class layer_shell_test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void test_create_layer_surface();
    void test_data_transfer();

    void test_exclusive_edge_data();
    void test_exclusive_edge();

    void test_margin();
    void test_xdg_popup();
    void test_output_removal();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        std::unique_ptr<Wrapland::Server::output> output;
        Wrapland::Server::globals globals;
    } server;

    QThread* m_thread{nullptr};
    Clt::ConnectionThread* connection{nullptr};
    Clt::EventQueue* queue{nullptr};
    Clt::ShmPool* shm{nullptr};
    Clt::Compositor* compositor{nullptr};
    Clt::LayerShellV1* layer_shell{nullptr};
    Clt::XdgShell* xdg_shell{nullptr};
    Clt::Output* output{nullptr};
};

constexpr auto socket_name{"wrapland-test-layer-shell-0"};

void layer_shell_test::init()
{
    qRegisterMetaType<Srv::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(std::string(socket_name));
    server.display->start();
    QVERIFY(server.display->running());

    server.globals.output_manager
        = std::make_unique<Wrapland::Server::output_manager>(*server.display);
    server.display->createShm();
    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());
    server.globals.layer_shell_v1
        = std::make_unique<Wrapland::Server::LayerShellV1>(server.display.get());
    server.globals.xdg_shell = std::make_unique<Wrapland::Server::XdgShell>(server.display.get());

    server.output = std::make_unique<Wrapland::Server::output>(*server.globals.output_manager);
    server.output->set_enabled(true);
    server.output->done();

    // setup connection
    connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    connection->moveToThread(m_thread);
    m_thread->start();

    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    queue = new Clt::EventQueue(this);
    queue->setup(connection);

    Clt::Registry registry;
    QSignalSpy output_announced_spy(&registry, &Clt::Registry::outputAnnounced);
    QVERIFY(output_announced_spy.isValid());
    QSignalSpy interfaces_announced_spy(&registry, &Clt::Registry::interfacesAnnounced);
    QVERIFY(interfaces_announced_spy.isValid());

    registry.setEventQueue(queue);
    registry.create(connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfaces_announced_spy.wait());

    shm = registry.createShmPool(registry.interface(Clt::Registry::Interface::Shm).name,
                                 registry.interface(Clt::Registry::Interface::Shm).version,
                                 this);
    QVERIFY(shm->isValid());
    compositor = registry.createCompositor(
        registry.interface(Clt::Registry::Interface::Compositor).name,
        registry.interface(Clt::Registry::Interface::Compositor).version,
        this);
    QVERIFY(compositor->isValid());
    layer_shell = registry.createLayerShellV1(
        registry.interface(Clt::Registry::Interface::LayerShellV1).name,
        registry.interface(Clt::Registry::Interface::LayerShellV1).version,
        this);
    QVERIFY(layer_shell->isValid());
    xdg_shell
        = registry.createXdgShell(registry.interface(Clt::Registry::Interface::XdgShell).name,
                                  registry.interface(Clt::Registry::Interface::XdgShell).version,
                                  this);
    QVERIFY(xdg_shell->isValid());
    output = registry.createOutput(output_announced_spy.first().first().value<quint32>(),
                                   output_announced_spy.first().last().value<quint32>(),
                                   this);
    QVERIFY(output->isValid());
}

void layer_shell_test::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(shm)
    CLEANUP(compositor)
    CLEANUP(layer_shell)
    CLEANUP(xdg_shell)
    CLEANUP(output)
    CLEANUP(queue)
    if (connection) {
        connection->deleteLater();
        connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
#undef CLEANUP

    server = {};
}

void layer_shell_test::test_create_layer_surface()
{
    // This test verifies that a layer surface can be created and communicates.
    QSignalSpy server_surface_spy(server.globals.compositor.get(),
                                  &Srv::Compositor::surfaceCreated);
    QVERIFY(server_surface_spy.isValid());

    std::unique_ptr<Clt::Surface> surface{compositor->createSurface()};
    QVERIFY(server_surface_spy.wait());
    auto server_surface = server_surface_spy.first().first().value<Srv::Surface*>();
    QVERIFY(server_surface);

    QSignalSpy layer_surface_spy(server.globals.layer_shell_v1.get(),
                                 &Srv::LayerShellV1::surface_created);
    QVERIFY(layer_surface_spy.isValid());

    std::unique_ptr<Clt::LayerSurfaceV1> layer_surface{layer_shell->get_layer_surface(
        surface.get(), nullptr, Clt::LayerShellV1::layer::background, "")};
    layer_surface->set_size(QSize(1, 1));

    QVERIFY(layer_surface_spy.wait());
    auto server_layer_surface = layer_surface_spy.first().first().value<Srv::LayerSurfaceV1*>();
    QVERIFY(server_layer_surface);
    QCOMPARE(server_layer_surface->surface(), server_surface);

    QSignalSpy commit_spy(server_surface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());

    // Now do a first commit according to protocol.
    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    QSignalSpy configure_spy(layer_surface.get(), &Clt::LayerSurfaceV1::configure_requested);
    QVERIFY(configure_spy.isValid());

    // Server replies with configure event.
    auto const size = QSize(100, 100);
    server_layer_surface->configure(size);
    QVERIFY(configure_spy.wait());
    QCOMPARE(configure_spy.first().first().value<QSize>(), size);

    auto const serial = configure_spy.first().last().value<quint32>();

    QSignalSpy ack_configure_spy(server_layer_surface,
                                 &Srv::LayerSurfaceV1::configure_acknowledged);
    QVERIFY(ack_configure_spy.isValid());

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);

    auto buf = shm->createBuffer(img);
    surface->attachBuffer(buf);
    surface->damage(QRect(QPoint(), size));
    layer_surface->ack_configure(serial);
    surface->commit(Clt::Surface::CommitFlag::None);

    QVERIFY(commit_spy.wait());
    QCOMPARE(ack_configure_spy.size(), 1);
    QCOMPARE(ack_configure_spy.first().first().value<quint32>(), serial);
}

void layer_shell_test::test_data_transfer()
{
    // Tests that basic data is set and received.
    QSignalSpy server_surface_spy(server.globals.compositor.get(),
                                  &Srv::Compositor::surfaceCreated);
    QVERIFY(server_surface_spy.isValid());

    std::unique_ptr<Clt::Surface> surface{compositor->createSurface()};
    QVERIFY(server_surface_spy.wait());
    auto server_surface = server_surface_spy.first().first().value<Srv::Surface*>();
    QVERIFY(server_surface);

    QSignalSpy layer_surface_spy(server.globals.layer_shell_v1.get(),
                                 &Srv::LayerShellV1::surface_created);
    QVERIFY(layer_surface_spy.isValid());

    std::unique_ptr<Clt::LayerSurfaceV1> layer_surface{layer_shell->get_layer_surface(
        surface.get(), output, Clt::LayerShellV1::layer::background, "")};

    QVERIFY(layer_surface_spy.wait());
    auto server_layer_surface = layer_surface_spy.first().first().value<Srv::LayerSurfaceV1*>();
    QVERIFY(server_layer_surface);
    QCOMPARE(server_layer_surface->surface(), server_surface);

    layer_surface->set_anchor(Qt::LeftEdge);
    layer_surface->set_exclusive_zone(10);
    layer_surface->set_keyboard_interactivity(Clt::LayerShellV1::keyboard_interactivity::on_demand);
    layer_surface->set_layer(Clt::LayerShellV1::layer::top);
    layer_surface->set_margin(QMargins(1, 0, 0, 0));
    layer_surface->set_size(QSize(100, 100));

    QSignalSpy commit_spy(server_surface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());

    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    QCOMPARE(server_layer_surface->output(), server.output.get());
    QCOMPARE(server_layer_surface->anchor(), Qt::LeftEdge);
    QCOMPARE(server_layer_surface->exclusive_zone(), 10);
    QCOMPARE(server_layer_surface->layer(), Srv::LayerSurfaceV1::Layer::Top);
    QCOMPARE(server_layer_surface->margins(), QMargins(1, 0, 0, 0));
    QCOMPARE(server_layer_surface->size(), QSize(100, 100));
}

void layer_shell_test::test_exclusive_edge_data()
{
    QTest::addColumn<Qt::Edges>("anchor");
    QTest::addColumn<Qt::Edge>("excl_anchor");
    QTest::addColumn<int>("zone");

    QTest::newRow("single") << Qt::Edges(Qt::LeftEdge) << Qt::LeftEdge << 1;
    QTest::newRow("horizontal") << (Qt::LeftEdge | Qt::RightEdge) << Qt::Edge() << 0;
    QTest::newRow("vertical") << (Qt::TopEdge | Qt::BottomEdge) << Qt::Edge() << 0;
    QTest::newRow("orthogonal") << (Qt::LeftEdge | Qt::BottomEdge) << Qt::Edge() << 0;
    QTest::newRow("3-way") << (Qt::LeftEdge | Qt::BottomEdge | Qt::RightEdge) << Qt::BottomEdge
                           << 1;
}

void layer_shell_test::test_exclusive_edge()
{
    // Tests that the exclusive edge is correctly set.
    QSignalSpy server_surface_spy(server.globals.compositor.get(),
                                  &Srv::Compositor::surfaceCreated);
    QVERIFY(server_surface_spy.isValid());

    std::unique_ptr<Clt::Surface> surface{compositor->createSurface()};
    QVERIFY(server_surface_spy.wait());
    auto server_surface = server_surface_spy.first().first().value<Srv::Surface*>();
    QVERIFY(server_surface);

    QSignalSpy layer_surface_spy(server.globals.layer_shell_v1.get(),
                                 &Srv::LayerShellV1::surface_created);
    QVERIFY(layer_surface_spy.isValid());

    std::unique_ptr<Clt::LayerSurfaceV1> layer_surface{layer_shell->get_layer_surface(
        surface.get(), nullptr, Clt::LayerShellV1::layer::background, "")};

    QVERIFY(layer_surface_spy.wait());
    auto server_layer_surface = layer_surface_spy.first().first().value<Srv::LayerSurfaceV1*>();
    QVERIFY(server_layer_surface);
    QCOMPARE(server_layer_surface->surface(), server_surface);

    QFETCH(Qt::Edges, anchor);
    layer_surface->set_anchor(anchor);
    layer_surface->set_size(QSize(1, 1));

    // Exclusive zone set to -1 should always produce the value.
    layer_surface->set_exclusive_zone(-1);

    QCOMPARE(server_layer_surface->exclusive_zone(), 0);

    QSignalSpy commit_spy(server_surface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());

    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    QCOMPARE(server_layer_surface->exclusive_edge(), Qt::Edge());
    QCOMPARE(server_layer_surface->exclusive_zone(), -1);

    layer_surface->set_exclusive_zone(1);
    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    QFETCH(Qt::Edge, excl_anchor);
    QFETCH(int, zone);
    QCOMPARE(server_layer_surface->exclusive_edge(), excl_anchor);
    QCOMPARE(server_layer_surface->exclusive_zone(), zone);
}

void layer_shell_test::test_margin()
{
    // Tests that margins are set and unset according to the anchor.
    QSignalSpy server_surface_spy(server.globals.compositor.get(),
                                  &Srv::Compositor::surfaceCreated);
    QVERIFY(server_surface_spy.isValid());

    std::unique_ptr<Clt::Surface> surface{compositor->createSurface()};
    QVERIFY(server_surface_spy.wait());
    auto server_surface = server_surface_spy.first().first().value<Srv::Surface*>();
    QVERIFY(server_surface);

    QSignalSpy layer_surface_spy(server.globals.layer_shell_v1.get(),
                                 &Srv::LayerShellV1::surface_created);
    QVERIFY(layer_surface_spy.isValid());

    std::unique_ptr<Clt::LayerSurfaceV1> layer_surface{layer_shell->get_layer_surface(
        surface.get(), nullptr, Clt::LayerShellV1::layer::background, "")};

    QVERIFY(layer_surface_spy.wait());
    auto server_layer_surface = layer_surface_spy.first().first().value<Srv::LayerSurfaceV1*>();
    QVERIFY(server_layer_surface);
    QCOMPARE(server_layer_surface->surface(), server_surface);

    layer_surface->set_anchor(Qt::LeftEdge | Qt::RightEdge);
    layer_surface->set_size(QSize(1, 1));

    // Exclusive zone set to -1 should always produce the value.
    layer_surface->set_margin(QMargins(1, 2, 3, 4));

    QCOMPARE(server_layer_surface->margins(), QMargins());

    QSignalSpy commit_spy(server_surface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());

    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    QCOMPARE(server_layer_surface->margins(), QMargins(1, 0, 3, 0));

    layer_surface->set_margin(QMargins(11, 12, 13, 14));
    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    QCOMPARE(server_layer_surface->margins(), QMargins(11, 0, 13, 0));

    layer_surface->set_anchor(Qt::TopEdge);

    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    QCOMPARE(server_layer_surface->margins(), QMargins(0, 12, 0, 0));
}

void layer_shell_test::test_xdg_popup()
{
    // Tests setting the layer surface as parent to an xdg-popup.
    QSignalSpy server_surface_spy(server.globals.compositor.get(),
                                  &Srv::Compositor::surfaceCreated);
    QVERIFY(server_surface_spy.isValid());

    std::unique_ptr<Clt::Surface> surface{compositor->createSurface()};
    QVERIFY(server_surface_spy.wait());
    auto server_surface = server_surface_spy.first().first().value<Srv::Surface*>();
    QVERIFY(server_surface);

    QSignalSpy layer_surface_spy(server.globals.layer_shell_v1.get(),
                                 &Srv::LayerShellV1::surface_created);
    QVERIFY(layer_surface_spy.isValid());

    std::unique_ptr<Clt::LayerSurfaceV1> layer_surface{layer_shell->get_layer_surface(
        surface.get(), nullptr, Clt::LayerShellV1::layer::background, "")};

    QVERIFY(layer_surface_spy.wait());
    auto server_layer_surface = layer_surface_spy.first().first().value<Srv::LayerSurfaceV1*>();
    QVERIFY(server_layer_surface);
    QCOMPARE(server_layer_surface->surface(), server_surface);

    QSignalSpy popup_created_spy(server.globals.xdg_shell.get(), &Srv::XdgShell::popupCreated);
    QVERIFY(popup_created_spy.isValid());

    std::unique_ptr<Clt::Surface> popup_surface{compositor->createSurface()};
    Clt::xdg_shell_positioner_data positioner_data;
    positioner_data.size = QSize(10, 10);
    positioner_data.anchor.rect = QRect(100, 100, 50, 50);

    // Now create the popup.
    std::unique_ptr<Clt::XdgShellPopup> xdg_popup{
        xdg_shell->create_popup(popup_surface.get(), positioner_data)};
    QVERIFY(popup_created_spy.wait());

    auto server_xdg_popup = popup_created_spy.first().first().value<Srv::XdgShellPopup*>();
    QVERIFY(server_xdg_popup);

    // Get the popup on the layer surface.
    QSignalSpy got_popup_spy(server_layer_surface, &Srv::LayerSurfaceV1::got_popup);
    QVERIFY(got_popup_spy.isValid());

    layer_surface->get_popup(xdg_popup.get());
    QVERIFY(got_popup_spy.wait());
    QCOMPARE(got_popup_spy.first().first().value<Srv::XdgShellPopup*>(), server_xdg_popup);
}

void layer_shell_test::test_output_removal()
{
    // This test verifies that a layer surface is closed once the associated output is removed.
    QSignalSpy server_surface_spy(server.globals.compositor.get(),
                                  &Srv::Compositor::surfaceCreated);
    QVERIFY(server_surface_spy.isValid());

    std::unique_ptr<Clt::Surface> surface{compositor->createSurface()};
    QVERIFY(server_surface_spy.wait());
    auto server_surface = server_surface_spy.first().first().value<Srv::Surface*>();
    QVERIFY(server_surface);

    QSignalSpy layer_surface_spy(server.globals.layer_shell_v1.get(),
                                 &Srv::LayerShellV1::surface_created);
    QVERIFY(layer_surface_spy.isValid());

    std::unique_ptr<Clt::LayerSurfaceV1> layer_surface{layer_shell->get_layer_surface(
        surface.get(), output, Clt::LayerShellV1::layer::background, "")};
    auto const size = QSize(100, 100);
    layer_surface->set_size(size);

    QVERIFY(layer_surface_spy.wait());
    auto server_layer_surface = layer_surface_spy.first().first().value<Srv::LayerSurfaceV1*>();
    QVERIFY(server_layer_surface);
    QCOMPARE(server_layer_surface->surface(), server_surface);

    QSignalSpy commit_spy(server_surface, &Srv::Surface::committed);
    QVERIFY(commit_spy.isValid());

    // Now do a first commit according to protocol.
    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(commit_spy.wait());

    QSignalSpy configure_spy(layer_surface.get(), &Clt::LayerSurfaceV1::configure_requested);
    QVERIFY(configure_spy.isValid());

    // Server replies with configure event.
    server_layer_surface->configure(size);
    QVERIFY(configure_spy.wait());
    QCOMPARE(configure_spy.first().first().value<QSize>(), size);

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);

    auto buf = shm->createBuffer(img);
    surface->attachBuffer(buf);
    surface->damage(QRect(QPoint(), size));
    layer_surface->ack_configure(configure_spy.first().last().value<quint32>());
    surface->commit(Clt::Surface::CommitFlag::None);

    QSignalSpy closed_spy(layer_surface.get(), &Clt::LayerSurfaceV1::closed);
    QVERIFY(closed_spy.isValid());

    // Now destroy output.
    server.output.reset();

    QVERIFY(closed_spy.wait());

    // Later commits are dropped.
    surface->damage(QRect(QPoint(), size));
    surface->commit(Clt::Surface::CommitFlag::None);
    QVERIFY(!commit_spy.wait(100));
}

QTEST_GUILESS_MAIN(layer_shell_test)
#include "layer_shell.moc"
