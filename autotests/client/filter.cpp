/********************************************************************
Copyright 2017  David Edmundson <davidedmundson@kde.org>

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
#include "../../src/client/blur.h"

#include "../../server/client.h"
#include "../../server/display.h"
#include "../../server/compositor.h"
#include "../../server/region.h"
#include "../../server/blur.h"
#include "../../server/filtered_display.h"

#include <wayland-server.h>

using namespace Wrapland::Client;

class TestDisplay;

class TestFilter : public QObject
{
    Q_OBJECT
public:
    explicit TestFilter(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();
    void testFilter_data();
    void testFilter();

private:
    TestDisplay *m_display;
    Wrapland::Server::Compositor *m_serverCompositor;
    Wrapland::Server::BlurManager *m_blurManagerInterface;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-blur-0");

//The following non-realistic class allows only clients in the m_allowedClients list to access the blur interface
//all other interfaces are allowed
class TestDisplay : public Wrapland::Server::FilteredDisplay
{
public:
    TestDisplay(QObject *parent);
    bool allowInterface(Wrapland::Server::Client* client, const QByteArray & interfaceName) override;
    QList<wl_client*> m_allowedClients;
};

TestDisplay::TestDisplay(QObject *parent):
    Wrapland::Server::FilteredDisplay(parent)
{}

bool TestDisplay::allowInterface(Wrapland::Server::Client* client, const QByteArray& interfaceName)
{
    if (interfaceName == "org_kde_kwin_blur_manager") {
        return m_allowedClients.contains(client->native());
    }
    return true;
}

TestFilter::TestFilter(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverCompositor(nullptr)
{}

void TestFilter::init()
{
    using namespace Wrapland::Server;
    delete m_display;
    m_display = new TestDisplay(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->running());

    m_serverCompositor = m_display->createCompositor(m_display);
    m_blurManagerInterface = m_display->createBlurManager(m_display);
}

void TestFilter::cleanup()
{
}

void TestFilter::testFilter_data()
{
    QTest::addColumn<bool>("accessAllowed");
    QTest::newRow("granted") << true;
    QTest::newRow("denied") << false;

}

void TestFilter::testFilter()
{
    QFETCH(bool, accessAllowed);

  // setup connection
    std::unique_ptr<Wrapland::Client::ConnectionThread> connection(new Wrapland::Client::ConnectionThread());
    QSignalSpy connectedSpy(connection.get(), &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    connection->setSocketName(s_socketName);

    std::unique_ptr<QThread> thread(new QThread(this));
    connection->moveToThread(thread.get());
    thread->start();

    connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    //use low level API as Server::Display::connections only lists connections which have
    //been previous fetched via getConnection()
    if (accessAllowed) {
        wl_client *clientConnection;
        wl_client_for_each(clientConnection, wl_display_get_client_list(m_display->native())) {
            m_display->m_allowedClients << clientConnection;
        }
    }

    Wrapland::Client::EventQueue queue;
    queue.setup(connection.get());

    Registry registry;
    QSignalSpy registryDoneSpy(&registry, &Registry::interfacesAnnounced);
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QSignalSpy blurSpy(&registry, &Registry::blurAnnounced);

    registry.setEventQueue(&queue);
    registry.create(connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(registryDoneSpy.wait());
    QVERIFY(compositorSpy.count() == 1);
    QVERIFY(blurSpy.count() == accessAllowed ? 1 : 0);

    thread->quit();
    thread->wait();
}


QTEST_GUILESS_MAIN(TestFilter)
#include "filter.moc"
