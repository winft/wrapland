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

#include "../../tests/globals.h"

class SelectionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void testClearOnEnter();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
        Wrapland::Server::Seat* seat{nullptr};
    } server;

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

constexpr auto socket_name{"wrapland-test-selection-0"};

void SelectionTest::init()
{
    qRegisterMetaType<Wrapland::Server::Surface*>();

    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.display->createShm();
    server.globals.compositor
        = std::make_unique<Wrapland::Server::Compositor>(server.display.get());

    server.globals.seats.emplace_back(
        std::make_unique<Wrapland::Server::Seat>(server.display.get()));
    server.seat = server.globals.seats.back().get();
    server.seat->setHasKeyboard(true);
    server.globals.data_device_manager
        = std::make_unique<Wrapland::Server::data_device_manager>(server.display.get());

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
    c->connection->setSocketName(QString::fromStdString(socket_name));

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
    server = {};
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
    server.seat->setHasKeyboard(true);

    QSignalSpy selection_offered_client1_spy(m_client1.dataDevice,
                                             &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selection_offered_client1_spy.isValid());

    QSignalSpy keyboardEnteredClient1Spy(m_client1.keyboard, &Wrapland::Client::Keyboard::entered);
    QVERIFY(keyboardEnteredClient1Spy.isValid());

    // Now create a Surface.
    QSignalSpy surfaceCreatedSpy(server.globals.compositor.get(),
                                 &Wrapland::Server::Compositor::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s1(m_client1.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface1 = surfaceCreatedSpy.first().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface1);

    auto keyboard = m_client1.seat->createKeyboard(m_client1.seat);
    QSignalSpy enter1_spy(keyboard, &Wrapland::Client::Keyboard::entered);
    QVERIFY(enter1_spy.isValid());
    QSignalSpy left1_spy(keyboard, &Wrapland::Client::Keyboard::left);
    QVERIFY(left1_spy.isValid());

    // Pass this surface keyboard focus.
    server.seat->setFocusedKeyboardSurface(serverSurface1);

    // should get no clear but left event.
    QVERIFY(enter1_spy.wait());
    QVERIFY(selection_offered_client1_spy.empty());

    // Let's set a selection.
    std::unique_ptr<Wrapland::Client::DataSource> dataSource(m_client1.ddm->createSource());
    dataSource->offer(QStringLiteral("text/plain"));
    m_client1.dataDevice->setSelection(keyboardEnteredClient1Spy.first().first().value<quint32>(),
                                       dataSource.get());

    QVERIFY(selection_offered_client1_spy.wait());
    QVERIFY(m_client1.dataDevice->offeredSelection());

    // Now let's bring in client 2.
    QSignalSpy selectionOfferedClient2Spy(m_client2.dataDevice,
                                          &Wrapland::Client::DataDevice::selectionOffered);
    QVERIFY(selectionOfferedClient2Spy.isValid());
    QSignalSpy keyboardEnteredClient2Spy(m_client2.keyboard, &Wrapland::Client::Keyboard::entered);
    QVERIFY(keyboardEnteredClient2Spy.isValid());
    std::unique_ptr<Wrapland::Client::Surface> s2(m_client2.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface2 = surfaceCreatedSpy.last().first().value<Wrapland::Server::Surface*>();
    QVERIFY(serverSurface2);

    // Entering that surface should give a selection offer.
    server.seat->setFocusedKeyboardSurface(serverSurface2);
    QVERIFY(selectionOfferedClient2Spy.wait());

    // Set a data source but without offers.
    std::unique_ptr<Wrapland::Client::DataSource> dataSource2(m_client2.ddm->createSource());
    m_client2.dataDevice->setSelection(keyboardEnteredClient2Spy.first().first().value<quint32>(),
                                       dataSource2.get());
    QVERIFY(selectionOfferedClient2Spy.wait());
    QVERIFY(m_client2.dataDevice->offeredSelection());

    // and clear
    m_client2.dataDevice->setSelection(keyboardEnteredClient2Spy.first().first().value<quint32>(),
                                       nullptr);
    QVERIFY(selectionOfferedClient2Spy.wait());
    QVERIFY(!m_client2.dataDevice->offeredSelection());

    // Now pass focus to first surface.
    server.seat->setFocusedKeyboardSurface(serverSurface1);
    selection_offered_client1_spy.clear();
    selectionOfferedClient2Spy.clear();

    // We should only get an enter event.
    QVERIFY(enter1_spy.wait());
    QVERIFY(selection_offered_client1_spy.empty());
    QVERIFY(selectionOfferedClient2Spy.empty());
}

QTEST_GUILESS_MAIN(SelectionTest)
#include "selection.moc"
