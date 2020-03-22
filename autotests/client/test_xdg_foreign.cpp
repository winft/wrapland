/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2017  Marco Martin <mart@kde.org>

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
// Qt
#include <QtTest>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"
#include "../../src/client/xdgforeign.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/surface_interface.h"
#include "../../src/server/xdgforeign_interface.h"

using namespace Wrapland::Client;

class TestForeign : public QObject
{
    Q_OBJECT
public:
    explicit TestForeign(QObject *parent = nullptr);
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

    Wrapland::Server::Display *m_display;
    Wrapland::Server::CompositorInterface *m_compositorInterface;
    Wrapland::Server::XdgForeignInterface *m_foreignInterface;
    Wrapland::Client::ConnectionThread *m_connection;
    Wrapland::Client::Compositor *m_compositor;
    Wrapland::Client::EventQueue *m_queue;
    Wrapland::Client::XdgExporter *m_exporter;
    Wrapland::Client::XdgImporter *m_importer;

    Wrapland::Client::Surface *m_exportedSurface;
    QPointer<Wrapland::Server::SurfaceInterface> m_exportedSurfaceInterface;

    Wrapland::Client::XdgExported *m_exported;
    Wrapland::Client::XdgImported *m_imported;

    Wrapland::Client::Surface *m_childSurface;
    QPointer<Wrapland::Server::SurfaceInterface> m_childSurfaceInterface;

    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("wrapland-test-xdg-foreign-0");

TestForeign::TestForeign(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_exporter(nullptr)
    , m_importer(nullptr)
    , m_thread(nullptr)
{
}

void TestForeign::init()
{
    using namespace Wrapland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    qRegisterMetaType<Wrapland::Server::SurfaceInterface*>("Wrapland::Server::SurfaceInterface");

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
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

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_foreignInterface = m_display->createXdgForeignInterface(m_display);
    m_foreignInterface->create();
    QVERIFY(m_foreignInterface->isValid());
    
    QVERIFY(exporterSpy.wait());
    //Both importer and exporter should have been triggered by now
    QCOMPARE(exporterSpy.count(), 1);
    QCOMPARE(importerSpy.count(), 1);

    m_exporter = registry.createXdgExporter(exporterSpy.first().first().value<quint32>(), exporterSpy.first().last().value<quint32>(), this);
    m_importer = registry.createXdgImporter(importerSpy.first().first().value<quint32>(), importerSpy.first().last().value<quint32>(), this);
}

void TestForeign::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }

    CLEANUP(m_exportedSurfaceInterface)
    CLEANUP(m_childSurfaceInterface)

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
    CLEANUP(m_compositorInterface)
    CLEANUP(m_foreignInterface)

    //internally there are some deleteLaters on exported interfaces
    //we want them processed before we delete the connection
    if (m_display) {
        QSignalSpy destroyedSpy(m_display, &QObject::destroyed);
        m_display->deleteLater();
        m_display = nullptr;
        destroyedSpy.wait();
    }

#undef CLEANUP
}

void TestForeign::doExport()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(Wrapland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    m_exportedSurface = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    m_exportedSurfaceInterface = serverSurfaceCreated.first().first().value<Wrapland::Server::SurfaceInterface*>();

    //Export a window
    m_exported = m_exporter->exportTopLevel(m_exportedSurface, m_connection);
    QVERIFY(m_exported->handle().isEmpty());
    QSignalSpy doneSpy(m_exported, &XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!m_exported->handle().isEmpty());

    QSignalSpy parentSpy(m_foreignInterface, &Wrapland::Server::XdgForeignInterface::parentChanged);
    QVERIFY(parentSpy.isValid());

    //Import the just exported window
    m_imported = m_importer->importTopLevel(m_exported->handle(), m_connection);
    QVERIFY(m_imported->isValid());

    QSignalSpy childSurfaceInterfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(Wrapland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    m_childSurface = m_compositor->createSurface();
    QVERIFY(childSurfaceInterfaceCreated.wait());

    m_childSurfaceInterface = childSurfaceInterfaceCreated.first().first().value<Wrapland::Server::SurfaceInterface*>();
    m_childSurface->commit(Surface::CommitFlag::None);

    m_imported->setParentOf(m_childSurface);

    QVERIFY(parentSpy.wait());

    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface);
    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::SurfaceInterface *>(), m_childSurfaceInterface);

    // parentOf API
    QCOMPARE(m_foreignInterface->parentOf(m_childSurfaceInterface), m_exportedSurfaceInterface);
}

void TestForeign::testExport()
{
    doExport();

    delete m_imported;
    delete m_exportedSurface;
    delete m_exported;
}

void TestForeign::testDeleteImported()
{
    doExport();

    QSignalSpy parentSpy(m_foreignInterface, &Wrapland::Server::XdgForeignInterface::parentChanged);
 
    QVERIFY(parentSpy.isValid());

    m_imported->deleteLater();

    QVERIFY(parentSpy.wait());

    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface);
    QVERIFY(!parentSpy.first().at(1).value<Wrapland::Server::SurfaceInterface *>());
    QVERIFY(!m_foreignInterface->parentOf(m_childSurfaceInterface));

    delete m_exportedSurface;
    delete m_exported;
}

void TestForeign::testDeleteChildSurface()
{
    doExport();

    QSignalSpy parentSpy(m_foreignInterface, &Wrapland::Server::XdgForeignInterface::parentChanged);
 
    QVERIFY(parentSpy.isValid());
    m_childSurface->deleteLater();

    QVERIFY(parentSpy.wait());

    //when the client surface dies, the server one will eventually die too
    QSignalSpy surfaceDestroyedSpy(m_childSurfaceInterface, SIGNAL(destroyed()));
    QVERIFY(surfaceDestroyedSpy.wait());

    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface);
    QVERIFY(!parentSpy.first().at(1).value<Wrapland::Server::SurfaceInterface *>());

    delete m_imported;
    delete m_exportedSurface;
    delete m_exported;
}

void TestForeign::testDeleteParentSurface()
{
    doExport();

    QSignalSpy parentSpy(m_foreignInterface, &Wrapland::Server::XdgForeignInterface::parentChanged);
 
    QVERIFY(parentSpy.isValid());
    m_exportedSurface->deleteLater();

    QSignalSpy exportedSurfaceDestroyedSpy(m_exportedSurfaceInterface, &QObject::destroyed);
    QVERIFY(exportedSurfaceDestroyedSpy.isValid());
    exportedSurfaceDestroyedSpy.wait();

    QVERIFY(parentSpy.count() || parentSpy.wait());

    QVERIFY(!parentSpy.first().at(0).value<Wrapland::Server::SurfaceInterface *>());
    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::SurfaceInterface *>(), m_childSurfaceInterface);
    QVERIFY(!m_foreignInterface->parentOf(m_childSurfaceInterface));

    delete m_imported;
    delete m_exported;
}

void TestForeign::testDeleteExported()
{
    doExport();

    QSignalSpy parentSpy(m_foreignInterface, &Wrapland::Server::XdgForeignInterface::parentChanged);
    QSignalSpy destroyedSpy(m_imported, &Wrapland::Client::XdgImported::importedDestroyed);
 
    QVERIFY(parentSpy.isValid());
    m_exported->deleteLater();

    QVERIFY(parentSpy.wait());
    QVERIFY(destroyedSpy.wait());

    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::SurfaceInterface *>(), m_childSurfaceInterface);
    QVERIFY(!parentSpy.first().at(0).value<Wrapland::Server::SurfaceInterface *>());
    QVERIFY(!m_foreignInterface->parentOf(m_childSurfaceInterface));

    QVERIFY(!m_imported->isValid());

    delete m_imported;
    delete m_exportedSurface;
}

void TestForeign::testExportTwoTimes()
{
    doExport();

    //Export second window
    Wrapland::Client::XdgExported *exported2 = m_exporter->exportTopLevel(m_exportedSurface, m_connection);
    QVERIFY(exported2->handle().isEmpty());
    QSignalSpy doneSpy(exported2, &XdgExported::done);
    QVERIFY(doneSpy.wait());
    QVERIFY(!exported2->handle().isEmpty());

    QSignalSpy parentSpy(m_foreignInterface, &Wrapland::Server::XdgForeignInterface::parentChanged);
    QVERIFY(parentSpy.isValid());

    //Import the just exported window
    Wrapland::Client::XdgImported *imported2 = m_importer->importTopLevel(exported2->handle(), m_connection);
    QVERIFY(imported2->isValid());

    //create a second child surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(Wrapland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    Wrapland::Client::Surface *childSurface2 = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    Wrapland::Server::SurfaceInterface *childSurface2Interface = serverSurfaceCreated.first().first().value<Wrapland::Server::SurfaceInterface*>();

    imported2->setParentOf(childSurface2);
    QVERIFY(parentSpy.wait());
    
    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::SurfaceInterface *>(), childSurface2Interface);
    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface.data());

    // parentOf API:
    // Check the old relationship is still here.
    QCOMPARE(m_foreignInterface->parentOf(m_childSurfaceInterface), m_exportedSurfaceInterface.data());

    // Check the new relationship.
    QCOMPARE(m_foreignInterface->parentOf(childSurface2Interface), m_exportedSurfaceInterface.data());

    delete childSurface2;
    delete imported2;
    delete exported2;
    delete m_imported;
    delete m_exportedSurface;
    delete m_exported;
}

void TestForeign::testImportTwoTimes()
{
    doExport();

    QSignalSpy parentSpy(m_foreignInterface, &Wrapland::Server::XdgForeignInterface::parentChanged);
    QVERIFY(parentSpy.isValid());

    //Import another time the exported window
    Wrapland::Client::XdgImported *imported2 = m_importer->importTopLevel(m_exported->handle(), m_connection);
    QVERIFY(imported2->isValid());

    //create a second child surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(Wrapland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    Wrapland::Client::Surface *childSurface2 = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());

    Wrapland::Server::SurfaceInterface *childSurface2Interface = serverSurfaceCreated.first().first().value<Wrapland::Server::SurfaceInterface*>();

    imported2->setParentOf(childSurface2);
    QVERIFY(parentSpy.wait());
    
    QCOMPARE(parentSpy.first().at(1).value<Wrapland::Server::SurfaceInterface *>(), childSurface2Interface);
    QCOMPARE(parentSpy.first().at(0).value<Wrapland::Server::SurfaceInterface *>(), m_exportedSurfaceInterface.data());

    // parentOf API:
    // Check the old relationship is still here.
    QCOMPARE(m_foreignInterface->parentOf(m_childSurfaceInterface), m_exportedSurfaceInterface.data());
    // Check the new relationship.
    QCOMPARE(m_foreignInterface->parentOf(childSurface2Interface), m_exportedSurfaceInterface.data());

    delete imported2;
    delete m_imported;
    delete m_exportedSurface;
    delete m_exported;
}

QTEST_GUILESS_MAIN(TestForeign)
#include "test_xdg_foreign.moc"
