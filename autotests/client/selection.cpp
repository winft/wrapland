/********************************************************************
Copyright © 2016 Martin Gräßlin <mgraesslin@kde.org>
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

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/data_device_manager.h"
#include "../../server/display.h"
#include "../../server/seat.h"
#include "../../server/surface.h"

class SelectionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void testClearOnEnter();

private:
    Wrapland::Server::Display* m_display = nullptr;
    Wrapland::Server::Compositor* m_serverCompositor = nullptr;
    Wrapland::Server::Seat* m_serverSeat = nullptr;
    Wrapland::Server::DataDeviceManager* m_serverDdm = nullptr;

    struct Connection {
        Wrapland::Client::ConnectionThread* connection = nullptr;
        QThread* thread = nullptr;
        Wrapland::Client::EventQueue* queue = nullptr;
        Wrapland::Client::Compositor* compositor = nullptr;
        Wrapland::Client::Seat* seat = nullptr;
        Wrapland::Client::DataDeviceManager* ddm = nullptr;
        Wrapland::Client::Keyboard* keyboard = nullptr;
        Wrapland::Client::DataDevice* dataDevice = nullptr;
    };
    bool setupConnection(Connection* c);
    void cleanupConnection(Connection* c);

    Connection m_client1;
    Connection m_client2;
};

static const std::string s_socketName{"wrapland-test-selection-0"};

void SelectionTest::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    m_display = new Wrapland::Server::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_display->createShm();
    m_serverCompositor = m_display->createCompositor(m_display);

    m_serverSeat = m_display->createSeat(m_display);
    m_serverSeat->setHasKeyboard(true);
    m_serverDdm = m_display->createDataDeviceManager(m_display);

    // setup connection
    setupConnection(&m_client1);
    setupConnection(&m_client2);
}

bool SelectionTest::setupConnection(Connection* c)
{
    c->connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(c->connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    if (!connectedSpy.isValid()) {
        return false;
    }
    c->connection->setSocketName(QString::fromStdString(s_socketName));

    c->thread = new QThread(this);
    c->connection->moveToThread(c->thread);
    c->thread->start();

    c->connection->establishConnection();
    if (!connectedSpy.wait(500)) {
        return false;
    }

    c->queue = new Wrapland::Client::EventQueue(this);
    c->queue->setup(c->connection);

    Wrapland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Wrapland::Client::Registry::interfacesAnnounced);
    if (!interfacesAnnouncedSpy.isValid()) {
        return false;
    }
    registry.setEventQueue(c->queue);
    registry.create(c->connection);
    if (!registry.isValid()) {
        return false;
    }
    registry.setup();
    if (!interfacesAnnouncedSpy.wait(500)) {
        return false;
    }

    c->compositor = registry.createCompositor(
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).name,
        registry.interface(Wrapland::Client::Registry::Interface::Compositor).version,
        this);
    if (!c->compositor->isValid()) {
        return false;
    }
    c->ddm = registry.createDataDeviceManager(
        registry.interface(Wrapland::Client::Registry::Interface::DataDeviceManager).name,
        registry.interface(Wrapland::Client::Registry::Interface::DataDeviceManager).version,
        this);
    if (!c->ddm->isValid()) {
        return false;
    }
    c->seat = registry.createSeat(
        registry.interface(Wrapland::Client::Registry::Interface::Seat).name,
        registry.interface(Wrapland::Client::Registry::Interface::Seat).version,
        this);
    if (!c->seat->isValid()) {
        return false;
    }
    QSignalSpy keyboardSpy(c->seat, &Wrapland::Client::Seat::hasKeyboardChanged);
    if (!keyboardSpy.isValid()) {
        return false;
    }
    if (!keyboardSpy.wait(500)) {
        return false;
    }
    if (!c->seat->hasKeyboard()) {
        return false;
    }
    c->keyboard = c->seat->createKeyboard(c->seat);
    if (!c->keyboard->isValid()) {
        return false;
    }
    c->dataDevice = c->ddm->getDevice(c->seat, this);
    if (!c->dataDevice->isValid()) {
        return false;
    }

    return true;
}

void SelectionTest::cleanup()
{
    cleanupConnection(&m_client1);
    cleanupConnection(&m_client2);
#define CLEANUP(variable)                                                                          \
    delete variable;                                                                               \
    variable = nullptr;

    CLEANUP(m_serverDdm)
    CLEANUP(m_serverSeat)
    CLEANUP(m_serverCompositor)
    CLEANUP(m_display)
#undef CLEANUP
}

void SelectionTest::cleanupConnection(Connection* c)
{
    delete c->dataDevice;
    c->dataDevice = nullptr;
    delete c->keyboard;
    c->keyboard = nullptr;
    delete c->ddm;
    c->ddm = nullptr;
    delete c->seat;
    c->seat = nullptr;
    delete c->compositor;
    c->compositor = nullptr;
    delete c->queue;
    c->queue = nullptr;
    if (c->connection) {
        c->connection->deleteLater();
        c->connection = nullptr;
    }
    if (c->thread) {
        c->thread->quit();
        c->thread->wait();
        delete c->thread;
        c->thread = nullptr;
    }
}

void SelectionTest::testClearOnEnter()
{
    // This test verifies that the selection is cleared prior to keyboard enter if there is no
    // current selection.

    QSignalSpy selectionClearedClient1Spy(m_client1.dataDevice,
                                          &Wrapland::Client::DataDevice::selectionCleared);
    QVERIFY(selectionClearedClient1Spy.isValid());
    QSignalSpy keyboardEnteredClient1Spy(m_client1.keyboard, &Wrapland::Client::Keyboard::entered);
    QVERIFY(keyboardEnteredClient1Spy.isValid());

    // Now create a Surface.
    QSignalSpy surfaceCreatedSpy(m_serverCompositor, &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s1(m_client1.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface1 = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface1);

    // Pass this surface keyboard focus.
    m_serverSeat->setFocusedKeyboardSurface(serverSurface1);
    // should get a clear
    QVERIFY(selectionClearedClient1Spy.wait());

    // Let's set a selection.
    std::unique_ptr<Wrapland::Client::DataSource> dataSource(m_client1.ddm->createSource());
    dataSource->offer(QStringLiteral("text/plain"));
    m_client1.dataDevice->setSelection(keyboardEnteredClient1Spy.first().first().value<quint32>(),
                                       dataSource.get());

    // Now let's bring in client 2.
    QSignalSpy selectionOfferedClient2Spy(m_client2.dataDevice,
                                          &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selectionOfferedClient2Spy.isValid());
    QSignalSpy selectionClearedClient2Spy(m_client2.dataDevice,
                                          &Wrapland::Client::DataDevice::selectionCleared);
    QVERIFY(selectionClearedClient2Spy.isValid());
    QSignalSpy keyboardEnteredClient2Spy(m_client2.keyboard, &Wrapland::Client::Keyboard::entered);
    QVERIFY(keyboardEnteredClient2Spy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s2(m_client2.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface2 = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface2);

    // Entering that surface should give a selection offer.
    m_serverSeat->setFocusedKeyboardSurface(serverSurface2);
    QVERIFY(selectionOfferedClient2Spy.wait());
    QVERIFY(selectionClearedClient2Spy.isEmpty());

    // Set a data source but without offers.
    std::unique_ptr<Wrapland::Client::DataSource> dataSource2(m_client2.ddm->createSource());
    m_client2.dataDevice->setSelection(keyboardEnteredClient2Spy.first().first().value<quint32>(),
                                       dataSource2.get());
    QVERIFY(selectionOfferedClient2Spy.wait());
    // and clear
    m_client2.dataDevice->clearSelection(
        keyboardEnteredClient2Spy.first().first().value<quint32>());
    QVERIFY(selectionClearedClient2Spy.wait());

    // Now pass focus to first surface.
    m_serverSeat->setFocusedKeyboardSurface(serverSurface1);

    // We should get a clear.
    QVERIFY(selectionClearedClient1Spy.wait());
}

QTEST_GUILESS_MAIN(SelectionTest)
#include "selection.moc"
