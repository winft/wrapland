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

#include "../../server/client.h"
#include "../../server/display.h"
#include "../../server/output.h"
#include "../../server/output_management_v1.h"
#include "../../server/wl_output.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-server.h>

using namespace Wrapland::Server;

class TestServerDisplay : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void testSocketName();
    void testStartStop();
    void testAddRemoveOutput();
    void testClientConnection();
    void testConnectNoSocket();
    void testOutputManagement();
    void testAutoSocketName();
};

void TestServerDisplay::init()
{
    qRegisterMetaType<Client*>("Wrapland::Server::Client");
}

void TestServerDisplay::testSocketName()
{
    Display display;
    QVERIFY(display.socket_name().empty());
    std::string const testSName = "fooBar";
    display.set_socket_name(testSName);
    QCOMPARE(display.socket_name(), testSName);
}

void TestServerDisplay::testStartStop()
{
    const std::string testSocketName = "kwin-wayland-server-display-test-0";
    QDir runtimeDir(qgetenv("XDG_RUNTIME_DIR"));
    QVERIFY(runtimeDir.exists());
    QVERIFY(!runtimeDir.exists(testSocketName.c_str()));

    Display display;
    QSignalSpy runningSpy(&display, &Display::started);
    QVERIFY(runningSpy.isValid());
    display.set_socket_name(testSocketName);
    QVERIFY(!display.running());
    display.start();

    QCOMPARE(runningSpy.count(), 1);
    QVERIFY(display.running());
    QVERIFY(runtimeDir.exists(testSocketName.c_str()));

    display.terminate();
    QVERIFY(!display.running());
    QVERIFY(!runtimeDir.exists(testSocketName.c_str()));
}

void TestServerDisplay::testAddRemoveOutput()
{
    Display display;
    display.set_socket_name(std::string("kwin-wayland-server-display-test-output-0"));
    display.start();

    auto output1 = std::make_unique<Wrapland::Server::output>(&display);
    output1->set_enabled(true);
    output1->done();

    QCOMPARE(display.outputs().size(), 1);
    QCOMPARE(display.outputs()[0], output1->wayland_output());

    // create a second output
    auto output2 = std::make_unique<Wrapland::Server::output>(&display);
    output2->set_enabled(true);
    output2->done();

    QCOMPARE(display.outputs().size(), 2);
    QCOMPARE(display.outputs()[0], output1->wayland_output());
    QCOMPARE(display.outputs()[1], output2->wayland_output());

    // remove the first output
    output1.reset();
    QCOMPARE(display.outputs().size(), 1);
    QCOMPARE(display.outputs()[0], output2->wayland_output());

    // and delete the second
    output2.reset();
    QVERIFY(display.outputs().empty());
}

void TestServerDisplay::testClientConnection()
{
    Display display;
    display.set_socket_name(std::string("kwin-wayland-server-display-test-client-connection"));
    display.start();

    QSignalSpy connectedSpy(&display, &Display::clientConnected);
    QVERIFY(connectedSpy.isValid());
    QSignalSpy disconnectedSpy(&display, &Display::clientDisconnected);
    QVERIFY(disconnectedSpy.isValid());

    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) >= 0);

    auto* wlClient = wl_client_create(display.native(), sv[0]);
    QVERIFY(wlClient);

    QVERIFY(connectedSpy.isEmpty());
    QVERIFY(display.clients().empty());
    Client* connection = display.getClient(wlClient);
    QVERIFY(connection);
    QCOMPARE(connection->native(), wlClient);

    if (getuid() == 0) {
        QEXPECT_FAIL("", "Please don't run test as root", Continue);
    }
    QVERIFY(connection->userId() != 0);
    if (getgid() == 0) {
        QEXPECT_FAIL("", "Please don't run test as root", Continue);
    }

    QVERIFY(connection->groupId() != 0);
    QVERIFY(connection->processId() != 0);
    QCOMPARE(connection->display(), &display);
    QCOMPARE(connection->executablePath(),
             QCoreApplication::applicationFilePath().toUtf8().constData());
    QCOMPARE((connection->native()), wlClient);

    Client const& constRef = *connection;
    QCOMPARE(constRef.native(), wlClient);
    QCOMPARE(connectedSpy.count(), 1);

    QCOMPARE(connectedSpy.first().first().value<Client*>(), connection);
    QCOMPARE(display.clients().size(), 1);
    QCOMPARE(display.clients()[0], connection);

    QCOMPARE(connection, display.getClient(wlClient));
    QCOMPARE(connectedSpy.count(), 1);

    // Create a second client.
    int sv2[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv2) >= 0);

    auto client2 = display.createClient(sv2[0]);
    QVERIFY(client2);

    auto connection2 = display.getClient(client2->native());
    QVERIFY(connection2);
    QCOMPARE(connection2, client2);
    QCOMPARE(connectedSpy.count(), 2);
    QCOMPARE(connectedSpy.first().first().value<Client*>(), connection);
    QCOMPARE(connectedSpy.last().first().value<Client*>(), connection2);
    QCOMPARE(connectedSpy.last().first().value<Client*>(), client2);
    QCOMPARE(display.clients().size(), 2);
    QCOMPARE(display.clients()[0], connection);
    QCOMPARE(display.clients()[1], connection2);
    QCOMPARE(display.clients()[1], client2);

    // and destroy
    QVERIFY(disconnectedSpy.isEmpty());
    wl_client_destroy(wlClient);
    QCOMPARE(disconnectedSpy.count(), 1);
    QSignalSpy clientDestroyedSpy(client2, &QObject::destroyed);
    QVERIFY(clientDestroyedSpy.isValid());
    client2->destroy();

    QCOMPARE(clientDestroyedSpy.count(), 1);
    QCOMPARE(disconnectedSpy.count(), 2);
    close(sv[0]);
    close(sv[1]);
    close(sv2[0]);
    close(sv2[1]);
    QVERIFY(display.clients().empty());
}

void TestServerDisplay::testConnectNoSocket()
{
    Display display;
    display.start(Display::StartMode::ConnectClientsOnly);
    QVERIFY(display.running());

    // let's try connecting a client
    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) >= 0);
    auto client = display.createClient(sv[0]);
    QVERIFY(client);

    wl_client_destroy(client->native());
    close(sv[0]);
    close(sv[1]);
}

void TestServerDisplay::testOutputManagement()
{
    Display display;
    display.set_socket_name(std::string("wrapland-test-0"));
    display.start();
    auto output_management = display.createOutputManagementV1();
}

void TestServerDisplay::testAutoSocketName()
{
    QTemporaryDir runtimeDir;
    QVERIFY(runtimeDir.isValid());
    QVERIFY(qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toUtf8()));

    Display display0;
    display0.start();
    QVERIFY(display0.running());
    QCOMPARE(display0.socket_name(), std::string("wayland-0"));

    Display display1;
    display1.start();
    QVERIFY(display1.running());
    QCOMPARE(display1.socket_name(), std::string("wayland-1"));
}

QTEST_GUILESS_MAIN(TestServerDisplay)
#include "display.moc"
