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
#include <QtTest>

#include "../../server/display.h"

#include "../../server/compositor.h"
#include "../../server/data_control_v1.h"
#include "../../server/data_device_manager.h"
#include "../../server/dpms.h"
#include "../../server/output.h"
#include "../../server/pointer_constraints_v1.h"
#include "../../server/pointer_gestures_v1.h"
#include "../../server/relative_pointer_v1.h"
#include "../../server/seat.h"
#include "../../server/subcompositor.h"
#include "../../server/xdg_shell.h"

#include "../../src/client/blur.h"
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/contrast.h"
#include "../../src/client/data_control_v1.h"
#include "../../src/client/dpms.h"
#include "../../src/client/drm_lease_v1.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/idleinhibit.h"
#include "../../src/client/output.h"
#include "../../src/client/pointerconstraints.h"
#include "../../src/client/pointergestures.h"
#include "../../src/client/presentation_time.h"
#include "../../src/client/primary_selection.h"
#include "../../src/client/registry.h"
#include "../../src/client/relativepointer.h"
#include "../../src/client/seat.h"
#include "../../src/client/subcompositor.h"
#include "../../src/client/surface.h"
#include "../../src/client/xdg_shell.h"
#include "../../src/client/xdgdecoration.h"

#include "../../server/blur.h"
#include "../../server/contrast.h"
#include "../../server/drm_lease_v1.h"
#include "../../server/idle_inhibit_v1.h"
#include "../../server/input_method_v2.h"
#include "../../server/linux_dmabuf_v1.h"
#include "../../server/output_device_v1.h"
#include "../../server/output_management_v1.h"
#include "../../server/presentation_time.h"
#include "../../server/primary_selection.h"
#include "../../server/slide.h"
#include "../../server/text_input_v2.h"
#include "../../server/text_input_v3.h"
#include "../../server/xdg_decoration.h"

#include <wayland-blur-client-protocol.h>
#include <wayland-client-protocol.h>
#include <wayland-contrast-client-protocol.h>
#include <wayland-dpms-client-protocol.h>
#include <wayland-drm-lease-v1-client-protocol.h>
#include <wayland-idle-inhibit-unstable-v1-client-protocol.h>
#include <wayland-input-method-v2-client-protocol.h>
#include <wayland-linux-dmabuf-unstable-v1-client-protocol.h>
#include <wayland-pointer-constraints-unstable-v1-client-protocol.h>
#include <wayland-pointer-gestures-unstable-v1-client-protocol.h>
#include <wayland-presentation-time-client-protocol.h>
#include <wayland-primary-selection-unstable-v1-client-protocol.h>
#include <wayland-relativepointer-unstable-v1-client-protocol.h>
#include <wayland-slide-client-protocol.h>
#include <wayland-text-input-v2-client-protocol.h>
#include <wayland-text-input-v3-client-protocol.h>
#include <wayland-wlr-data-control-v1-client-protocol.h>
#include <wayland-xdg-decoration-unstable-v1-client-protocol.h>
#include <wayland-xdg-shell-client-protocol.h>

class TestWaylandRegistry : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandRegistry(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testBindCompositor();
    void testBindOutput();
    void testBindShm();
    void testBindDmaBuf();
    void testBindSeat();
    void testBindSubCompositor();
    void testBindDataDeviceManager();
    void testBindDataControlManager();
    void testBindDrmLeaseDeviceV1();
    void testBindBlurManager();
    void testBindContrastManager();
    void testBindSlideManager();
    void testBindDpmsManager();
    void testBindInputMethodManagerV2();
    void testBindXdgDecorationUnstableV1();
    void testBindTextInputManagerV2();
    void testBindTextInputManagerV3();
    void testBindXdgShell();
    void testBindRelativePointerManagerUnstableV1();
    void testBindPointerGesturesUnstableV1();
    void testBindPointerConstraintsUnstableV1();
    void testBindPresentationTime();
    void testBindPrimarySelectionDeviceManager();
    void testBindIdleIhibitManagerUnstableV1();
    void testRemoval();
    void testOutOfSyncRemoval();
    void testDestroy();
    void testGlobalSync();
    void testGlobalSyncThreaded();
    void testAnnounceMultiple();
    void testAnnounceMultipleOutputDeviceV1s();

private:
    std::unique_ptr<Wrapland::Server::Display> m_display;
    std::unique_ptr<Wrapland::Server::Compositor> m_compositor;
    std::unique_ptr<Wrapland::Server::Output> m_output;
    std::unique_ptr<Wrapland::Server::Seat> m_seat;
    std::unique_ptr<Wrapland::Server::Subcompositor> m_subcompositor;
    std::unique_ptr<Wrapland::Server::data_device_manager> m_dataDeviceManager;
    std::unique_ptr<Wrapland::Server::data_control_manager_v1> m_dataControlManager;
    std::unique_ptr<Wrapland::Server::drm_lease_device_v1> m_drmLeaseDeviceV1;
    std::unique_ptr<Wrapland::Server::input_method_manager_v2> m_inputMethodManagerV2;
    std::unique_ptr<Wrapland::Server::OutputManagementV1> m_outputManagement;
    std::unique_ptr<Wrapland::Server::XdgDecorationManager> m_xdgDecorationManager;
    std::unique_ptr<Wrapland::Server::TextInputManagerV2> m_textInputManagerV2;
    std::unique_ptr<Wrapland::Server::text_input_manager_v3> m_textInputManagerV3;
    std::unique_ptr<Wrapland::Server::XdgShell> m_serverXdgShell;
    std::unique_ptr<Wrapland::Server::RelativePointerManagerV1> m_relativePointerV1;
    std::unique_ptr<Wrapland::Server::PointerGesturesV1> m_pointerGesturesV1;
    std::unique_ptr<Wrapland::Server::PointerConstraintsV1> m_pointerConstraintsV1;
    std::unique_ptr<Wrapland::Server::PresentationManager> m_presentation;
    std::unique_ptr<Wrapland::Server::primary_selection_device_manager> m_primarySelection;
    std::unique_ptr<Wrapland::Server::ContrastManager> m_contrast;
    std::unique_ptr<Wrapland::Server::BlurManager> m_blur;
    std::unique_ptr<Wrapland::Server::IdleInhibitManagerV1> m_idleInhibit;
    std::unique_ptr<Wrapland::Server::LinuxDmabufV1> m_dmabuf;
};

static const QString s_socketName = QStringLiteral("wrapland-test-wayland-registry-0");

TestWaylandRegistry::TestWaylandRegistry(QObject* parent)
    : QObject(parent)
{
}

void TestWaylandRegistry::init()
{
    m_display.reset(new Wrapland::Server::Display());
    m_display->setSocketName(s_socketName);
    m_display->start();

    m_display->createShm();
    m_compositor.reset(m_display->createCompositor());
    m_output.reset(new Wrapland::Server::Output(m_display.get()));
    m_seat.reset(m_display->createSeat());
    m_subcompositor.reset(m_display->createSubCompositor());
    m_dataDeviceManager.reset(m_display->createDataDeviceManager());
    m_dataControlManager.reset(m_display->create_data_control_manager_v1());
    m_drmLeaseDeviceV1.reset(m_display->createDrmLeaseDeviceV1());
    m_outputManagement.reset(m_display->createOutputManagementV1());
    m_blur.reset(m_display->createBlurManager());
    m_contrast.reset(m_display->createContrastManager());
    m_display->createSlideManager(this);
    m_display->createDpmsManager(this);
    m_inputMethodManagerV2.reset(m_display->createInputMethodManagerV2());
    m_serverXdgShell.reset(m_display->createXdgShell());
    m_xdgDecorationManager.reset(m_display->createXdgDecorationManager(m_serverXdgShell.get()));
    m_textInputManagerV2.reset(m_display->createTextInputManagerV2());
    m_textInputManagerV3.reset(m_display->createTextInputManagerV3());
    m_relativePointerV1.reset(m_display->createRelativePointerManager());
    m_pointerGesturesV1.reset(m_display->createPointerGestures());
    m_pointerConstraintsV1.reset(m_display->createPointerConstraints());
    m_presentation.reset(m_display->createPresentationManager());
    m_primarySelection.reset(m_display->createPrimarySelectionDeviceManager());
    m_idleInhibit.reset(m_display->createIdleInhibitManager());
    m_dmabuf.reset(m_display->createLinuxDmabuf());

    m_output->set_enabled(true);
    m_output->done();
}

void TestWaylandRegistry::cleanup()
{
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

#define TEST_BIND(iface, signalName, bindMethod, destroyFunction)                                  \
    Wrapland::Client::ConnectionThread connection;                                                 \
    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged); \
    connection.setSocketName(s_socketName);                                                        \
    connection.establishConnection();                                                              \
    QVERIFY(connectedSpy.count() || connectedSpy.wait());                                          \
    QCOMPARE(connectedSpy.count(), 1);                                                             \
                                                                                                   \
    Wrapland::Client::Registry registry;                                                           \
    /* before registry is created, we cannot bind the interface*/                                  \
    QVERIFY(!registry.bindMethod(0, 0));                                                           \
                                                                                                   \
    QVERIFY(!registry.isValid());                                                                  \
    registry.create(&connection);                                                                  \
    QVERIFY(registry.isValid());                                                                   \
    /* created but not yet connected still results in no bind */                                   \
    QVERIFY(!registry.bindMethod(0, 0));                                                           \
    /* interface information should be empty */                                                    \
    QVERIFY(registry.interfaces(iface).isEmpty());                                                 \
    QCOMPARE(registry.interface(iface).name, 0u);                                                  \
    QCOMPARE(registry.interface(iface).version, 0u);                                               \
                                                                                                   \
    /* now let's register */                                                                       \
    QSignalSpy announced(&registry, signalName);                                                   \
    QVERIFY(announced.isValid());                                                                  \
    registry.setup();                                                                              \
    wl_display_flush(connection.display());                                                        \
    QVERIFY(announced.wait());                                                                     \
    const quint32 name = announced.first().first().value<quint32>();                               \
    const quint32 version = announced.first().last().value<quint32>();                             \
    QCOMPARE(registry.interfaces(iface).count(), 1);                                               \
    QCOMPARE(registry.interfaces(iface).first().name, name);                                       \
    QCOMPARE(registry.interfaces(iface).first().version, version);                                 \
    QCOMPARE(registry.interface(iface).name, name);                                                \
    QCOMPARE(registry.interface(iface).version, version);                                          \
                                                                                                   \
    /* registry should know about the interface now */                                             \
    QVERIFY(registry.hasInterface(iface));                                                         \
    QVERIFY(!registry.bindMethod(name + 1, version));                                              \
    auto* higherVersion = registry.bindMethod(name, version + 1);                                  \
    QVERIFY(higherVersion);                                                                        \
    destroyFunction(higherVersion);                                                                \
    auto* c = registry.bindMethod(name, version);                                                  \
    QVERIFY(c);                                                                                    \
    destroyFunction(c);

void TestWaylandRegistry::testBindCompositor()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Compositor,
              SIGNAL(compositorAnnounced(quint32, quint32)),
              bindCompositor,
              wl_compositor_destroy)
}

void TestWaylandRegistry::testBindOutput()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Output,
              SIGNAL(outputAnnounced(quint32, quint32)),
              bindOutput,
              wl_output_destroy)
}

void TestWaylandRegistry::testBindDmaBuf()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::LinuxDmabufV1,
              SIGNAL(LinuxDmabufV1Announced(quint32, quint32)),
              bindLinuxDmabufV1,
              zwp_linux_dmabuf_v1_destroy)
}

void TestWaylandRegistry::testBindSeat()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Seat,
              SIGNAL(seatAnnounced(quint32, quint32)),
              bindSeat,
              wl_seat_destroy)
}

void TestWaylandRegistry::testBindShm()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Shm,
              SIGNAL(shmAnnounced(quint32, quint32)),
              bindShm,
              wl_shm_destroy)
}

void TestWaylandRegistry::testBindSubCompositor()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::SubCompositor,
              SIGNAL(subCompositorAnnounced(quint32, quint32)),
              bindSubCompositor,
              wl_subcompositor_destroy)
}

void TestWaylandRegistry::testBindDataDeviceManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::DataDeviceManager,
              SIGNAL(dataDeviceManagerAnnounced(quint32, quint32)),
              bindDataDeviceManager,
              wl_data_device_manager_destroy)
}

void TestWaylandRegistry::testBindDataControlManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::DataControlManagerV1,
              SIGNAL(dataControlManagerV1Announced(quint32, quint32)),
              bindDataControlManagerV1,
              zwlr_data_control_manager_v1_destroy)
}

void TestWaylandRegistry::testBindDrmLeaseDeviceV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::DrmLeaseDeviceV1,
              SIGNAL(drmLeaseDeviceV1Announced(quint32, quint32)),
              bindDrmLeaseDeviceV1,
              wp_drm_lease_device_v1_destroy)
}

void TestWaylandRegistry::testBindBlurManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Blur,
              SIGNAL(blurAnnounced(quint32, quint32)),
              bindBlurManager,
              org_kde_kwin_blur_manager_destroy)
}

void TestWaylandRegistry::testBindContrastManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Contrast,
              SIGNAL(contrastAnnounced(quint32, quint32)),
              bindContrastManager,
              org_kde_kwin_contrast_manager_destroy)
}

void TestWaylandRegistry::testBindSlideManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Slide,
              SIGNAL(slideAnnounced(quint32, quint32)),
              bindSlideManager,
              org_kde_kwin_slide_manager_destroy)
}

void TestWaylandRegistry::testBindDpmsManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::Dpms,
              SIGNAL(dpmsAnnounced(quint32, quint32)),
              bindDpmsManager,
              org_kde_kwin_dpms_manager_destroy)
}

void TestWaylandRegistry::testBindInputMethodManagerV2()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::InputMethodManagerV2,
              SIGNAL(inputMethodManagerV2Announced(quint32, quint32)),
              bindInputMethodManagerV2,
              zwp_input_method_manager_v2_destroy)
}

void TestWaylandRegistry::testBindXdgDecorationUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::XdgDecorationUnstableV1,
              SIGNAL(xdgDecorationAnnounced(quint32, quint32)),
              bindXdgDecorationUnstableV1,
              zxdg_decoration_manager_v1_destroy)
}

void TestWaylandRegistry::testBindTextInputManagerV2()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::TextInputManagerV2,
              SIGNAL(textInputManagerV2Announced(quint32, quint32)),
              bindTextInputManagerV2,
              zwp_text_input_manager_v2_destroy)
}

void TestWaylandRegistry::testBindTextInputManagerV3()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::TextInputManagerV3,
              SIGNAL(textInputManagerV3Announced(quint32, quint32)),
              bindTextInputManagerV3,
              zwp_text_input_manager_v3_destroy)
}

void TestWaylandRegistry::testBindXdgShell()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::XdgShell,
              SIGNAL(xdgShellAnnounced(quint32, quint32)),
              bindXdgShell,
              xdg_wm_base_destroy)
}

void TestWaylandRegistry::testBindRelativePointerManagerUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::RelativePointerManagerUnstableV1,
              SIGNAL(relativePointerManagerUnstableV1Announced(quint32, quint32)),
              bindRelativePointerManagerUnstableV1,
              zwp_relative_pointer_manager_v1_destroy)
}

void TestWaylandRegistry::testBindPointerGesturesUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::PointerGesturesUnstableV1,
              SIGNAL(pointerGesturesUnstableV1Announced(quint32, quint32)),
              bindPointerGesturesUnstableV1,
              zwp_pointer_gestures_v1_destroy)
}

void TestWaylandRegistry::testBindPointerConstraintsUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::PointerConstraintsUnstableV1,
              SIGNAL(pointerConstraintsUnstableV1Announced(quint32, quint32)),
              bindPointerConstraintsUnstableV1,
              zwp_pointer_constraints_v1_destroy)
}

void TestWaylandRegistry::testBindPresentationTime()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::PresentationManager,
              SIGNAL(presentationManagerAnnounced(quint32, quint32)),
              bindPresentationManager,
              wp_presentation_destroy)
}

void TestWaylandRegistry::testBindPrimarySelectionDeviceManager()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::PrimarySelectionDeviceManager,
              SIGNAL(primarySelectionDeviceManagerAnnounced(quint32, quint32)),
              bindPrimarySelectionDeviceManager,
              zwp_primary_selection_device_manager_v1_destroy)
}

void TestWaylandRegistry::testBindIdleIhibitManagerUnstableV1()
{
    TEST_BIND(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1,
              SIGNAL(idleInhibitManagerUnstableV1Announced(quint32, quint32)),
              bindIdleInhibitManagerUnstableV1,
              zwp_idle_inhibit_manager_v1_destroy)
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

    connect(QCoreApplication::eventDispatcher(),
            &QAbstractEventDispatcher::aboutToBlock,
            &connection,
            [&connection] { wl_display_flush(connection.display()); });

    Wrapland::Client::Registry registry;
    QSignalSpy shmAnnouncedSpy(&registry, SIGNAL(shmAnnounced(quint32, quint32)));
    QVERIFY(shmAnnouncedSpy.isValid());
    QSignalSpy compositorAnnouncedSpy(&registry, SIGNAL(compositorAnnounced(quint32, quint32)));
    QVERIFY(compositorAnnouncedSpy.isValid());
    QSignalSpy outputAnnouncedSpy(&registry, SIGNAL(outputAnnounced(quint32, quint32)));
    QVERIFY(outputAnnouncedSpy.isValid());
    QSignalSpy outputDeviceAnnouncedSpy(&registry,
                                        SIGNAL(outputDeviceV1Announced(quint32, quint32)));
    QVERIFY(outputDeviceAnnouncedSpy.isValid());
    QSignalSpy shellAnnouncedSpy(&registry, SIGNAL(xdgShellAnnounced(quint32, quint32)));
    QVERIFY(shellAnnouncedSpy.isValid());
    QSignalSpy seatAnnouncedSpy(&registry, SIGNAL(seatAnnounced(quint32, quint32)));
    QVERIFY(seatAnnouncedSpy.isValid());
    QSignalSpy subCompositorAnnouncedSpy(&registry,
                                         SIGNAL(subCompositorAnnounced(quint32, quint32)));
    QVERIFY(subCompositorAnnouncedSpy.isValid());
    QSignalSpy outputManagementAnnouncedSpy(&registry,
                                            SIGNAL(outputManagementV1Announced(quint32, quint32)));
    QVERIFY(outputManagementAnnouncedSpy.isValid());
    QSignalSpy presentationAnnouncedSpy(&registry, &Registry::presentationManagerAnnounced);
    QVERIFY(presentationAnnouncedSpy.isValid());
    QSignalSpy xdgDecorationAnnouncedSpy(&registry,
                                         SIGNAL(xdgDecorationAnnounced(quint32, quint32)));
    QVERIFY(xdgDecorationAnnouncedSpy.isValid());
    QSignalSpy blurAnnouncedSpy(&registry, &Registry::blurAnnounced);
    QVERIFY(blurAnnouncedSpy.isValid());
    QSignalSpy idleInhibitManagerUnstableV1AnnouncedSpy(
        &registry, &Registry::idleInhibitManagerUnstableV1Announced);
    QVERIFY(idleInhibitManagerUnstableV1AnnouncedSpy.isValid());

    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    registry.setup();

    QVERIFY(shmAnnouncedSpy.wait());
    QVERIFY(!compositorAnnouncedSpy.isEmpty());
    QVERIFY(!outputAnnouncedSpy.isEmpty());
    QVERIFY(!outputDeviceAnnouncedSpy.isEmpty());
    QVERIFY(!presentationAnnouncedSpy.isEmpty());
    QVERIFY(!shellAnnouncedSpy.isEmpty());
    QVERIFY(!seatAnnouncedSpy.isEmpty());
    QVERIFY(!subCompositorAnnouncedSpy.isEmpty());
    QVERIFY(!outputManagementAnnouncedSpy.isEmpty());
    QVERIFY(!xdgDecorationAnnouncedSpy.isEmpty());
    QVERIFY(!blurAnnouncedSpy.isEmpty());
    QVERIFY(!idleInhibitManagerUnstableV1AnnouncedSpy.isEmpty());

    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Compositor));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Output));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::OutputDeviceV1));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::PresentationManager));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Seat));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::XdgShell));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Shm));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::SubCompositor));
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::FullscreenShell));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::OutputManagementV1));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::XdgDecorationUnstableV1));
    QVERIFY(registry.hasInterface(Wrapland::Client::Registry::Interface::Blur));
    QVERIFY(
        registry.hasInterface(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1));

    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Compositor).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Output).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::OutputDeviceV1).isEmpty());
    QVERIFY(
        !registry.interfaces(Wrapland::Client::Registry::Interface::PresentationManager).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Seat).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::XdgShell).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Shm).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::SubCompositor).isEmpty());
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::FullscreenShell).isEmpty());
    QVERIFY(
        !registry.interfaces(Wrapland::Client::Registry::Interface::OutputManagementV1).isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::XdgDecorationUnstableV1)
                 .isEmpty());
    QVERIFY(!registry.interfaces(Wrapland::Client::Registry::Interface::Blur).isEmpty());
    QVERIFY(
        !registry.interfaces(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1)
             .isEmpty());

    QSignalSpy seatRemovedSpy(&registry, SIGNAL(seatRemoved(quint32)));
    QVERIFY(seatRemovedSpy.isValid());

    Seat* seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name,
                                     registry.interface(Registry::Interface::Seat).version,
                                     &registry);
    auto shell = registry.createXdgShell(registry.interface(Registry::Interface::XdgShell).name,
                                         registry.interface(Registry::Interface::XdgShell).version,
                                         &registry);
    Output* output = registry.createOutput(registry.interface(Registry::Interface::Output).name,
                                           registry.interface(Registry::Interface::Output).version,
                                           &registry);
    auto presentationManager = registry.createPresentationManager(
        registry.interface(Registry::Interface::PresentationManager).name,
        registry.interface(Registry::Interface::PresentationManager).version,
        &registry);
    Compositor* compositor
        = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name,
                                    registry.interface(Registry::Interface::Compositor).version,
                                    &registry);
    SubCompositor* subcompositor = registry.createSubCompositor(
        registry.interface(Registry::Interface::SubCompositor).name,
        registry.interface(Registry::Interface::SubCompositor).version,
        &registry);
    XdgDecorationManager* xdgDecoManager = registry.createXdgDecorationManager(
        registry.interface(Registry::Interface::XdgDecorationUnstableV1).name,
        registry.interface(Registry::Interface::XdgDecorationUnstableV1).version,
        &registry);
    BlurManager* blurManager
        = registry.createBlurManager(registry.interface(Registry::Interface::Blur).name,
                                     registry.interface(Registry::Interface::Blur).version,
                                     &registry);
    auto idleInhibitManager = registry.createIdleInhibitManager(
        registry.interface(Registry::Interface::IdleInhibitManagerUnstableV1).name,
        registry.interface(Registry::Interface::IdleInhibitManagerUnstableV1).version,
        &registry);

    connection.flush();
    m_display->dispatchEvents();
    QSignalSpy seatObjectRemovedSpy(seat, &Seat::removed);
    QVERIFY(seatObjectRemovedSpy.isValid());
    QSignalSpy shellObjectRemovedSpy(shell, &XdgShell::removed);
    QVERIFY(shellObjectRemovedSpy.isValid());
    QSignalSpy outputObjectRemovedSpy(output, &Output::removed);
    QVERIFY(outputObjectRemovedSpy.isValid());
    QSignalSpy compositorObjectRemovedSpy(compositor, &Compositor::removed);
    QVERIFY(compositorObjectRemovedSpy.isValid());
    QSignalSpy subcompositorObjectRemovedSpy(subcompositor, &SubCompositor::removed);
    QVERIFY(subcompositorObjectRemovedSpy.isValid());
    QSignalSpy idleInhibitManagerObjectRemovedSpy(idleInhibitManager, &IdleInhibitManager::removed);
    QVERIFY(idleInhibitManagerObjectRemovedSpy.isValid());
    QSignalSpy xdgDecorationManagerObjectRemovedSpy(xdgDecoManager, &XdgDecorationManager::removed);
    QVERIFY(xdgDecorationManagerObjectRemovedSpy.isValid());

    m_seat.reset();
    QVERIFY(seatRemovedSpy.wait());
    QCOMPARE(seatRemovedSpy.first().first(), seatAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Seat));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Seat).isEmpty());
    QCOMPARE(seatObjectRemovedSpy.count(), 1);

    QSignalSpy shellRemovedSpy(&registry, SIGNAL(xdgShellRemoved(quint32)));
    QVERIFY(shellRemovedSpy.isValid());

    m_serverXdgShell.reset();
    QVERIFY(shellRemovedSpy.wait());
    QCOMPARE(shellRemovedSpy.first().first(), shellAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::XdgShell));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::XdgShell).isEmpty());
    QCOMPARE(shellObjectRemovedSpy.count(), 1);

    QSignalSpy outputRemovedSpy(&registry, SIGNAL(outputRemoved(quint32)));
    QVERIFY(outputRemovedSpy.isValid());

    QSignalSpy outputDeviceRemovedSpy(&registry, SIGNAL(outputDeviceV1Removed(quint32)));
    QVERIFY(outputDeviceRemovedSpy.isValid());

    m_output.reset();
    QVERIFY(outputRemovedSpy.wait());
    QCOMPARE(outputRemovedSpy.first().first(), outputAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Output));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Output).isEmpty());
    QCOMPARE(outputObjectRemovedSpy.count(), 1);

    m_output.reset();
    if (!outputDeviceRemovedSpy.size()) {
        QVERIFY(outputDeviceRemovedSpy.wait());
    }
    QCOMPARE(outputDeviceRemovedSpy.first().first(), outputDeviceAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::OutputDeviceV1));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::OutputDeviceV1).isEmpty());

    QSignalSpy compositorRemovedSpy(&registry, SIGNAL(compositorRemoved(quint32)));
    QVERIFY(compositorRemovedSpy.isValid());

    m_compositor.reset();
    QVERIFY(compositorRemovedSpy.wait());
    QCOMPARE(compositorRemovedSpy.first().first(), compositorAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Compositor));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Compositor).isEmpty());
    QCOMPARE(compositorObjectRemovedSpy.count(), 1);

    QSignalSpy subCompositorRemovedSpy(&registry, SIGNAL(subCompositorRemoved(quint32)));
    QVERIFY(subCompositorRemovedSpy.isValid());

    m_subcompositor.reset();
    QVERIFY(subCompositorRemovedSpy.wait());
    QCOMPARE(subCompositorRemovedSpy.first().first(), subCompositorAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::SubCompositor));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::SubCompositor).isEmpty());
    QCOMPARE(subcompositorObjectRemovedSpy.count(), 1);

    QSignalSpy outputManagementRemovedSpy(&registry, SIGNAL(outputManagementV1Removed(quint32)));
    QVERIFY(outputManagementRemovedSpy.isValid());

    m_outputManagement.reset();
    QVERIFY(outputManagementRemovedSpy.wait());
    QCOMPARE(outputManagementRemovedSpy.first().first(),
             outputManagementAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::OutputManagementV1));
    QVERIFY(
        registry.interfaces(Wrapland::Client::Registry::Interface::OutputManagementV1).isEmpty());

    QSignalSpy xdgDecorationRemovedSpy(&registry, SIGNAL(xdgDecorationRemoved(quint32)));
    QVERIFY(xdgDecorationRemovedSpy.isValid());

    QSignalSpy presentationRemovedSpy(&registry, &Registry::presentationManagerRemoved);
    QVERIFY(presentationRemovedSpy.isValid());
    QSignalSpy presentationObjectRemovedSpy(presentationManager, &PresentationManager::removed);
    QVERIFY(presentationObjectRemovedSpy.isValid());

    m_presentation.reset();
    QVERIFY(presentationRemovedSpy.wait());
    QCOMPARE(presentationRemovedSpy.first().first(), presentationRemovedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::PresentationManager));
    QVERIFY(
        registry.interfaces(Wrapland::Client::Registry::Interface::PresentationManager).isEmpty());
    QCOMPARE(presentationObjectRemovedSpy.count(), 1);

    m_xdgDecorationManager.reset();
    QVERIFY(xdgDecorationRemovedSpy.wait());
    QCOMPARE(xdgDecorationRemovedSpy.first().first(), xdgDecorationAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::XdgDecorationUnstableV1));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::XdgDecorationUnstableV1)
                .isEmpty());
    QCOMPARE(xdgDecorationManagerObjectRemovedSpy.count(), 1);

    QSignalSpy blurRemovedSpy(&registry, &Registry::blurRemoved);
    QVERIFY(blurRemovedSpy.isValid());
    QSignalSpy blurObjectRemovedSpy(blurManager, &BlurManager::removed);
    QVERIFY(blurObjectRemovedSpy.isValid());

    m_blur.reset();
    QVERIFY(blurRemovedSpy.wait());
    QCOMPARE(blurRemovedSpy.first().first(), blurRemovedSpy.first().first());
    QVERIFY(!registry.hasInterface(Wrapland::Client::Registry::Interface::Blur));
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::Blur).isEmpty());
    QCOMPARE(blurObjectRemovedSpy.count(), 1);

    QSignalSpy idleInhibitManagerUnstableV1RemovedSpy(
        &registry, &Registry::idleInhibitManagerUnstableV1Removed);
    QVERIFY(idleInhibitManagerUnstableV1RemovedSpy.isValid());
    m_idleInhibit.reset();
    QVERIFY(idleInhibitManagerUnstableV1RemovedSpy.wait());
    QCOMPARE(idleInhibitManagerUnstableV1RemovedSpy.first().first(),
             idleInhibitManagerUnstableV1AnnouncedSpy.first().first());
    QVERIFY(registry.interfaces(Wrapland::Client::Registry::Interface::IdleInhibitManagerUnstableV1)
                .isEmpty());
    QCOMPARE(idleInhibitManagerObjectRemovedSpy.count(), 1);

    // cannot test shmRemoved as there is no functionality for it

    // verify everything has been removed only once
    QCOMPARE(seatObjectRemovedSpy.count(), 1);
    QCOMPARE(shellObjectRemovedSpy.count(), 1);
    QCOMPARE(outputObjectRemovedSpy.count(), 1);
    QCOMPARE(presentationObjectRemovedSpy.count(), 1);
    QCOMPARE(compositorObjectRemovedSpy.count(), 1);
    QCOMPARE(subcompositorObjectRemovedSpy.count(), 1);
    QCOMPARE(xdgDecorationManagerObjectRemovedSpy.count(), 1);
    QCOMPARE(blurObjectRemovedSpy.count(), 1);
    QCOMPARE(idleInhibitManagerObjectRemovedSpy.count(), 1);

    delete seat;
    delete shell;
    delete output;
    delete presentationManager;
    delete compositor;
    delete subcompositor;
    delete xdgDecoManager;
    delete blurManager;
    delete idleInhibitManager;
    registry.release();
}

void TestWaylandRegistry::testOutOfSyncRemoval()
{
    // This tests the following sequence of events:
    // 1. Server announces global.
    // 2. Client binds to that global.
    // 3. Server removes the global.
    //    Simultaneously the client legimitely uses the bound resource to the global.
    // 4. Client then gets the server events...it should no-op, not be a protocol error.

    using namespace Wrapland::Client;
    Wrapland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    connection.setSocketName(s_socketName);
    connection.establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    connect(QCoreApplication::eventDispatcher(),
            &QAbstractEventDispatcher::aboutToBlock,
            &connection,
            [&connection] { wl_display_flush(connection.display()); });

    Wrapland::Client::Registry registry;
    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    registry.setup();
    QSignalSpy blurAnnouncedSpy(&registry, &Registry::blurAnnounced);
    QSignalSpy contrastAnnouncedSpy(&registry, &Registry::blurAnnounced);

    QVERIFY(blurAnnouncedSpy.wait());
    QVERIFY(contrastAnnouncedSpy.count() || contrastAnnouncedSpy.wait());
    QCOMPARE(contrastAnnouncedSpy.count(), 1);

    BlurManager* blurManager
        = registry.createBlurManager(registry.interface(Registry::Interface::Blur).name,
                                     registry.interface(Registry::Interface::Blur).version,
                                     &registry);

    connection.flush();
    m_display->dispatchEvents();

    std::unique_ptr<Compositor> compositor(
        registry.createCompositor(registry.interface(Registry::Interface::Compositor).name,
                                  registry.interface(Registry::Interface::Compositor).version));
    std::unique_ptr<Surface> surface(compositor->createSurface());
    QVERIFY(surface);

    // remove blur
    QSignalSpy blurRemovedSpy(&registry, &Registry::blurRemoved);

    m_blur.reset();

    // client hasn't processed the event yet
    QVERIFY(blurRemovedSpy.count() == 0);

    // use the in the client
    auto* blur = blurManager->createBlur(surface.get(), nullptr);

    // now process events,
    QVERIFY(blurRemovedSpy.wait());
    QVERIFY(blurRemovedSpy.count() == 1);

    // Remove contrast global.
    QSignalSpy contrastRemovedSpy(&registry, &Registry::contrastRemoved);

    m_contrast.reset();

    // Client hasn't processed the event yet.
    QVERIFY(contrastRemovedSpy.count() == 0);

    // Create and use the in the client.
    ContrastManager* contrastManager
        = registry.createContrastManager(registry.interface(Registry::Interface::Contrast).name,
                                         registry.interface(Registry::Interface::Contrast).version,
                                         &registry);
    auto* contrast = contrastManager->createContrast(surface.get(), nullptr);

    // Now process events.
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
    QSignalSpy shellAnnouncedSpy(&registry, &Registry::xdgShellAnnounced);

    QVERIFY(!registry.isValid());
    registry.create(&connection);
    registry.setup();
    QVERIFY(registry.isValid());

    // create some arbitrary Interface
    QVERIFY(shellAnnouncedSpy.wait());
    std::unique_ptr<XdgShell> shell(
        registry.createXdgShell(registry.interface(Registry::Interface::XdgShell).name,
                                registry.interface(Registry::Interface::XdgShell).version,
                                &registry));

    QSignalSpy registryDiedSpy(&registry, &Registry::registryReleased);
    QVERIFY(registryDiedSpy.isValid());

    m_display.reset();
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

    connect(QCoreApplication::eventDispatcher(),
            &QAbstractEventDispatcher::aboutToBlock,
            &connection,
            [&connection] { wl_display_flush(connection.display()); });

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

    std::unique_ptr<Wrapland::Server::Output> output1{
        new Wrapland::Server::Output(m_display.get())};
    output1->set_enabled(true);
    output1->done();
    QVERIFY(outputAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name,
             outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version,
             outputAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name,
             outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version,
             outputAnnouncedSpy.first().last().value<quint32>());

    std::unique_ptr<Wrapland::Server::Output> output2{
        new Wrapland::Server::Output(m_display.get())};
    output2->set_enabled(true);
    output2->done();
    QVERIFY(outputAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 3);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name,
             outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version,
             outputAnnouncedSpy.last().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name,
             outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version,
             outputAnnouncedSpy.last().last().value<quint32>());

    QSignalSpy outputRemovedSpy(&registry, &Registry::outputRemoved);
    QVERIFY(outputRemovedSpy.isValid());

    output2.reset();
    QVERIFY(outputRemovedSpy.wait());
    QCOMPARE(outputRemovedSpy.first().first().value<quint32>(),
             outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name,
             outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version,
             outputAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name,
             outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version,
             outputAnnouncedSpy.first().last().value<quint32>());

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

    connect(QCoreApplication::eventDispatcher(),
            &QAbstractEventDispatcher::aboutToBlock,
            &connection,
            [&connection] { wl_display_flush(connection.display()); });

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

    std::unique_ptr<Wrapland::Server::Output> device1{
        new Wrapland::Server::Output(m_display.get())};
    QVERIFY(outputDeviceAnnouncedSpy.wait());

    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().name,
             outputDeviceAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().version,
             outputDeviceAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).name,
             outputDeviceAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).version,
             outputDeviceAnnouncedSpy.first().last().value<quint32>());

    std::unique_ptr<Wrapland::Server::Output> device2{
        new Wrapland::Server::Output(m_display.get())};
    QVERIFY(outputDeviceAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).count(), 3);
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().name,
             outputDeviceAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().version,
             outputDeviceAnnouncedSpy.last().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).name,
             outputDeviceAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).version,
             outputDeviceAnnouncedSpy.last().last().value<quint32>());

    QSignalSpy outputDeviceRemovedSpy(&registry, &Registry::outputDeviceV1Removed);
    QVERIFY(outputDeviceRemovedSpy.isValid());

    device2.reset();
    QVERIFY(outputDeviceRemovedSpy.wait());
    QCOMPARE(outputDeviceRemovedSpy.first().first().value<quint32>(),
             outputDeviceAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().name,
             outputDeviceAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::OutputDeviceV1).last().version,
             outputDeviceAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).name,
             outputDeviceAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::OutputDeviceV1).version,
             outputDeviceAnnouncedSpy.first().last().value<quint32>());

    registry.release();
}

QTEST_GUILESS_MAIN(TestWaylandRegistry)
#include "registry.moc"
