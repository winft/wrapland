/********************************************************************
Copyright 2018  David Edmundson <davidedmundson@kde.org>

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

#include "../../server/display.h"
#include "../../server/globals.h"

#include "../../server/xdg_output.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/output.h"
#include "../../src/client/registry.h"
#include "../../src/client/xdgoutput.h"

class TestXdgOutput : public QObject
{
    Q_OBJECT
public:
    explicit TestXdgOutput(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testChanges_data();
    void testChanges();

private:
    struct {
        std::unique_ptr<Wrapland::Server::Display> display;
        Wrapland::Server::globals globals;
    } server;

    std::string m_name = "HDMI-A";
    std::string m_make = "Foocorp";
    std::string m_model = "Barmodel";
    std::string m_description;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::EventQueue* m_queue;
    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-xdg-output-0"};

TestXdgOutput::TestXdgOutput(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_thread(nullptr)
{
}

void TestXdgOutput::init()
{
    server.display = std::make_unique<Wrapland::Server::Display>();
    server.display->set_socket_name(socket_name);
    server.display->start();
    QVERIFY(server.display->running());

    server.globals.outputs.push_back(
        std::make_unique<Wrapland::Server::Output>(server.display.get()));
    auto&& server_output = server.globals.outputs.back();
    server_output->add_mode(Wrapland::Server::Output::Mode{QSize(1920, 1080), 60000, true, 1});
    server_output->set_mode(1);
    server_output->set_enabled(true);

    server_output->set_name(m_name);
    server_output->set_make(m_make);
    server_output->set_model(m_model);

    server_output->generate_description();
    m_description = server_output->description();

    // Not a sensible position for one monitor but works for this test. And a 1.5 scale factor.
    server_output->set_geometry(QRectF(QPoint(11, 12), QSize(1280, 720)));
    server_output->done();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
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
}

void TestXdgOutput::cleanup()
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

void TestXdgOutput::testChanges_data()
{
    QTest::addColumn<int>("client_version");

    QTest::newRow("v1") << 1;
    QTest::newRow("v2") << 2;
    QTest::newRow("v3") << 3;
}

void TestXdgOutput::testChanges()
{
    // Verify the server modes.
    QFETCH(int, client_version);

    Wrapland::Client::Registry registry;
    QSignalSpy announced(&registry, SIGNAL(outputAnnounced(quint32, quint32)));
    QSignalSpy xdgOutputAnnounced(&registry, SIGNAL(xdgOutputAnnounced(quint32, quint32)));

    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(announced.wait());
    if (xdgOutputAnnounced.count() != 1) {
        QVERIFY(xdgOutputAnnounced.wait());
    }

    Wrapland::Client::Output output;
    QSignalSpy outputChanged(&output, SIGNAL(changed()));

    output.setup(registry.bindOutput(announced.first().first().value<quint32>(),
                                     announced.first().last().value<quint32>()));
    QVERIFY(outputChanged.wait());

    QVERIFY(xdgOutputAnnounced.first().last().value<quint32>()
            >= static_cast<quint32>(client_version));

    std::unique_ptr<Wrapland::Client::XdgOutputManager> xdgOutputManager(
        registry.createXdgOutputManager(
            xdgOutputAnnounced.first().first().value<quint32>(), client_version, this));

    std::unique_ptr<Wrapland::Client::XdgOutput> xdgOutput(
        xdgOutputManager->getXdgOutput(&output, this));
    QSignalSpy xdgOutputChanged(xdgOutput.get(), SIGNAL(changed()));

    // check details are sent on client bind
    QVERIFY(xdgOutputChanged.wait());
    xdgOutputChanged.clear();

    QCOMPARE(xdgOutput->logicalPosition(), QPoint(11, 12));
    QCOMPARE(xdgOutput->logicalSize(), QSize(1280, 720));

    if (client_version == 1) {
        QCOMPARE(xdgOutput->name(), "");
        QCOMPARE(xdgOutput->description(), "");
    } else {
        QCOMPARE(xdgOutput->name(), m_name);
        QCOMPARE(xdgOutput->description(), m_description);
    }

    // dynamic updates
    server.globals.outputs.back()->set_geometry(QRectF(QPoint(1000, 2000), QSize(100, 200)));

    std::string updated_description = "Updated xdg-output description";
    server.globals.outputs.back()->set_description(updated_description);
    server.globals.outputs.back()->done();

    QVERIFY(xdgOutputChanged.wait());
    QCOMPARE(xdgOutputChanged.count(), 1);

    QCOMPARE(xdgOutput->logicalPosition(), QPoint(1000, 2000));
    QCOMPARE(xdgOutput->logicalSize(), QSize(100, 200));

    if (client_version == 1) {
        QCOMPARE(xdgOutput->description(), "");
    } else if (client_version == 2) {
        // Description is immutable in version 2.
        QCOMPARE(xdgOutput->description(), m_description);
    } else {
        QCOMPARE(xdgOutput->description(), updated_description);
    }
}

QTEST_GUILESS_MAIN(TestXdgOutput)
#include "xdg_output.moc"
