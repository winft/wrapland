/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2017  Marco Martin <mart@kde.org>
Copyright 2020  Roman Gilg <subdiff@gmail.com>

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

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"
#include "../../src/client/xdgforeign.h"

#include "../../server/display.h"
#include "../../server/globals.h"
#include "../../server/surface.h"
#include "../../server/xdg_foreign.h"

using namespace Wrapland::Client;

class TestForeign : public QObject
{
    Q_OBJECT
public:
    explicit TestForeign(QObject* parent = nullptr);

private Q_SLOTS:
    void init();
    void cleanup();

    void testExport();
    void testDeleteImported();
    void testDeleteChildSurface();
    void testDeleteParentSurface();
    void testDeleteExported();
    void testExportTwoTimes();
    void testImportTwoTimes();

private:
    void doExport();

    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;

        Wrapland::Server::Surface* child_surface{nullptr};
    } server;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::EventQueue* m_queue;
    Wrapland::Client::XdgExporter* m_exporter;
    Wrapland::Client::XdgImporter* m_importer;

    Wrapland::Client::Surface* m_exportedSurface;
    Wrapland::Server::Surface* m_exportedServerSurface;

    Wrapland::Client::XdgExported* m_exported;
    Wrapland::Client::XdgImported* m_imported;

    Wrapland::Client::Surface* m_childSurface;

    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-xdg-foreign-0"};

TestForeign::TestForeign(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_exporter(nullptr)
    , m_importer(nullptr)
    , m_thread(nullptr)
{
    qRegisterMetaType<Wrapland::Server::Surface*>();
}

void TestForeign::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
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

    Registry registry;
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QVERIFY(compositorSpy.isValid());

    QSignalSpy exporterSpy(&registry, &Registry::exporterUnstableV2Announced);
    QVERIFY(exporterSpy.isValid());

    QSignalSpy importerSpy(&registry, &Registry::importerUnstableV2Announced);
    QVERIFY(importerSpy.isValid());

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    server.globals.compositor = server.display->createCompositor();

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);

    server.globals.xdg_foreign = server.display->createXdgForeign();

    QVERIFY(exporterSpy.wait());
    // Both importer and exporter should have been triggered by now
    QCOMPARE(exporterSpy.count(), 1);
    QCOMPARE(importerSpy.count(), 1);

    m_exporter = registry.createXdgExporter(exporterSpy.first().first().value<quint32>(),
                                            exporterSpy.first().last().value<quint32>(),
                                            this);
    m_importer = registry.createXdgImporter(importerSpy.first().first().value<quint32>(),
                                            importerSpy.first().last().value<quint32>(),
                                            this);
}

void TestForeign::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }

    CLEANUP(m_compositor)
    CLEANUP(m_exporter)
    CLEANUP(m_importer)
    CLEANUP(m_queue)
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
#undef CLEANUP

    server = {};
}

void TestForeign::doExport()
{
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    m_exportedSurface = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    m_exportedServerSurface
        = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();

    // Export a window
    m_exported = m_exporter->exportTopLevel(m_exportedSurface);

    QVERIFY(m_exported->handle().isEmpty());
    QSignalSpy doneSpy(m_exported, &XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!m_exported->handle().isEmpty());

    QSignalSpy parentSpy(server.globals.xdg_foreign.get(),
                         &Wrapland::Server::XdgForeign::parentChanged);
    QVERIFY(parentSpy.isValid());

    // Import the just exported window
    m_imported = m_importer->importTopLevel(m_exported->handle());
    QVERIFY(m_imported->isValid());

    QSignalSpy childServerSurfaceCreated(server.globals.compositor.get(),
                                         &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    m_childSurface = m_compositor->createSurface();
    QVERIFY(childServerSurfaceCreated.wait());

    server.child_surface
        = childServerSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();
    m_childSurface->commit(Surface::CommitFlag::None);

    m_imported->setParentOf(m_childSurface);

    QVERIFY(parentSpy.wait());

    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::Surface*>(), m_exportedServerSurface);
    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::Surface*>(), server.child_surface);

    // parentOf API
    QCOMPARE(server.globals.xdg_foreign->parentOf(server.child_surface), m_exportedServerSurface);
}

void TestForeign::testExport()
{
    doExport();

    delete m_imported;
    delete m_exportedSurface;
    delete m_exported;
    delete m_childSurface;
}

void TestForeign::testDeleteImported()
{
    doExport();

    QSignalSpy parentSpy(server.globals.xdg_foreign.get(),
                         &Wrapland::Server::XdgForeign::parentChanged);

    QVERIFY(parentSpy.isValid());

    m_imported->deleteLater();

    QVERIFY(parentSpy.wait());

    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::Surface*>(), m_exportedServerSurface);
    QVERIFY(!parentSpy.first().at(1).value<Wrapland::Server::Surface*>());
    QVERIFY(!server.globals.xdg_foreign->parentOf(server.child_surface));

    delete m_exportedSurface;
    delete m_exported;
    delete m_childSurface;
}

void TestForeign::testDeleteChildSurface()
{
    doExport();

    QSignalSpy parentSpy(server.globals.xdg_foreign.get(),
                         &Wrapland::Server::XdgForeign::parentChanged);
    QVERIFY(parentSpy.isValid());

    // When the client surface dies, the server one will eventually die too
    QSignalSpy surfaceDestroyedSpy(server.child_surface,
                                   &Wrapland::Server::Surface::resourceDestroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());

    m_childSurface->deleteLater();

    QVERIFY(parentSpy.wait());
    QVERIFY(surfaceDestroyedSpy.count() || surfaceDestroyedSpy.wait());

    //    QVERIFY(surfaceDestroyedSpy.wait());

    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::Surface*>(), m_exportedServerSurface);
    QVERIFY(!parentSpy.first().at(1).value<Wrapland::Server::Surface*>());

    delete m_imported;
    delete m_exportedSurface;
    delete m_exported;
}

void TestForeign::testDeleteParentSurface()
{
    doExport();

    QSignalSpy parentSpy(server.globals.xdg_foreign.get(),
                         &Wrapland::Server::XdgForeign::parentChanged);
    QVERIFY(parentSpy.isValid());

    QSignalSpy exportedSurfaceDestroyedSpy(m_exportedServerSurface,
                                           &Wrapland::Server::Surface::resourceDestroyed);
    QVERIFY(exportedSurfaceDestroyedSpy.isValid());

    m_exportedSurface->deleteLater();

    exportedSurfaceDestroyedSpy.wait();

    QVERIFY(parentSpy.count() || parentSpy.wait());

    QVERIFY(!parentSpy.first().at(0).value<Wrapland::Server::Surface*>());
    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::Surface*>(), server.child_surface);
    QVERIFY(!server.globals.xdg_foreign->parentOf(server.child_surface));

    delete m_imported;
    delete m_exported;
    delete m_childSurface;
}

void TestForeign::testDeleteExported()
{
    doExport();

    QSignalSpy parentSpy(server.globals.xdg_foreign.get(),
                         &Wrapland::Server::XdgForeign::parentChanged);
    QSignalSpy destroyedSpy(m_imported, &Wrapland::Client::XdgImported::importedDestroyed);

    QVERIFY(parentSpy.isValid());
    m_exported->deleteLater();

    QVERIFY(parentSpy.wait());
    QVERIFY(destroyedSpy.wait());

    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::Surface*>(), server.child_surface);
    QVERIFY(!parentSpy.first().at(0).value<Wrapland::Server::Surface*>());
    QVERIFY(!server.globals.xdg_foreign->parentOf(server.child_surface));

    QVERIFY(!m_imported->isValid());

    delete m_imported;
    delete m_exportedSurface;
    delete m_childSurface;
}

void TestForeign::testExportTwoTimes()
{
    doExport();

    // Export second window
    Wrapland::Client::XdgExported* exported2 = m_exporter->exportTopLevel(m_exportedSurface);
    QVERIFY(exported2->handle().isEmpty());
    QSignalSpy doneSpy(exported2, &XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!exported2->handle().isEmpty());

    QSignalSpy parentSpy(server.globals.xdg_foreign.get(),
                         &Wrapland::Server::XdgForeign::parentChanged);
    QVERIFY(parentSpy.isValid());

    // Import the just exported window
    Wrapland::Client::XdgImported* imported2 = m_importer->importTopLevel(exported2->handle());
    QVERIFY(imported2->isValid());

    // create a second child surface
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    Wrapland::Client::Surface* childSurface2 = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    Wrapland::Server::Surface* childSurface2Interface
        = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();

    imported2->setParentOf(childSurface2);
    QVERIFY(parentSpy.wait());

    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::Surface*>(), childSurface2Interface);
    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::Surface*>(), m_exportedServerSurface);

    // parentOf API:
    // Check the old relationship is still here.
    QCOMPARE(server.globals.xdg_foreign->parentOf(server.child_surface), m_exportedServerSurface);

    // Check the new relationship.
    QCOMPARE(server.globals.xdg_foreign->parentOf(childSurface2Interface), m_exportedServerSurface);

    delete childSurface2;
    delete imported2;
    delete exported2;
    delete m_imported;
    delete m_exportedSurface;
    delete m_exported;
    delete m_childSurface;
}

void TestForeign::testImportTwoTimes()
{
    doExport();

    QSignalSpy parentSpy(server.globals.xdg_foreign.get(),
                         &Wrapland::Server::XdgForeign::parentChanged);
    QVERIFY(parentSpy.isValid());

    // Import another time the exported window
    Wrapland::Client::XdgImported* imported2 = m_importer->importTopLevel(m_exported->handle());
    QVERIFY(imported2->isValid());

    // create a second child surface
    QSignalSpy serverSurfaceCreated(server.globals.compositor.get(),
                                    &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    auto childSurface2 = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    Wrapland::Server::Surface* childSurface2Interface
        = serverSurfaceCreated.first().first().value<Wrapland::Server::Surface*>();

    imported2->setParentOf(childSurface2);
    QVERIFY(parentSpy.wait());

    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::Surface*>(), childSurface2Interface);
    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::Surface*>(), m_exportedServerSurface);

    // parentOf API:
    // Check the old relationship is still here.
    QCOMPARE(server.globals.xdg_foreign->parentOf(server.child_surface), m_exportedServerSurface);
    // Check the new relationship.
    QCOMPARE(server.globals.xdg_foreign->parentOf(childSurface2Interface), m_exportedServerSurface);

    delete imported2;
    delete m_imported;
    delete m_exportedSurface;
    delete m_exported;
    delete m_childSurface;
    delete childSurface2;
}

QTEST_GUILESS_MAIN(TestForeign)
#include "xdg_foreign.moc"
