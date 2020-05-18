/********************************************************************
Copyright © 2016  Martin Gräßlin <mgraesslin@kde.org>
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
#include "../../src/client/registry.h"
#include "../../src/client/plasmawindowmanagement.h"
#include "../../src/client/plasmawindowmodel.h"

#include "../../server/display.h"
#include "../../server/plasma_window.h"
#include "../../server/plasma_virtual_desktop.h"

#include <QtTest>

#include <linux/input.h>

namespace Clt = Wrapland::Client;
namespace Srv = Wrapland::Server;

Q_DECLARE_METATYPE(Qt::MouseButton)

typedef void (Clt::PlasmaWindow::*ClientWindowSignal)();
Q_DECLARE_METATYPE(ClientWindowSignal)

typedef void (Srv::PlasmaWindow::*ServerWindowBoolSetter)(bool);
Q_DECLARE_METATYPE(ServerWindowBoolSetter)

typedef void (Srv::PlasmaWindow::*ServerWindowStringSetter)(const QString&);
Q_DECLARE_METATYPE(ServerWindowStringSetter)

typedef void (Srv::PlasmaWindow::*ServerWindowQuint32Setter)(quint32);
Q_DECLARE_METATYPE(ServerWindowQuint32Setter)

typedef void (Srv::PlasmaWindow::*ServerWindowVoidSetter)();
Q_DECLARE_METATYPE(ServerWindowVoidSetter)

typedef void (Srv::PlasmaWindow::*ServerWindowIconSetter)(const QIcon&);
Q_DECLARE_METATYPE(ServerWindowIconSetter)

class PlasmaWindowModelTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testRoleNames_data();
    void testRoleNames();

    void testAddRemoveRows();

    void testDefaultData_data();
    void testDefaultData();

    void testIsActive();
    void testIsFullscreenable();
    void testIsFullscreen();
    void testIsMaximizable();
    void testIsMaximized();
    void testIsMinimizable();
    void testIsMinimized();
    void testIsKeepAbove();
    void testIsKeepBelow();

    void testIsDemandingAttention();
    void testSkipTaskbar();
    void testSkipSwitcher();

    void testIsShadeable();
    void testIsShaded();
    void testIsMovable();
    void testIsResizable();
    void testIsVirtualDesktopChangeable();
    void testIsCloseable();

    void testGeometry();
    void testTitle();
    void testAppId();
    void testPid();
    void testVirtualDesktops();

    // TODO icon: can we ensure a theme is installed on CI?
    void testRequests();

    // TODO: minimized geometry
    // TODO: model reset
    void testCreateWithUnmappedWindow();
    void testChangeWindowAfterModelDestroy_data();
    void testChangeWindowAfterModelDestroy();
    void testCreateWindowAfterModelDestroy();

private:
    bool testBooleanData(Clt::PlasmaWindowModel::AdditionalRoles role,
                         void (Srv::PlasmaWindow::*function)(bool));

    Srv::Display *m_display = nullptr;

    Srv::PlasmaWindowManager *m_serverWindowManager = nullptr;
    Clt::PlasmaWindowManagement *m_pw = nullptr;

    Srv::PlasmaVirtualDesktopManager *m_serverPlasmaVirtualDesktopManager = nullptr;

    Clt::ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    Clt::EventQueue *m_queue = nullptr;
};

static const QString s_socketName = QStringLiteral("wrapland-test-fake-input-0");

void PlasmaWindowModelTest::init()
{
    delete m_display;
    m_display = new Srv::Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->running());

    m_display->createShm();
    m_serverWindowManager = m_display->createPlasmaWindowManager();

    m_serverPlasmaVirtualDesktopManager = m_display->createPlasmaVirtualDesktopManager(m_display);

    m_serverPlasmaVirtualDesktopManager->createDesktop("desktop1");
    m_serverPlasmaVirtualDesktopManager->createDesktop("desktop2");
    m_serverWindowManager->setVirtualDesktopManager(m_serverPlasmaVirtualDesktopManager);

    // Setup connection.
    m_connection = new Clt::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Clt::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Clt::EventQueue(this);
    m_queue->setup(m_connection);

    Clt::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Clt::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());

    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());

    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_pw = registry.createPlasmaWindowManagement(
                registry.interface(Clt::Registry::Interface::PlasmaWindowManagement).name,
                registry.interface(Clt::Registry::Interface::PlasmaWindowManagement).version,
                this);
    QVERIFY(m_pw->isValid());
}

void PlasmaWindowModelTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_pw)
    CLEANUP(m_serverPlasmaVirtualDesktopManager)
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

    CLEANUP(m_serverWindowManager)
    CLEANUP(m_display)
#undef CLEANUP
}

bool PlasmaWindowModelTest::testBooleanData(Clt::PlasmaWindowModel::AdditionalRoles role,
                                            void (Srv::PlasmaWindow::*function)(bool))
{
#define VERIFY(statement) \
if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
    return false;
#define COMPARE(actual, expected) \
if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
    return false;

    auto *model = m_pw->createWindowModel();
    VERIFY(model);

    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    VERIFY(rowInsertedSpy.isValid());

    QSignalSpy initSpy(m_pw, &Clt::PlasmaWindowManagement::windowCreated);
    VERIFY(initSpy.isValid());
    QSignalSpy dataChangedSpy(model, &Clt::PlasmaWindowModel::dataChanged);
    VERIFY(dataChangedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    VERIFY(serverWindow);

    COMPARE(dataChangedSpy.count(), 0);
    COMPARE(initSpy.count(), 0);

    VERIFY(rowInsertedSpy.wait());
    COMPARE(initSpy.count(), 1);

    // Wait for the first event announcing the icon on resource creation.
    VERIFY(dataChangedSpy.count() == 1 || dataChangedSpy.wait());
    COMPARE(dataChangedSpy.count(), 1);
    COMPARE(initSpy.count(), 1);

    // The current API is not very well defined in regards to which evente we can except when.
    // Make sure we do not get additional data changed signals before actually setting the data.
    VERIFY(!dataChangedSpy.wait(100));

    const QModelIndex index = model->index(0);
    COMPARE(model->data(index, role).toBool(), false);

    (serverWindow->*(function))(true);

    VERIFY(dataChangedSpy.wait());
    COMPARE(dataChangedSpy.count(), 2);

    // Check that there is only one data changed signal we receive.
    VERIFY(!dataChangedSpy.wait(100));

    COMPARE(dataChangedSpy.last().first().toModelIndex(), index);
    COMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(role)});
    COMPARE(model->data(index, role).toBool(), true);

    (serverWindow->*(function))(false);
    VERIFY(dataChangedSpy.wait());
    COMPARE(dataChangedSpy.count(), 3);

    COMPARE(dataChangedSpy.last().first().toModelIndex(), index);
    COMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(role)});
    COMPARE(model->data(index, role).toBool(), false);

#undef COMPARE
#undef VERIFY
    return true;
}

void PlasmaWindowModelTest::testRoleNames_data()
{
    QTest::addColumn<int>("role");
    QTest::addColumn<QByteArray>("name");

    QTest::newRow("display") << int(Qt::DisplayRole) << QByteArrayLiteral("DisplayRole");
    QTest::newRow("decoration") << int(Qt::DecorationRole) << QByteArrayLiteral("DecorationRole");

    QTest::newRow("AppId")
            << int(Clt::PlasmaWindowModel::AppId) << QByteArrayLiteral("AppId");
    QTest::newRow("Pid")
            << int(Clt::PlasmaWindowModel::Pid) << QByteArrayLiteral("Pid");
    QTest::newRow("IsActive")
            << int(Clt::PlasmaWindowModel::IsActive) << QByteArrayLiteral("IsActive");
    QTest::newRow("IsFullscreenable")
            << int(Clt::PlasmaWindowModel::IsFullscreenable)
            << QByteArrayLiteral("IsFullscreenable");
    QTest::newRow("IsFullscreen")
            << int(Clt::PlasmaWindowModel::IsFullscreen) << QByteArrayLiteral("IsFullscreen");
    QTest::newRow("IsMaximizable")
            << int(Clt::PlasmaWindowModel::IsMaximizable) << QByteArrayLiteral("IsMaximizable");
    QTest::newRow("IsMaximized")
            << int(Clt::PlasmaWindowModel::IsMaximized) << QByteArrayLiteral("IsMaximized");
    QTest::newRow("IsMinimizable")
            << int(Clt::PlasmaWindowModel::IsMinimizable) << QByteArrayLiteral("IsMinimizable");
    QTest::newRow("IsMinimized")
            << int(Clt::PlasmaWindowModel::IsMinimized) << QByteArrayLiteral("IsMinimized");
    QTest::newRow("IsKeepAbove")
            << int(Clt::PlasmaWindowModel::IsKeepAbove) << QByteArrayLiteral("IsKeepAbove");
    QTest::newRow("IsKeepBelow")
            << int(Clt::PlasmaWindowModel::IsKeepBelow) << QByteArrayLiteral("IsKeepBelow");
    QTest::newRow("IsOnAllDesktops")
            << int(Clt::PlasmaWindowModel::IsOnAllDesktops) << QByteArrayLiteral("IsOnAllDesktops");
    QTest::newRow("IsDemandingAttention")
            << int(Clt::PlasmaWindowModel::IsDemandingAttention)
            << QByteArrayLiteral("IsDemandingAttention");
    QTest::newRow("SkipTaskbar")
            << int(Clt::PlasmaWindowModel::SkipTaskbar) << QByteArrayLiteral("SkipTaskbar");
    QTest::newRow("SkipSwitcher")
            << int(Clt::PlasmaWindowModel::SkipSwitcher) << QByteArrayLiteral("SkipSwitcher");
    QTest::newRow("IsShadeable")
            << int(Clt::PlasmaWindowModel::IsShadeable) << QByteArrayLiteral("IsShadeable");
    QTest::newRow("IsShaded")
            << int(Clt::PlasmaWindowModel::IsShaded) << QByteArrayLiteral("IsShaded");
    QTest::newRow("IsMovable")
            << int(Clt::PlasmaWindowModel::IsMovable) << QByteArrayLiteral("IsMovable");
    QTest::newRow("IsResizable")
            << int(Clt::PlasmaWindowModel::IsResizable) << QByteArrayLiteral("IsResizable");
    QTest::newRow("IsVirtualDesktopChangeable")
            << int(Clt::PlasmaWindowModel::IsVirtualDesktopChangeable)
            << QByteArrayLiteral("IsVirtualDesktopChangeable");
    QTest::newRow("IsCloseable")
            << int(Clt::PlasmaWindowModel::IsCloseable) << QByteArrayLiteral("IsCloseable");
    QTest::newRow("Geometry")
            << int(Clt::PlasmaWindowModel::Geometry) << QByteArrayLiteral("Geometry");
}

void PlasmaWindowModelTest::testRoleNames()
{
    // Just verifies that all role names are available.
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    const QHash<int, QByteArray> roles = model->roleNames();

    QFETCH(int, role);
    auto it = roles.find(role);
    QVERIFY(it != roles.end());
    QTEST(it.value(), "name");
}

void PlasmaWindowModelTest::testAddRemoveRows()
{
    // This test verifies that adding/removing rows to the Model works.
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QCOMPARE(model->rowCount(), 0);
    QVERIFY(!model->index(0).isValid());

    // Now let's add a row.
    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());

    // This happens by creating a PlasmaWindow on server side.
    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(serverWindow);

    QVERIFY(rowInsertedSpy.wait());
    QCOMPARE(rowInsertedSpy.count(), 1);
    QVERIFY(!rowInsertedSpy.first().at(0).toModelIndex().isValid());
    QCOMPARE(rowInsertedSpy.first().at(1).toInt(), 0);
    QCOMPARE(rowInsertedSpy.first().at(2).toInt(), 0);

    // The model should have a row now.
    QCOMPARE(model->rowCount(), 1);
    QVERIFY(model->index(0).isValid());
    // That index doesn't have children.
    QCOMPARE(model->rowCount(model->index(0)), 0);

    // Process events in order to ensure that the resource is created on server side before we
    // unmap the window.
    QCoreApplication::instance()->processEvents(QEventLoop::WaitForMoreEvents);

    // Now let's remove that again.
    QSignalSpy rowRemovedSpy(model, &Clt::PlasmaWindowModel::rowsRemoved);
    QVERIFY(rowRemovedSpy.isValid());

    QSignalSpy windowDestroyedSpy(serverWindow, &QObject::destroyed);
    QVERIFY(windowDestroyedSpy.isValid());

    serverWindow->unmap();
    QVERIFY(rowRemovedSpy.wait());
    QCOMPARE(rowRemovedSpy.count(), 1);
    QVERIFY(!rowRemovedSpy.first().at(0).toModelIndex().isValid());
    QCOMPARE(rowRemovedSpy.first().at(1).toInt(), 0);
    QCOMPARE(rowRemovedSpy.first().at(2).toInt(), 0);

    // Now the model is empty again.
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(!model->index(0).isValid());

    QCOMPARE(windowDestroyedSpy.count(), 1);
}

void PlasmaWindowModelTest::testDefaultData_data()
{
    QTest::addColumn<int>("role");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("display") << int(Qt::DisplayRole) << QVariant(QString());
    QTest::newRow("decoration") << int(Qt::DecorationRole) << QVariant(QIcon());

    QTest::newRow("AppId")
            << int(Clt::PlasmaWindowModel::AppId) << QVariant(QString());
    QTest::newRow("IsActive")
            << int(Clt::PlasmaWindowModel::IsActive) << QVariant(false);
    QTest::newRow("IsFullscreenable")
            << int(Clt::PlasmaWindowModel::IsFullscreenable) << QVariant(false);
    QTest::newRow("IsFullscreen")
            << int(Clt::PlasmaWindowModel::IsFullscreen) << QVariant(false);
    QTest::newRow("IsMaximizable")
            << int(Clt::PlasmaWindowModel::IsMaximizable) << QVariant(false);
    QTest::newRow("IsMaximized")
            << int(Clt::PlasmaWindowModel::IsMaximized) << QVariant(false);
    QTest::newRow("IsMinimizable")
            << int(Clt::PlasmaWindowModel::IsMinimizable) << QVariant(false);
    QTest::newRow("IsMinimized")
            << int(Clt::PlasmaWindowModel::IsMinimized) << QVariant(false);
    QTest::newRow("IsKeepAbove")
            << int(Clt::PlasmaWindowModel::IsKeepAbove) << QVariant(false);
    QTest::newRow("IsKeepBelow")
            << int(Clt::PlasmaWindowModel::IsKeepBelow) << QVariant(false);
    QTest::newRow("IsOnAllDesktops")
            << int(Clt::PlasmaWindowModel::IsOnAllDesktops) << QVariant(true);
    QTest::newRow("IsDemandingAttention")
            << int(Clt::PlasmaWindowModel::IsDemandingAttention) << QVariant(false);
    QTest::newRow("IsShadeable")
            << int(Clt::PlasmaWindowModel::IsShadeable) << QVariant(false);
    QTest::newRow("IsShaded")
            << int(Clt::PlasmaWindowModel::IsShaded) << QVariant(false);
    QTest::newRow("SkipTaskbar")
            << int(Clt::PlasmaWindowModel::SkipTaskbar) << QVariant(false);
    QTest::newRow("IsMovable")
            << int(Clt::PlasmaWindowModel::IsMovable) << QVariant(false);
    QTest::newRow("IsResizable")
            << int(Clt::PlasmaWindowModel::IsResizable) << QVariant(false);
    QTest::newRow("IsVirtualDesktopChangeable")
            << int(Clt::PlasmaWindowModel::IsVirtualDesktopChangeable) << QVariant(false);
    QTest::newRow("IsCloseable")
            << int(Clt::PlasmaWindowModel::IsCloseable) << QVariant(false);
    QTest::newRow("Geometry")
            << int(Clt::PlasmaWindowModel::Geometry) << QVariant(QRect());
    QTest::newRow("Pid")
            << int(Clt::PlasmaWindowModel::Pid) << QVariant(0);
}

void PlasmaWindowModelTest::testDefaultData()
{
    // This test validates the default data of a PlasmaWindow without having set any values.
    // First create a model with a window.
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(serverWindow);
    QVERIFY(rowInsertedSpy.wait());

    QModelIndex index = model->index(0);
    QFETCH(int, role);
    QTEST(model->data(index, role), "value");
}

void PlasmaWindowModelTest::testIsActive()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsActive,
                            &Srv::PlasmaWindow::setActive));
}

void PlasmaWindowModelTest::testIsFullscreenable()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsFullscreenable,
                            &Srv::PlasmaWindow::setFullscreenable));
}

void PlasmaWindowModelTest::testIsFullscreen()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsFullscreen,
                            &Srv::PlasmaWindow::setFullscreen));
}

void PlasmaWindowModelTest::testIsMaximizable()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsMaximizable,
                            &Srv::PlasmaWindow::setMaximizeable));
}

void PlasmaWindowModelTest::testIsMaximized()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsMaximized,
                            &Srv::PlasmaWindow::setMaximized));
}

void PlasmaWindowModelTest::testIsMinimizable()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsMinimizable,
                            &Srv::PlasmaWindow::setMinimizeable));
}

void PlasmaWindowModelTest::testIsMinimized()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsMinimized,
                            &Srv::PlasmaWindow::setMinimized));
}

void PlasmaWindowModelTest::testIsKeepAbove()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsKeepAbove,
                            &Srv::PlasmaWindow::setKeepAbove));
}

void PlasmaWindowModelTest::testIsKeepBelow()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsKeepBelow,
                            &Srv::PlasmaWindow::setKeepBelow));
}

void PlasmaWindowModelTest::testIsDemandingAttention()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsDemandingAttention,
                            &Srv::PlasmaWindow::setDemandsAttention));
}

void PlasmaWindowModelTest::testSkipTaskbar()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::SkipTaskbar,
                            &Srv::PlasmaWindow::setSkipTaskbar));
}

void PlasmaWindowModelTest::testSkipSwitcher()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::SkipSwitcher,
                            &Srv::PlasmaWindow::setSkipSwitcher));
}

void PlasmaWindowModelTest::testIsShadeable()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsShadeable,
                            &Srv::PlasmaWindow::setShadeable));
}

void PlasmaWindowModelTest::testIsShaded()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsShaded,
                            &Srv::PlasmaWindow::setShaded));
}

void PlasmaWindowModelTest::testIsMovable()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsMovable,
                            &Srv::PlasmaWindow::setMovable));
}

void PlasmaWindowModelTest::testIsResizable()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsResizable,
                            &Srv::PlasmaWindow::setResizable));
}

void PlasmaWindowModelTest::testIsVirtualDesktopChangeable()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsVirtualDesktopChangeable,
                            &Srv::PlasmaWindow::setVirtualDesktopChangeable));
}

void PlasmaWindowModelTest::testIsCloseable()
{
    QVERIFY(testBooleanData(Clt::PlasmaWindowModel::IsCloseable,
                            &Srv::PlasmaWindow::setCloseable));
}

void PlasmaWindowModelTest::testGeometry()
{
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(serverWindow);
    QVERIFY(rowInsertedSpy.wait());

    const QModelIndex index = model->index(0);

    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::Geometry).toRect(), QRect());

    QSignalSpy dataChangedSpy(model, &Clt::PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());

    const QRect geom(0, 15, 50, 75);
    serverWindow->setGeometry(geom);

    QVERIFY(dataChangedSpy.wait());

    // An icon and the geometry will be sent.
    QTRY_COMPARE(dataChangedSpy.count(), 2);
    QCOMPARE(dataChangedSpy.first().first().toModelIndex(), index);

    // The icon is received with QtConcurrent in the beginning. So it can arrive before or after
    // the geometry.
    const bool last = dataChangedSpy[0].last().value<QVector<int>>()
                        == QVector<int>{int(Qt::DecorationRole)};

    QCOMPARE(dataChangedSpy[last ? 1 : 0].last().value<QVector<int>>(),
             QVector<int>{int(Clt::PlasmaWindowModel::Geometry)});

    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::Geometry).toRect(), geom);
}

void PlasmaWindowModelTest::testTitle()
{
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(serverWindow);
    QVERIFY(rowInsertedSpy.wait());

    m_connection->flush();
    m_display->dispatchEvents();

    QSignalSpy dataChangedSpy(model, &Clt::PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());

    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, Qt::DisplayRole).toString(), QString());

    serverWindow->setTitle(QStringLiteral("foo"));
    QVERIFY(dataChangedSpy.wait());

    // An icon and the title will be sent.
    QTRY_COMPARE(dataChangedSpy.count(), 2);
    QCOMPARE(dataChangedSpy.first().first().toModelIndex(), index);

    // The icon is received with QtConcurrent in the beginning. So it can arrive before or after
    // the title.
    const bool last = dataChangedSpy[0].last().value<QVector<int>>()
                        == QVector<int>{int(Qt::DecorationRole)};

    QCOMPARE(dataChangedSpy[last ? 1 : 0].last().value<QVector<int>>(),
             QVector<int>{int(Qt::DisplayRole)});
    QCOMPARE(model->data(index, Qt::DisplayRole).toString(), QStringLiteral("foo"));
}

void PlasmaWindowModelTest::testAppId()
{
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(serverWindow);
    QVERIFY(rowInsertedSpy.wait());

    m_connection->flush();
    m_display->dispatchEvents();

    QSignalSpy dataChangedSpy(model, &Clt::PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());

    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::AppId).toString(), QString());

    serverWindow->setAppId(QStringLiteral("org.kde.testapp"));
    QVERIFY(dataChangedSpy.count() || dataChangedSpy.wait());

    // The App Id and the geometry will be sent.
    QTRY_COMPARE(dataChangedSpy.count(), 2);
    QCOMPARE(dataChangedSpy.first().first().toModelIndex(), index);

    // The icon is received with QtConcurrent in the beginning. So it can arrive before or after
    // the app id.
    const bool last = dataChangedSpy[0].last().value<QVector<int>>()
                        == QVector<int>{int(Qt::DecorationRole)};

    QCOMPARE(dataChangedSpy[last ? 1 : 0].last().value<QVector<int>>(),
             QVector<int>{int(Clt::PlasmaWindowModel::AppId)});
    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::AppId).toString(),
             QStringLiteral("org.kde.testapp"));
}

void PlasmaWindowModelTest::testPid()
{
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    serverWindow->setPid(1337);
    QVERIFY(serverWindow);

    m_connection->flush();
    m_display->dispatchEvents();
    QVERIFY(rowInsertedSpy.wait());

    //pid should be set as soon as the new row appears
    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::Pid).toInt(), 1337);
}

void PlasmaWindowModelTest::testVirtualDesktops()
{
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());
    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(serverWindow);
    QVERIFY(rowInsertedSpy.wait());

    m_connection->flush();
    m_display->dispatchEvents();
    QSignalSpy dataChangedSpy(model, &Clt::PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());

    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::VirtualDesktops).toStringList(),
             QStringList());

    serverWindow->addPlasmaVirtualDesktop("desktop1");
    QVERIFY(dataChangedSpy.wait());
    QTRY_COMPARE(dataChangedSpy.count(), 3);

    QCOMPARE(dataChangedSpy.first().first().toModelIndex(), index);
    QCOMPARE(dataChangedSpy.last().first().toModelIndex(), index);

    // The icon is received with QtConcurrent in the beginning. So it can arrive before or after
    // the virtual desktop.
    const bool last = dataChangedSpy[0].last().value<QVector<int>>()
                        == QVector<int>{int(Qt::DecorationRole)};

    QCOMPARE(dataChangedSpy[last ? 1 : 0].last().value<QVector<int>>(),
             QVector<int>{int(Clt::PlasmaWindowModel::VirtualDesktops)});
    QCOMPARE(dataChangedSpy[last ? 2 : 1].last().value<QVector<int>>(),
             QVector<int>{int(Clt::PlasmaWindowModel::IsOnAllDesktops)});

    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::VirtualDesktops).toStringList(),
             QStringList({"desktop1"}));
    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::IsOnAllDesktops).toBool(), false);

    dataChangedSpy.clear();
    QVERIFY(!dataChangedSpy.count());

    serverWindow->addPlasmaVirtualDesktop("desktop2");
    QVERIFY(dataChangedSpy.wait());
    QTRY_COMPARE(dataChangedSpy.count(), 1);

    QCOMPARE(dataChangedSpy.first().first().toModelIndex(), index);
    QCOMPARE(dataChangedSpy.first().last().value<QVector<int>>(),
             QVector<int>{int(Clt::PlasmaWindowModel::VirtualDesktops)});

    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::VirtualDesktops).toStringList(),
             QStringList({"desktop1", "desktop2"}));
    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::IsOnAllDesktops).toBool(), false);

    serverWindow->removePlasmaVirtualDesktop("desktop2");
    serverWindow->removePlasmaVirtualDesktop("desktop1");

    QVERIFY(dataChangedSpy.wait());
    QTRY_COMPARE(dataChangedSpy.count(), 5);

    QCOMPARE(dataChangedSpy.last().last().value<QVector<int>>(),
             QVector<int>{int(Clt::PlasmaWindowModel::IsOnAllDesktops)});

    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::VirtualDesktops).toStringList(),
             QStringList({}));
    QCOMPARE(model->data(index, Clt::PlasmaWindowModel::IsOnAllDesktops).toBool(), true);
    QVERIFY(!dataChangedSpy.wait(100));
}

void PlasmaWindowModelTest::testRequests()
{
    // This test verifies that the various requests are properly passed to the server.
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy rowInsertedSpy(model, &Clt::PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(serverWindow);
    QVERIFY(rowInsertedSpy.wait());

    QSignalSpy activateRequestedSpy(serverWindow, &Srv::PlasmaWindow::activeRequested);
    QVERIFY(activateRequestedSpy.isValid());
    QSignalSpy closeRequestedSpy(serverWindow, &Srv::PlasmaWindow::closeRequested);
    QVERIFY(closeRequestedSpy.isValid());
    QSignalSpy moveRequestedSpy(serverWindow, &Srv::PlasmaWindow::moveRequested);
    QVERIFY(moveRequestedSpy.isValid());
    QSignalSpy resizeRequestedSpy(serverWindow, &Srv::PlasmaWindow::resizeRequested);
    QVERIFY(resizeRequestedSpy.isValid());
    QSignalSpy keepAboveRequestedSpy(serverWindow, &Srv::PlasmaWindow::keepAboveRequested);
    QVERIFY(keepAboveRequestedSpy.isValid());
    QSignalSpy keepBelowRequestedSpy(serverWindow, &Srv::PlasmaWindow::keepBelowRequested);
    QVERIFY(keepBelowRequestedSpy.isValid());
    QSignalSpy minimizedRequestedSpy(serverWindow, &Srv::PlasmaWindow::minimizedRequested);
    QVERIFY(minimizedRequestedSpy.isValid());
    QSignalSpy maximizeRequestedSpy(serverWindow, &Srv::PlasmaWindow::maximizedRequested);
    QVERIFY(maximizeRequestedSpy.isValid());
    QSignalSpy shadeRequestedSpy(serverWindow, &Srv::PlasmaWindow::shadedRequested);
    QVERIFY(shadeRequestedSpy.isValid());

    // First let's use some invalid row numbers.
    model->requestActivate(-1);
    model->requestClose(-1);
    model->requestToggleKeepAbove(-1);
    model->requestToggleKeepBelow(-1);
    model->requestToggleMinimized(-1);
    model->requestToggleMaximized(-1);
    model->requestActivate(1);
    model->requestClose(1);
    model->requestMove(1);
    model->requestResize(1);
    model->requestToggleKeepAbove(1);
    model->requestToggleKeepBelow(1);
    model->requestToggleMinimized(1);
    model->requestToggleMaximized(1);
    model->requestToggleShaded(1);

    // That should not have triggered any signals.
    QVERIFY(!activateRequestedSpy.wait(100));
    QVERIFY(activateRequestedSpy.isEmpty());
    QVERIFY(closeRequestedSpy.isEmpty());
    QVERIFY(moveRequestedSpy.isEmpty());
    QVERIFY(resizeRequestedSpy.isEmpty());
    QVERIFY(minimizedRequestedSpy.isEmpty());
    QVERIFY(maximizeRequestedSpy.isEmpty());
    QVERIFY(shadeRequestedSpy.isEmpty());

    // Now with the proper row.
    // Activate
    model->requestActivate(0);
    QVERIFY(activateRequestedSpy.wait());
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(activateRequestedSpy.first().first().toBool(), true);
    QCOMPARE(closeRequestedSpy.count(), 0);
    QCOMPARE(moveRequestedSpy.count(), 0);
    QCOMPARE(resizeRequestedSpy.count(), 0);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Close
    model->requestClose(0);
    QVERIFY(closeRequestedSpy.wait());
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 0);
    QCOMPARE(resizeRequestedSpy.count(), 0);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Move
    model->requestMove(0);
    QVERIFY(moveRequestedSpy.wait());
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 0);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Resize
    model->requestResize(0);
    QVERIFY(resizeRequestedSpy.wait());
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Virtual desktop
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Keep above
    model->requestToggleKeepAbove(0);
    QVERIFY(keepAboveRequestedSpy.wait());
    QCOMPARE(keepAboveRequestedSpy.count(), 1);
    QCOMPARE(keepAboveRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Keep below
    model->requestToggleKeepBelow(0);
    QVERIFY(keepBelowRequestedSpy.wait());
    QCOMPARE(keepBelowRequestedSpy.count(), 1);
    QCOMPARE(keepBelowRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Minimize
    model->requestToggleMinimized(0);
    QVERIFY(minimizedRequestedSpy.wait());
    QCOMPARE(minimizedRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Maximize
    model->requestToggleMaximized(0);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.count(), 1);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // Shade
    model->requestToggleShaded(0);
    QVERIFY(shadeRequestedSpy.wait());
    QCOMPARE(shadeRequestedSpy.count(), 1);
    QCOMPARE(shadeRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.count(), 1);

    // The toggles can also support a different state.
    QSignalSpy dataChangedSpy(model, &Clt::PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());
    // Keep above
    serverWindow->setKeepAbove(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleKeepAbove(0);
    QVERIFY(keepAboveRequestedSpy.wait());
    QCOMPARE(keepAboveRequestedSpy.count(), 2);
    QCOMPARE(keepAboveRequestedSpy.last().first().toBool(), false);
    // Keep below
    serverWindow->setKeepBelow(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleKeepBelow(0);
    QVERIFY(keepBelowRequestedSpy.wait());
    QCOMPARE(keepBelowRequestedSpy.count(), 2);
    QCOMPARE(keepBelowRequestedSpy.last().first().toBool(), false);
    // Minimize
    serverWindow->setMinimized(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleMinimized(0);
    QVERIFY(minimizedRequestedSpy.wait());
    QCOMPARE(minimizedRequestedSpy.count(), 2);
    QCOMPARE(minimizedRequestedSpy.last().first().toBool(), false);
    // Maximized
    serverWindow->setMaximized(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleMaximized(0);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 2);
    QCOMPARE(maximizeRequestedSpy.last().first().toBool(), false);
    // Shaded
    serverWindow->setShaded(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleShaded(0);
    QVERIFY(shadeRequestedSpy.wait());
    QCOMPARE(shadeRequestedSpy.count(), 2);
    QCOMPARE(shadeRequestedSpy.last().first().toBool(), false);
}

void PlasmaWindowModelTest::testCreateWithUnmappedWindow()
{
    // This test verifies that creating the model just when an unmapped window exists doesn't cause
    // problems.
    // That is the unmapped window should be added (as expected), but also be removed again.

    // Create a window in "normal way".
    QSignalSpy windowCreatedSpy(m_pw, &Clt::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowCreatedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(serverWindow);
    QVERIFY(windowCreatedSpy.wait());

    auto *window = windowCreatedSpy.first().first().value<Clt::PlasmaWindow*>();
    QVERIFY(window);

    // Make sure the resource is properly created on server side.
    QCoreApplication::instance()->processEvents(QEventLoop::WaitForMoreEvents);

    QSignalSpy unmappedSpy(window, &Clt::PlasmaWindow::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy destroyedSpy(window, &Clt::PlasmaWindow::destroyed);
    QVERIFY(destroyedSpy.isValid());

    // Unmap should be triggered, but not yet the destroyed.
    serverWindow->unmap();
    QVERIFY(unmappedSpy.wait());
    QVERIFY(destroyedSpy.isEmpty());

    auto *model = m_pw->createWindowModel();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 1);
    QSignalSpy rowRemovedSpy(model, &Clt::PlasmaWindowModel::rowsRemoved);
    QVERIFY(rowRemovedSpy.isValid());
    QVERIFY(rowRemovedSpy.wait());
    QCOMPARE(rowRemovedSpy.count(), 1);
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(destroyedSpy.count(), 1);
}

void PlasmaWindowModelTest::testChangeWindowAfterModelDestroy_data()
{
    QTest::addColumn<ClientWindowSignal>("changedSignal");
    QTest::addColumn<QVariant>("setter");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("active")
            << &Clt::PlasmaWindow::activeChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setActive)
            << QVariant(true);
    QTest::newRow("minimized")
            << &Clt::PlasmaWindow::minimizedChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setMinimized)
            << QVariant(true);
    QTest::newRow("fullscreen")
            << &Clt::PlasmaWindow::fullscreenChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setFullscreen)
            << QVariant(true);
    QTest::newRow("keepAbove")
            << &Clt::PlasmaWindow::keepAboveChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setKeepAbove)
            << QVariant(true);
    QTest::newRow("keepBelow")
            << &Clt::PlasmaWindow::keepBelowChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setKeepBelow)
            << QVariant(true);
    QTest::newRow("maximized")
            << &Clt::PlasmaWindow::maximizedChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setMaximized)
            << QVariant(true);
    QTest::newRow("demandsAttention")
            << &Clt::PlasmaWindow::demandsAttentionChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setDemandsAttention)
            << QVariant(true);
    QTest::newRow("closeable")
            << &Clt::PlasmaWindow::closeableChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setCloseable)
            << QVariant(true);
    QTest::newRow("minimizeable")
            << &Clt::PlasmaWindow::minimizeableChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setMinimizeable)
            << QVariant(true);
    QTest::newRow("maximizeable")
            << &Clt::PlasmaWindow::maximizeableChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setMaximizeable)
            << QVariant(true);
    QTest::newRow("fullscreenable")
            << &Clt::PlasmaWindow::fullscreenableChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setFullscreenable)
            << QVariant(true);
    QTest::newRow("skipTaskbar")
            << &Clt::PlasmaWindow::skipTaskbarChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setSkipTaskbar)
            << QVariant(true);
    QTest::newRow("shadeable")
            << &Clt::PlasmaWindow::shadeableChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setShadeable)
            << QVariant(true);
    QTest::newRow("shaded")
            << &Clt::PlasmaWindow::shadedChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setShaded)
            << QVariant(true);
    QTest::newRow("movable")
            << &Clt::PlasmaWindow::movableChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setMovable)
            << QVariant(true);
    QTest::newRow("resizable")
            << &Clt::PlasmaWindow::resizableChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setResizable)
            << QVariant(true);
    QTest::newRow("vdChangeable")
            << &Clt::PlasmaWindow::virtualDesktopChangeableChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setVirtualDesktopChangeable)
            << QVariant(true);
    QTest::newRow("onallDesktop")
            << &Clt::PlasmaWindow::onAllDesktopsChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setOnAllDesktops)
            << QVariant(true);
    QTest::newRow("title")
            << &Clt::PlasmaWindow::titleChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setTitle)
            << QVariant(QStringLiteral("foo"));
    QTest::newRow("appId")
            << &Clt::PlasmaWindow::appIdChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setAppId)
            << QVariant(QStringLiteral("foo"));

    // Disable the icon test for now. Our way of providing icons is fundamentally wrong and the
    // whole concept needs to be redone so it works on all setups and in particular in a CI setting.
    // See issue #8.
#if 0
    QTest::newRow("icon" )
            << &Clt::PlasmaWindow::iconChanged
            << QVariant::fromValue(&Srv::PlasmaWindow::setIcon)
            << QVariant::fromValue(QIcon::fromTheme(QStringLiteral("foo")));
#endif

    QTest::newRow("unmapped")
            << &Clt::PlasmaWindow::unmapped
            << QVariant::fromValue(&Srv::PlasmaWindow::unmap)
            << QVariant();
}

void PlasmaWindowModelTest::testChangeWindowAfterModelDestroy()
{
    // This test verifies that changes in a window after the model got destroyed doesn't crash.
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy windowCreatedSpy(m_pw, &Clt::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowCreatedSpy.isValid());

    auto *serverWindow = m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(windowCreatedSpy.wait());
    Clt::PlasmaWindow *window = windowCreatedSpy.first().first().value<Clt::PlasmaWindow*>();

    // Make sure the resource is properly created on server side.
    QCoreApplication::instance()->processEvents(QEventLoop::WaitForMoreEvents);
    QCOMPARE(model->rowCount(), 1);
    delete model;

    QFETCH(ClientWindowSignal, changedSignal);
    QSignalSpy changedSpy(window, changedSignal);
    QVERIFY(changedSpy.isValid());
    QVERIFY(!window->isActive());

    QFETCH(QVariant, setter);
    QFETCH(QVariant, value);

    if (QMetaType::Type(value.type()) == QMetaType::Bool) {
        (serverWindow->*(setter.value<ServerWindowBoolSetter>()))(value.toBool());
    } else if (QMetaType::Type(value.type()) == QMetaType::QString) {
        (serverWindow->*(setter.value<ServerWindowStringSetter>()))(value.toString());
    } else if (QMetaType::Type(value.type()) == QMetaType::UInt) {
        (serverWindow->*(setter.value<ServerWindowQuint32Setter>()))(value.toUInt());
    } else if (!value.isValid()) {
        (serverWindow->*(setter.value<ServerWindowVoidSetter>()))();
    }

    QVERIFY(changedSpy.wait());
}

void PlasmaWindowModelTest::testCreateWindowAfterModelDestroy()
{
    // This test verifies that creating a window after the model got destroyed doesn't crash.
    auto *model = m_pw->createWindowModel();
    QVERIFY(model);

    delete model;

    QSignalSpy windowCreatedSpy(m_pw, &Clt::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowCreatedSpy.isValid());

    m_serverWindowManager->createWindow(m_serverWindowManager);
    QVERIFY(windowCreatedSpy.wait());
}

QTEST_GUILESS_MAIN(PlasmaWindowModelTest)
#include "plasma_window_model.moc"
