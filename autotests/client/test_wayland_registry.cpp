/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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

#include "../../server/display.h"

#include "../../server/compositor.h"
#include "../../server/data_device_manager.h"
#include "../../server/dpms.h"
#include "../../server/output.h"
#include "../../server/seat.h"
#include "../../server/subcompositor.h"

#include "../../src/client/blur.h"
#include "../../src/client/contrast.h"
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/dpms.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/output.h"
#include "../../src/client/pointerconstraints.h"
#include "../../src/client/pointergestures.h"
#include "../../src/client/idleinhibit.h"
#include "../../src/client/seat.h"
#include "../../src/client/relativepointer.h"
#include "../../src/client/server_decoration.h"
#include "../../src/client/shell.h"
#include "../../src/client/surface.h"
#include "../../src/client/subcompositor.h"
#include "../../src/client/xdgshell.h"

#include "../../src/server/idleinhibit_interface.h"
#include "../../src/server/output_interface.h"
#include "../../src/server/shell_interface.h"
#include "../../src/server/blur_interface.h"
#include "../../src/server/contrast_interface.h"
#include "../../src/server/server_decoration_interface.h"
#include "../../src/server/slide_interface.h"
#include "../../src/server/output_management_v1_interface.h"
#include "../../src/server/output_device_v1_interface.h"
#include "../../src/server/textinput_interface.h"
#include "../../src/server/xdgshell_interface.h"
// Wayland
#include <wayland-blur-client-protocol.h>
#include <wayland-client-protocol.h>
#include <wayland-contrast-client-protocol.h>
#include <wayland-dpms-client-protocol.h>
#include <wayland-idle-inhibit-unstable-v1-client-protocol.h>
#include <wayland-server-decoration-client-protocol.h>
#include <wayland-slide-client-protocol.h>
#include <wayland-text-input-v0-client-protocol.h>
#include <wayland-text-input-v2-client-protocol.h>
#include <wayland-relativepointer-unstable-v1-client-protocol.h>
#include <wayland-pointer-gestures-unstable-v1-client-protocol.h>
#include <wayland-pointer-constraints-unstable-v1-client-protocol.h>
#include "../../src/compat/wayland-xdg-shell-v5-client-protocol.h"

class TestWaylandRegistry : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandRegistry(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testBindCompositor();
    void testBindShell();
    void testBindOutput();
    void testBindShm();
    void testBindSeat();
    void testBindSubCompositor();
    void testBindDataDeviceManager();
    void testBindBlurManager();
    void testBindContrastManager();
    void testBindSlideManager();
    void testBindDpmsManager();
    void testBindServerSideDecorationManager();
    void testBindTextInputManagerUnstableV0();
    void testBindTextInputManagerUnstableV2();
    void testBindXdgShellUnstableV5();
    void testBindRelativePointerManagerUnstableV1();
    void testBindPointerGesturesUnstableV1();
    void testBindPointerConstraintsUnstableV1();
    void testBindIdleIhibitManagerUnstableV1();
    void testGlobalSync();
    void testGlobalSyncThreaded();
    void testRemoval();
    void testOutOfSyncRemoval();
    void testDestroy();
    void testAnnounceMultiple();
    void testAnnounceMultipleOutputDeviceV1s();

private:
    Wrapland::Server::D_isplay *m_display;
    Wrapland::Server::Compositor *m_compositor;
    Wrapland::Server::Output *m_output;
    Wrapland::Server::OutputDeviceV1Interface *m_outputDevice;
    Wrapland::Server::Seat* m_seat;
    Wrapland::Server::ShellInterface *m_shell;
    Wrapland::Server::Subcompositor *m_subcompositor;
    Wrapland::Server::DataDeviceManager* m_dataDeviceManager;
    Wrapland::Server::OutputManagementV1Interface *m_outputManagement;
    Wrapland::Server::ServerSideDecorationManagerInterface *m_serverSideDecorationManager;
    Wrapland::Server::TextInputManagerInterface *m_textInputManagerV0;
    Wrapland::Server::TextInputManagerInterface *m_textInputManagerV2;
    Wrapland::Server::XdgShellInterface *m_xdgShellUnstableV5;
    Wrapland::Server::RelativePointerManagerV1* m_relativePointerV1;
    Wrapland::Server::PointerGesturesV1* m_pointerGesturesV1;
    Wrapland::Server::PointerConstraintsV1* m_pointerConstraintsV1;
    Wrapland::Server::BlurManagerInterface *m_blur;
    Wrapland::Server::ContrastManagerInterface *m_contrast;
    Wrapland::Server::IdleInhibitManagerInterface *m_idleInhibit;

};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-registry-0");

TestWaylandRegistry::TestWaylandRegistry(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositor(nullptr)
    , m_output(nullptr)
    , m_outputDevice(nullptr)
    , m_seat(nullptr)
    , m_shell(nullptr)
    , m_subcompositor(nullptr)
    , m_dataDeviceManager(nullptr)
    , m_outputManagement(nullptr)
    , m_serverSideDecorationManager(nullptr)
    , m_textInputManagerV0(nullptr)
    , m_textInputManagerV2(nullptr)
    , m_xdgShellUnstableV5(nullptr)
    , m_relativePointerV1(nullptr)
    , m_pointerGesturesV1(nullptr)
    , m_pointerConstraintsV1(nullptr)
    , m_blur(nullptr)
    , m_contrast(nullptr)
    , m_idleInhibit(nullptr)
{
}

void TestWaylandRegistry::init()
{
    m_display = new Wrapland::Server::D_isplay();
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_display->createShm();
    m_compositor = m_display->createCompositor();
    m_output = m_display->createOutput();
    m_seat = m_display->createSeat();
    m_shell = m_display->createShell();
    m_shell->create();
    m_subcompositor = m_display->createSubCompositor();
    m_dataDeviceManager = m_display->createDataDeviceManager();
    m_outputManagement = m_display->createOutputManagementV1();
    m_outputManagement->create();
    m_outputDevice = m_display->createOutputDeviceV1();
    m_outputDevice->create();
    QVERIFY(m_outputManagement->isValid());
    m_blur = m_display->createBlurManager(this);
    m_blur->create();
    m_contrast = m_display->createContrastManager(this);
    m_contrast->create();
    m_display->createSlideManager(this)->create();
    m_display->createDpmsManager();
    m_serverSideDecorationManager = m_display->createServerSideDecorationManager();
    m_serverSideDecorationManager->create();
    m_textInputManagerV0 = m_display->createTextInputManager(Wrapland::Server::TextInputInterfaceVersion::UnstableV0);
    QCOMPARE(m_textInputManagerV0->interfaceVersion(), Wrapland::Server::TextInputInterfaceVersion::UnstableV0);
    m_textInputManagerV0->create();
    m_textInputManagerV2 = m_display->createTextInputManager(Wrapland::Server::TextInputInterfaceVersion::UnstableV2);
    QCOMPARE(m_textInputManagerV2->interfaceVersion(), Wrapland::Server::TextInputInterfaceVersion::UnstableV2);
    m_textInputManagerV2->create();
    m_xdgShellUnstableV5 = m_display->createXdgShell(Wrapland::Server::XdgShellInterfaceVersion::UnstableV5);
    m_xdgShellUnstableV5->create();
    QCOMPARE(m_xdgShellUnstableV5->interfaceVersion(), Wrapland::Server::XdgShellInterfaceVersion::UnstableV5);
    m_relativePointerV1 = m_display->createRelativePointerManager();
    m_pointerGesturesV1 = m_display->createPointerGestures();
    m_pointerConstraintsV1 = m_display->createPointerConstraints();
    m_idleInhibit = m_display->createIdleInhibitManager(Wrapland::Server::IdleInhibitManagerInterfaceVersion::UnstableV1);
    m_idleInhibit->create();
    QCOMPARE(m_idleInhibit->interfaceVersion(), Wrapland::Server::IdleInhibitManagerInterfaceVersion::UnstableV1);
}

void TestWaylandRegistry::cleanup()
{
    delete m_display;
    m_display = nullptr;
}

void TestWaylandRegistry::testCreate()
{
    Wrapland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    connection.setSocketName(s_socketName);
    connection.establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    Wrapland::Client::Registry registry;
    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.release();
    QVERIFY(!registry.isValid());
}

#define TEST_BIND(iface, signalName, bindMethod, destroyFunction) \
    Wrapland::Client::ConnectionThread connection; \
    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged); \
    connection.setSocketName(s_socketName); \
    connection.establishConnection(); \
    QVERIFY(connectedSpy.count() || connectedSpy.wait()); \
    QCOMPARE(connectedSpy.count(), 1); \
    \
    Wrapland::Client::Registry registry; \
    /* before registry is created, we cannot bind the interface*/ \
    QVERIFY(!registry.bindMethod(0, 0)); \
    \
    QVERIFY(!registry.isValid()); \
    registry.create(&connection); \
    QVERIFY(registry.isValid()); \
    /* created but not yet connected still results in no bind */ \
    QVERIFY(!registry.bindMethod(0, 0)); \
    /* interface information should be empty */ \
    QVERIFY(registry.interfaces(iface).isEmpty()); \
    QCOMPARE(registry.interface(iface).name, 0u); \
    QCOMPARE(registry.interface(iface).version, 0u); \
    \
    /* now let's register */ \
    QSignalSpy announced(&registry, signalName); \
    QVERIFY(announced.isValid()); \
    registry.setup(); \
    wl_display_flush(connection.display()); \
    QVERIFY(announced.wait()); \
    const quint32 name = announced.first().first().value<quint32>(); \
    const quint32 version = announced.first().last().value<quint32>(); \
    QCOMPARE(registry.interfaces(iface).count(), 1); \
    QCOMPARE(registry.interfaces(iface).first().name, name); \
    QCOMPARE(registry.interfaces(iface).first().version, version); \
    QCOMPARE(registry.interface(iface).name, name); \
    QCOMPARE(registry.interface(iface).version, version); \
    \
    /* registry should know about the interface now */ \
    QVERIFY(registry.hasInterface(iface)); \
    QVERIFY(!registry.bindMethod(name+1, version)); \
    auto *higherVersion = registry.bindMethod(name, version+1); \
    QVERIFY(higherVersion); \
    destroyFunction(higherVersion); \
    auto *c = registry.bindMethod(name, version); \
    QVERIFY(c); \
    destroyFunction(c); \

void TestWaylandRegistry::testBindCompositor()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Compositor, SIGNAL(compositorAnnounced(quint32,quint32)), bindCompositor, wl_compositor_destroy)
}

void TestWaylandRegistry::testBindShell()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Shell, SIGNAL(shellAnnounced(quint32,quint32)), bindShell, free)
}

void TestWaylandRegistry::testBindOutput()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Output, SIGNAL(outputAnnounced(quint32,quint32)), bindOutput, wl_output_destroy)
}

void TestWaylandRegistry::testBindSeat()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Seat, SIGNAL(seatAnnounced(quint32,quint32)), bindSeat, wl_seat_destroy)
}

void TestWaylandRegistry::testBindShm()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Shm, SIGNAL(shmAnnounced(quint32,quint32)), bindShm, wl_shm_destroy)
}

void TestWaylandRegistry::testBindSubCompositor()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::SubCompositor, SIGNAL(subCompositorAnnounced(quint32,quint32)), bindSubCompositor, wl_subcompositor_destroy)
}

void TestWaylandRegistry::testBindDataDeviceManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::DataDeviceManager, SIGNAL(dataDeviceManagerAnnounced(quint32,quint32)), bindDataDeviceManager, wl_data_device_manager_destroy)
}

void TestWaylandRegistry::testBindBlurManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Blur,
              SIGNAL(blurAnnounced(quint32,quint32)), bindBlurManager,
              org_kde_kwin_blur_manager_destroy)
}

void TestWaylandRegistry::testBindContrastManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Contrast,
              SIGNAL(contrastAnnounced(quint32,quint32)), bindContrastManager,
              org_kde_kwin_contrast_manager_destroy)
}

void TestWaylandRegistry::testBindSlideManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Slide,
              SIGNAL(slideAnnounced(quint32,quint32)), bindSlideManager,
              org_kde_kwin_slide_manager_destroy)
}

void TestWaylandRegistry::testBindDpmsManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Dpms, SIGNAL(dpmsAnnounced(quint32,quint32)), bindDpmsManager, org_kde_kwin_dpms_manager_destroy)
}

void TestWaylandRegistry::testBindServerSideDecorationManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::ServerSideDecorationManager, SIGNAL(serverSideDecorationManagerAnnounced(quint32,quint32)), bindServerSideDecorationManager, org_kde_kwin_server_decoration_manager_destroy)
}

void TestWaylandRegistry::testBindTextInputManagerUnstableV0()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::TextInputManagerUnstableV0, SIGNAL(textInputManagerUnstableV0Announced(quint32,quint32)), bindTextInputManagerUnstableV0, wl_text_input_manager_destroy)
}

void TestWaylandRegistry::testBindTextInputManagerUnstableV2()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::TextInputManagerUnstableV2, SIGNAL(textInputManagerUnstableV2Announced(quint32,quint32)), bindTextInputManagerUnstableV2, zwp_text_input_manager_v2_destroy)
}

void TestWaylandRegistry::testBindXdgShellUnstableV5()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::XdgShellUnstableV5, SIGNAL(xdgShellUnstableV5Announced(quint32,quint32)), bindXdgShellUnstableV5, zxdg_shell_v5_destroy)
}

void TestWaylandRegistry::testBindRelativePointerManagerUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::RelativePointerManagerUnstableV1, SIGNAL(relativePointerManagerUnstableV1Announced(quint32,quint32)), bindRelativePointerManagerUnstableV1, zwp_relative_pointer_manager_v1_destroy)
}

void TestWaylandRegistry::testBindPointerGesturesUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::PointerGesturesUnstableV1, SIGNAL(pointerGesturesUnstableV1Announced(quint32,quint32)), bindPointerGesturesUnstableV1, zwp_pointer_gestures_v1_destroy)
}

void TestWaylandRegistry::testBindPointerConstraintsUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::PointerConstraintsUnstableV1, SIGNAL(pointerConstraintsUnstableV1Announced(quint32,quint32)), bindPointerConstraintsUnstableV1, zwp_pointer_constraints_v1_destroy)
}

void TestWaylandRegistry::testBindIdleIhibitManagerUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1, SIGNAL(idleInhibitManagerUnstableV1Announced(quint32,quint32)), bindIdleInhibitManagerUnstableV1, zwp_idle_inhibit_manager_v1_destroy)
    QTest::qWait(100);
}

#undef TEST_BIND

void TestWaylandRegistry::testRemoval()
{
    using namespace Wrapland::Client;
    Wrapland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    connection.setSocketName(s_socketName);
    connection.establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, &connection,
        [&connection] {
            wl_display_flush(connection.display());
        }
    );

    Wrapland::Client::Registry registry;
    QSignalSpy shmAnnouncedSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    QVERIFY(shmAnnouncedSpy.isValid());
    QSignalSpy compositorAnnouncedSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QVERIFY(compositorAnnouncedSpy.isValid());
    QSignalSpy outputAnnouncedSpy(&registry, SIGNAL(outputAnnounced(quint32,quint32)));
    QVERIFY(outputAnnouncedSpy.isValid());
    QSignalSpy outputDeviceAnnouncedSpy(&registry, SIGNAL(outputDeviceV1Announced(quint32,quint32)));
    QVERIFY(outputDeviceAnnouncedSpy.isValid());
    QSignalSpy shellAnnouncedSpy(&registry, SIGNAL(shellAnnounced(quint32,quint32)));
    QVERIFY(shellAnnouncedSpy.isValid());
    QSignalSpy seatAnnouncedSpy(&registry, SIGNAL(seatAnnounced(quint32,quint32)));
    QVERIFY(seatAnnouncedSpy.isValid());
    QSignalSpy subCompositorAnnouncedSpy(&registry, SIGNAL(subCompositorAnnounced(quint32,quint32)));
    QVERIFY(subCompositorAnnouncedSpy.isValid());
    QSignalSpy outputManagementAnnouncedSpy(&registry, SIGNAL(outputManagementV1Announced(quint32,quint32)));
    QVERIFY(outputManagementAnnouncedSpy.isValid());
    QSignalSpy serverSideDecorationManagerAnnouncedSpy(&registry, &Registry::serverSideDecorationManagerAnnounced);
    QVERIFY(serverSideDecorationManagerAnnouncedSpy.isValid());
    QSignalSpy blurAnnouncedSpy(&registry, &Registry::blurAnnounced);
    QVERIFY(blurAnnouncedSpy.isValid());
    QSignalSpy idleInhibitManagerUnstableV1AnnouncedSpy(&registry, &Registry::idleInhibitManagerUnstableV1Announced);
    QVERIFY(idleInhibitManagerUnstableV1AnnouncedSpy.isValid());

    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    registry.setup();

    QVERIFY(shmAnnouncedSpy.wait());
    QVERIFY(!compositorAnnouncedSpy.isEmpty());
    QVERIFY(!outputAnnouncedSpy.isEmpty());
    QVERIFY(!outputDeviceAnnouncedSpy.isEmpty());
    QVERIFY(!shellAnnouncedSpy.isEmpty());
    QVERIFY(!seatAnnouncedSpy.isEmpty());
    QVERIFY(!subCompositorAnnouncedSpy.isEmpty());
    QVERIFY(!outputManagementAnnouncedSpy.isEmpty());
    QVERIFY(!serverSideDecorationManagerAnnouncedSpy.isEmpty());
    QVERIFY(!blurAnnouncedSpy.isEmpty());
    QVERIFY(!idleInhibitManagerUnstableV1AnnouncedSpy.isEmpty());


    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Compositor));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Output));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::OutputDeviceV1));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Seat));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Shell));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Shm));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::SubCompositor));
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::FullscreenShell));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::OutputManagementV1));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::ServerSideDecorationManager));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Blur));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1));

    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Compositor).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Output).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::OutputDeviceV1).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Seat).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Shell).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Shm).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::SubCompositor).isEmpty());
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::FullscreenShell).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::OutputManagementV1).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::ServerSideDecorationManager).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Blur).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1).isEmpty());

    QSignalSpy seatRemovedSpy(&registry, SIGNAL(seatRemoved(quint32)));
    QVERIFY(seatRemovedSpy.isValid());

    Seat *seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name, registry.interface(Registry::Interface::Seat).version, &registry);
    Shell *shell = registry.createShell(registry.interface(Registry::Interface::Shell).name, registry.interface(Registry::Interface::Shell).version, &registry);
    Output *output = registry.createOutput(registry.interface(Registry::Interface::Output).name, registry.interface(Registry::Interface::Output).version, &registry);
    Compositor *compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name, registry.interface(Registry::Interface::Compositor).version, &registry);
    SubCompositor *subcompositor = registry.createSubCompositor(registry.interface(Registry::Interface::SubCompositor).name, registry.interface(Registry::Interface::SubCompositor).version, &registry);
    ServerSideDecorationManager *serverSideDeco = registry.createServerSideDecorationManager(registry.interface(Registry::Interface::ServerSideDecorationManager).name, registry.interface(Registry::Interface::ServerSideDecorationManager).version, &registry);
    BlurManager *blurManager = registry.createBlurManager(registry.interface(Registry::Interface::Blur).name, registry.interface(Registry::Interface::Blur).version, &registry);
    auto idleInhibitManager = registry.createIdleInhibitManager(registry.interface(Registry::Interface::IdleInhibitManagerUnstableV1).name, registry.interface(Registry::Interface::IdleInhibitManagerUnstableV1).version, &registry);

    connection.flush();
    m_display->dispatchEvents();
    QSignalSpy seatObjectRemovedSpy(seat, &Seat::removed);
    QVERIFY(seatObjectRemovedSpy.isValid());
    QSignalSpy shellObjectRemovedSpy(shell, &Shell::removed);
    QVERIFY(shellObjectRemovedSpy.isValid());
    QSignalSpy outputObjectRemovedSpy(output, &Output::removed);
    QVERIFY(outputObjectRemovedSpy.isValid());
    QSignalSpy compositorObjectRemovedSpy(compositor, &Compositor::removed);
    QVERIFY(compositorObjectRemovedSpy.isValid());
    QSignalSpy subcompositorObjectRemovedSpy(subcompositor, &SubCompositor::removed);
    QVERIFY(subcompositorObjectRemovedSpy.isValid());
    QSignalSpy idleInhibitManagerObjectRemovedSpy(idleInhibitManager, &IdleInhibitManager::removed);
    QVERIFY(idleInhibitManagerObjectRemovedSpy.isValid());

    delete m_seat;
    QVERIFY(seatRemovedSpy.wait());
    QCOMPARE(seatRemovedSpy.first().first(), seatAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Seat));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Seat).isEmpty());
    QCOMPARE(seatObjectRemovedSpy.count(), 1);

    QSignalSpy shellRemovedSpy(&registry, SIGNAL(shellRemoved(quint32)));
    QVERIFY(shellRemovedSpy.isValid());

    delete m_shell;
    QVERIFY(shellRemovedSpy.wait());
    QCOMPARE(shellRemovedSpy.first().first(), shellAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Shell));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Shell).isEmpty());
    QCOMPARE(shellObjectRemovedSpy.count(), 1);

    QSignalSpy outputRemovedSpy(&registry, SIGNAL(outputRemoved(quint32)));
    QVERIFY(outputRemovedSpy.isValid());

    delete m_output;
    QVERIFY(outputRemovedSpy.wait());
    QCOMPARE(outputRemovedSpy.first().first(), outputAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Output));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Output).isEmpty());
    QCOMPARE(outputObjectRemovedSpy.count(), 1);

    QSignalSpy outputDeviceRemovedSpy(&registry, SIGNAL(outputDeviceV1Removed(quint32)));
    QVERIFY(outputDeviceRemovedSpy.isValid());

    delete m_outputDevice;
    QVERIFY(outputDeviceRemovedSpy.wait());
    QCOMPARE(outputDeviceRemovedSpy.first().first(), outputDeviceAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::OutputDeviceV1));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::OutputDeviceV1).isEmpty());

    QSignalSpy compositorRemovedSpy(&registry, SIGNAL(compositorRemoved(quint32)));
    QVERIFY(compositorRemovedSpy.isValid());

    delete m_compositor;
    QVERIFY(compositorRemovedSpy.wait());
    QCOMPARE(compositorRemovedSpy.first().first(), compositorAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Compositor));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Compositor).isEmpty());
    QCOMPARE(compositorObjectRemovedSpy.count(), 1);

    QSignalSpy subCompositorRemovedSpy(&registry, SIGNAL(subCompositorRemoved(quint32)));
    QVERIFY(subCompositorRemovedSpy.isValid());

    delete m_subcompositor;
    QVERIFY(subCompositorRemovedSpy.wait());
    QCOMPARE(subCompositorRemovedSpy.first().first(), subCompositorAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::SubCompositor));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::SubCompositor).isEmpty());
    QCOMPARE(subcompositorObjectRemovedSpy.count(), 1);

    QSignalSpy outputManagementRemovedSpy(&registry, SIGNAL(outputManagementV1Removed(quint32)));
    QVERIFY(outputManagementRemovedSpy.isValid());

    delete m_outputManagement;
    QVERIFY(outputManagementRemovedSpy.wait());
    QCOMPARE(outputManagementRemovedSpy.first().first(), outputManagementAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::OutputManagementV1));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::OutputManagementV1).isEmpty());

    QSignalSpy serverSideDecoManagerRemovedSpy(&registry, &Registry::serverSideDecorationManagerRemoved);
    QVERIFY(serverSideDecoManagerRemovedSpy.isValid());
    QSignalSpy serverSideDecoManagerObjectRemovedSpy(serverSideDeco, &ServerSideDecorationManager::removed);
    QVERIFY(serverSideDecoManagerObjectRemovedSpy.isValid());

    delete m_serverSideDecorationManager;
    QVERIFY(serverSideDecoManagerRemovedSpy.wait());
    QCOMPARE(serverSideDecoManagerRemovedSpy.first().first(), serverSideDecorationManagerAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::ServerSideDecorationManager));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::ServerSideDecorationManager).isEmpty());
    QCOMPARE(serverSideDecoManagerObjectRemovedSpy.count(), 1);

    QSignalSpy blurRemovedSpy(&registry, &Registry::blurRemoved);
    QVERIFY(blurRemovedSpy.isValid());
    QSignalSpy blurObjectRemovedSpy(blurManager, &BlurManager::removed);
    QVERIFY(blurObjectRemovedSpy.isValid());

    delete m_blur;
    QVERIFY(blurRemovedSpy.wait());
    QCOMPARE(blurRemovedSpy.first().first(), blurRemovedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Blur));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Blur).isEmpty());
    QCOMPARE(blurObjectRemovedSpy.count(), 1);

    QSignalSpy idleInhibitManagerUnstableV1RemovedSpy(&registry, &Registry::idleInhibitManagerUnstableV1Removed);
    QVERIFY(idleInhibitManagerUnstableV1RemovedSpy.isValid());
    delete m_idleInhibit;
    QVERIFY(idleInhibitManagerUnstableV1RemovedSpy.wait());
    QCOMPARE(idleInhibitManagerUnstableV1RemovedSpy.first().first(), idleInhibitManagerUnstableV1AnnouncedSpy.first().first());
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1).isEmpty());
    QCOMPARE(idleInhibitManagerObjectRemovedSpy.count(), 1);

    // cannot test shmRemoved as there is no functionality for it

    // verify everything has been removed only once
    QCOMPARE(seatObjectRemovedSpy.count(), 1);
    QCOMPARE(shellObjectRemovedSpy.count(), 1);
    QCOMPARE(outputObjectRemovedSpy.count(), 1);
    QCOMPARE(compositorObjectRemovedSpy.count(), 1);
    QCOMPARE(subcompositorObjectRemovedSpy.count(), 1);
    QCOMPARE(serverSideDecoManagerObjectRemovedSpy.count(), 1);
    QCOMPARE(blurObjectRemovedSpy.count(), 1);
    QCOMPARE(idleInhibitManagerObjectRemovedSpy.count(), 1);

    delete seat;
    delete shell;
    delete output;
    delete compositor;
    delete subcompositor;
    delete serverSideDeco;
    delete blurManager;
    delete idleInhibitManager;
    registry.release();
}

void TestWaylandRegistry::testOutOfSyncRemoval()
{
    //This tests the following sequence of events
    //server announces global
    //client binds to that global

    //server removes the global
    //(simultaneously) the client legimitely uses the bound resource to the global
    //client then gets the server events...it should no-op, not be a protocol error

    using namespace Wrapland::Client;
    Wrapland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    connection.setSocketName(s_socketName);
    connection.establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, &connection,
        [&connection] {
            wl_display_flush(connection.display());
        }
    );

    Wrapland::Client::Registry registry;
    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    registry.setup();
    QSignalSpy blurAnnouncedSpy(&registry, &Registry::blurAnnounced);
    QSignalSpy contrastAnnouncedSpy(&registry, &Registry::blurAnnounced);

    QVERIFY(blurAnnouncedSpy.wait());
    QVERIFY(contrastAnnouncedSpy.count() || contrastAnnouncedSpy.wait());
    QCOMPARE(contrastAnnouncedSpy.count(), 1);

    BlurManager *blurManager = registry.createBlurManager(registry.interface(Registry::Interface::Blur).name, registry.interface(Registry::Interface::Blur).version, &registry);
    ContrastManager *contrastManager = registry.createContrastManager(registry.interface(Registry::Interface::Contrast).name, registry.interface(Registry::Interface::Contrast).version, &registry);

    connection.flush();
    m_display->dispatchEvents();

    std::unique_ptr<Compositor> compositor(registry.createCompositor(registry.interface(Registry::Interface::Compositor).name,
                                             registry.interface(Registry::Interface::Compositor).version));
    std::unique_ptr<Surface> surface(compositor->createSurface());
    QVERIFY(surface);

    //remove blur
    QSignalSpy blurRemovedSpy(&registry, &Registry::blurRemoved);

    delete m_blur;

    //client hasn't processed the event yet
    QVERIFY(blurRemovedSpy.count() == 0);

    //use the in the client
    auto *blur = blurManager->createBlur(surface.get(), nullptr);

    //now process events,
    QVERIFY(blurRemovedSpy.wait());
    QVERIFY(blurRemovedSpy.count() == 1);

    //remove background contrast
    QSignalSpy contrastRemovedSpy(&registry, &Registry::contrastRemoved);

    delete m_contrast;

    //client hasn't processed the event yet
    QVERIFY(contrastRemovedSpy.count() == 0);

    //use the in the client
    auto *contrast = contrastManager->createContrast(surface.get(), nullptr);

    //now process events,
    QVERIFY(contrastRemovedSpy.wait());
    QVERIFY(contrastRemovedSpy.count() == 1);

    delete blur;
    delete contrast;
    surface.reset();
    delete blurManager;
    delete contrastManager;
    compositor.reset();
    registry.release();
}

void TestWaylandRegistry::testDestroy()
{
    using namespace Wrapland::Client;
    Wrapland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    connection.setSocketName(s_socketName);
    connection.establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    Registry registry;
    QSignalSpy shellAnnouncedSpy(&registry, SIGNAL(shellAnnounced(quint32,quint32)));

    QVERIFY(!registry.isValid());
    registry.create(&connection);
    registry.setup();
    QVERIFY(registry.isValid());

    //create some arbitrary Interface
    shellAnnouncedSpy.wait();
    std::unique_ptr<Shell>
            shell(registry.createShell(registry.interface(Registry::Interface::Shell).name,
                                       registry.interface(Registry::Interface::Shell).version,
                                       &registry));

    QSignalSpy registryDiedSpy(&registry, &Registry::registryReleased);
    QVERIFY(registryDiedSpy.isValid());

    delete m_display;
    m_display = nullptr;
    QTRY_VERIFY(!connection.established());

    QTRY_VERIFY(registryDiedSpy.count() == 1);

    // Now the registry should be released.
    QVERIFY(!registry.isValid());

    // Calling release again should not fail.
    registry.release();
    shell->release();
}

void TestWaylandRegistry::testGlobalSync()
{
    using namespace Wrapland::Client;
    ConnectionThread connection;
    connection.setSocketName(s_socketName);
    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    connection.establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    Registry registry;
    QSignalSpy syncSpy(&registry, SIGNAL(interfacesAnnounced()));
    // Most simple case: don't even use the ConnectionThread,
    // just its display.
    registry.create(connection.display());
    registry.setup();
    QVERIFY(syncSpy.wait());
    QCOMPARE(syncSpy.count(), 1);
    registry.release();
}

void TestWaylandRegistry::testGlobalSyncThreaded()
{
    // More complex case, use a ConnectionThread, in a different Thread,
    // and our own EventQueue
    using namespace Wrapland::Client;
    ConnectionThread connection;
    connection.setSocketName(s_socketName);
    QThread thread;
    connection.moveToThread(&thread);
    thread.start();

    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    connection.establishConnection();

    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    EventQueue queue;
    queue.setup(&connection);

    Registry registry;
    QSignalSpy syncSpy(&registry, SIGNAL(interfacesAnnounced()));
    registry.setEventQueue(&queue);
    registry.create(&connection);
    registry.setup();

    QVERIFY(syncSpy.wait());
    QCOMPARE(syncSpy.count(), 1);
    registry.release();

    queue.release();
    connection.flush();

    thread.quit();
    thread.wait();
}

void TestWaylandRegistry::testAnnounceMultiple()
{
    using namespace Wrapland::Client;
    ConnectionThread connection;
    connection.setSocketName(s_socketName);
    QSignalSpy connectedSpy(&connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    connection.establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, &connection,
            [&connection] {
                wl_display_flush(connection.display());
            }
    );

    Registry registry;
    QSignalSpy syncSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(syncSpy.isValid());
    // Most simple case: don't even use the ConnectionThread,
    // just its display.
    registry.create(connection.display());
    registry.setup();
    QVERIFY(syncSpy.wait());
    QCOMPARE(syncSpy.count(), 1);

    // we should have one output now
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 1);

    QSignalSpy outputAnnouncedSpy(&registry, &Registry::outputAnnounced);
    QVERIFY(outputAnnouncedSpy.isValid());
    m_display->createOutput();
    QVERIFY(outputAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name, outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version, outputAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name, outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version, outputAnnouncedSpy.first().last().value<quint32>());

    auto output = m_display->createOutput();
//    output->create();
    QVERIFY(outputAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 3);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name, outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version, outputAnnouncedSpy.last().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name, outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version, outputAnnouncedSpy.last().last().value<quint32>());

    QSignalSpy outputRemovedSpy(&registry, &Registry::outputRemoved);
    QVERIFY(outputRemovedSpy.isValid());
    delete output;
    QVERIFY(outputRemovedSpy.wait());
    QCOMPARE(outputRemovedSpy.first().first().value<quint32>(), outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name, outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version, outputAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name, outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version, outputAnnouncedSpy.first().last().value<quint32>());

    registry.release();
}

void TestWaylandRegistry::testAnnounceMultipleOutputDeviceV1s()
{
    using namespace Wrapland::Client;
    ConnectionThread connection;
    connection.setSocketName(s_socketName);
    QSignalSpy connectedSpy(&connection, &ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    connection.establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, &connection,
            [&connection] {
                wl_display_flush(connection.display());
            }
    );

    Registry registry;
    QSignalSpy syncSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(syncSpy.isValid());
    // Most simple case: don't even use the ConnectionThread,
    // just its display.
    registry.create(connection.display());
    registry.setup();
    QVERIFY(syncSpy.wait());
    QCOMPARE(syncSpy.count(), 1);

    // we should have one output now
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).count(), 1);

    QSignalSpy outputDeviceAnnouncedSpy(&registry, &Registry::outputDeviceV1Announced);
    QVERIFY(outputDeviceAnnouncedSpy.isValid());
    m_display->createOutputDeviceV1()->create();
    QVERIFY(outputDeviceAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().name, outputDeviceAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().version, outputDeviceAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).name, outputDeviceAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).version, outputDeviceAnnouncedSpy.first().last().value<quint32>());

    auto outputDevice = m_display->createOutputDeviceV1();
    outputDevice->create();
    QVERIFY(outputDeviceAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).count(), 3);
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().name, outputDeviceAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().version, outputDeviceAnnouncedSpy.last().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).name, outputDeviceAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).version, outputDeviceAnnouncedSpy.last().last().value<quint32>());

    QSignalSpy outputDeviceRemovedSpy(&registry, &Registry::outputDeviceV1Removed);
    QVERIFY(outputDeviceRemovedSpy.isValid());
    delete outputDevice;
    QVERIFY(outputDeviceRemovedSpy.wait());
    QCOMPARE(outputDeviceRemovedSpy.first().first().value<quint32>(), outputDeviceAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().name, outputDeviceAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().version, outputDeviceAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).name, outputDeviceAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).version, outputDeviceAnnouncedSpy.first().last().value<quint32>());

    registry.release();
}

QTEST_GUILESS_MAIN(TestWaylandRegistry)
#include "test_wayland_registry.moc"
