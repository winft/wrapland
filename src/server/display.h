/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
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
#ifndef WAYLAND_SERVER_DISPLAY_H
#define WAYLAND_SERVER_DISPLAY_H

#include <QList>
#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include "clientconnection.h"

//STD
#include <memory>

struct wl_client;
struct wl_display;
struct wl_event_loop;

namespace Wrapland
{
/**
 * @short Wrapland Server.
 *
 * This namespace groups all classes related to the Server module.
 *
 * The main entry point into the Wrapland::Server API is the Display class.
 * It allows to create a Wayland server and create various global objects on it.
 *
 * Wrapland::Server is an API to easily create a head-less Wayland server with a
 * Qt style API.
 *
 * @see Display
 **/
namespace Server
{
class D_isplay;

//class CompositorInterface;
//class DataDeviceManagerInterface;
class IdleInterface;
//enum class IdleInhibitManagerInterfaceVersion;
class RemoteAccessManagerInterface;
class IdleInhibitManagerInterface;
class FakeInputInterface;
class PlasmaShellInterface;
//class PlasmaWindowManagementInterface;
class SeatInterface;
//class ShadowManagerInterface;
//class ContrastManagerInterface;
//class BlurManagerInterface;
class OutputInterface;
class OutputConfigurationV1Interface;
class OutputDeviceV1Interface;
class OutputManagementV1Interface;
//class SlideManagerInterface;
//class ShellInterface;
//class SubCompositorInterface;
//enum class TextInputInterfaceVersion;
//class TextInputManagerInterface;
//class XdgShellV5Interface;
//enum class XdgShellInterfaceVersion;
//class XdgShellInterface;
//enum class RelativePointerInterfaceVersion;
//class RelativePointerManagerInterface;
//enum class PointerGesturesInterfaceVersion;
//class PointerGesturesInterface;
//enum class PointerConstraintsInterfaceVersion;
//class PointerConstraintsInterface;
//class XdgForeignInterface;
//class AppMenuManagerInterface;
class ServerSideDecorationPaletteManagerInterface;
//class PlasmaVirtualDesktopManagementInterface;
//class ViewporterInterface;
class XdgOutputManagerInterface;
//class XdgDecorationManagerInterface;
class EglStreamControllerInterface;
//class KeyState;
//class LinuxDmabufUnstableV1Interface;

/**
 * @brief Class holding the Wayland server display loop.
 *
 * @todo Improve documentation
 **/
class WRAPLANDSERVER_EXPORT Display : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString socketName READ socketName WRITE setSocketName NOTIFY socketNameChanged)
    Q_PROPERTY(bool automaticSocketNaming READ automaticSocketNaming WRITE setAutomaticSocketNaming NOTIFY automaticSocketNamingChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
public:
    explicit Display(QObject *parent = nullptr, bool newInvoked = false);
    virtual ~Display();

    /**
     * Sets the basename of the socket to @p name. If @p name is empty, it will use
     * wl_display_add_socket_auto to get a free socket with a filename "wayland-%d".
     **/
    void setSocketName(const QString &name);
    QString socketName() const;

    /**
     * If automaticSocketNaming is true, the manually set socketName is ignored
     * and it will use wl_display_add_socket_auto on start to get a free socket with
     * a filename "wayland-%d" instead. The effective socket is written into socketName.
     * @since 0.0.555
     **/
    void setAutomaticSocketNaming(bool automaticSocketNaming);
    bool automaticSocketNaming() const;

    quint32 serial();
    quint32 nextSerial();

    /**
     * How to setup the server connection.
     * @li ConnectToSocket: the server will open the socket identified by the socket name
     * @li ConnectClientsOnly: only connections through createClient are possible
     **/
    enum class StartMode {
        ConnectToSocket,
        ConnectClientsOnly
    };
    void start(StartMode mode = StartMode::ConnectToSocket);
    void terminate();
    /**
     * Starts the event loop for the server socket.
     * This method should only be used if start() is used before creating the
     * QCoreApplication. In that case start() cannot fully setup the event processing
     * and the loop needs to be started after the QCoreApplication got created.
     * @see start
     * @see dispatchEvents
     **/
    void startLoop();
    /**
     * Dispatches pending events in a blocking way. May only be used if the Display is
     * created and started before the QCoreApplication is created. Once the QCoreApplication
     * is created and the event loop is started this method delegates to the normal dispatch
     * handling.
     * @see startLoop
     **/
    void dispatchEvents(int msecTimeout = -1);

    /**
     * Create a client for the given file descriptor.
     *
     * The client is created as if it connected through the normal server
     * socket. This method can be used to create a connection bypassing the
     * normal socket connection. It's recommended to use together with
     * socketpair and pass the other side of the socket to the client.
     *
     * @param fd The file descriptor for the socket to the client
     * @returns The new ClientConnection or @c null on failure.
     **/
    ClientConnection *createClient(int fd);

    operator wl_display*();
    operator wl_display*() const;
    bool isRunning() const;

    OutputInterface *createOutput(QObject *parent = nullptr);
    void removeOutput(OutputInterface *output);
    QList<OutputInterface*> outputs() const;

    OutputDeviceV1Interface *createOutputDeviceV1(QObject *parent = nullptr);
    void removeOutputDevice(OutputDeviceV1Interface *outputDevice);
    QList<OutputDeviceV1Interface*> outputDevices() const;

//    CompositorInterface *createCompositor(QObject *parent = nullptr);
    void createShm();
//    ShellInterface *createShell(QObject *parent = nullptr);
    SeatInterface *createSeat(QObject *parent = nullptr);
    /**
     * @returns All SeatInterface currently managed on the Display.
     * @since 5.6
     **/
    QVector<SeatInterface*> seats() const;
//    SubCompositorInterface *createSubCompositor(QObject *parent = nullptr);
//    DataDeviceManagerInterface *createDataDeviceManager(QObject *parent = nullptr);
    OutputManagementV1Interface *createOutputManagementV1(QObject *parent = nullptr);
    PlasmaShellInterface *createPlasmaShell(QObject *parent = nullptr);
    //PlasmaWindowManagement *createPlasmaWindowManagement(QObject *parent = nullptr);
    IdleInterface *createIdle(QObject *parent = nullptr);
    RemoteAccessManagerInterface *createRemoteAccessManager(QObject *parent = nullptr);
    FakeInputInterface *createFakeInput(QObject *parent = nullptr);
//    ShadowManagerInterface *createShadowManager(QObject *parent = nullptr);
//    SlideManagerInterface *createSlideManager(QObject *parent = nullptr);
//    ContrastManagerInterface *createContrastManager(QObject *parent = nullptr);
//    BlurManagerInterface *createBlurManager(QObject *parent = nullptr);

    /** @since 0.0.560 */
    //KeyState *createKeyState(QObject *parent = nullptr);

    /**
     * Create the text input manager in interface @p version.
     * @returns The created manager object
     * @since 0.0.523
     **/
    //TextInputManagerInterface *createTextInputManager(const TextInputInterfaceVersion &version, QObject *parent = nullptr);

//    /**
//     * Creates the XdgShell in interface @p version.
//     *
//     * @since 0.0.525
//     **/
//    XdgShellInterface *createXdgShell(const XdgShellInterfaceVersion &version, QObject *parent = nullptr);

//    /**
//     * Creates the RelativePointerManagerInterface in interface @p version
//     *
//     * @returns The created manager object
//     * @since 0.0.528
//     **/
//    RelativePointerManagerInterface *createRelativePointerManager(const RelativePointerInterfaceVersion &version, QObject *parent = nullptr);

//    /**
//     * Creates the PointerGesturesInterface in interface @p version
//     *
//     * @returns The created manager object
//     * @since 0.0.529
//     **/
//    PointerGesturesInterface *createPointerGestures(const PointerGesturesInterfaceVersion &version, QObject *parent = nullptr);

//    /**
//     * Creates the PointerConstraintsInterface in interface @p version
//     *
//     * @returns The created manager object
//     * @since 0.0.529
//     **/
//    PointerConstraintsInterface *createPointerConstraints(const PointerConstraintsInterfaceVersion &version, QObject *parent = nullptr);

//    /**
//     * Creates the XdgForeignInterface in interface @p version
//     *
//     * @returns The created manager object
//     * @since 0.0.540
//     **/
//    XdgForeignInterface *createXdgForeignInterface(QObject *parent = nullptr);

    /**
     * Creates the IdleInhibitManager in interface @p version.
     *
     * @returns The created manager object
     * @since 0.0.541
     **/
    //IdleInhibitManager *createIdleInhibitManager(const IdleInhibitManagerInterfaceVersion &version, QObject *parent = nullptr);

    /**
     * Creates the AppMenuManagerInterface in interface @p version.
     *
     * @returns The created manager object
     * @since 0.0.542
     **/
   // AppMenuManagerInterface *createAppMenuManagerInterface(QObject *parent = nullptr);

    /**
     * Creates the ServerSideDecorationPaletteManagerInterface in interface @p version.
     *
     * @returns The created manager object
     * @since 0.0.542
     **/
    ServerSideDecorationPaletteManagerInterface *createServerSideDecorationPaletteManager(QObject *parent = nullptr);

//    /**
//     * Creates the LinuxDmabufUnstableV1Interface in interface @p version.
//     *
//     * @returns A pointer to the created interface
//     **/
//    LinuxDmabufUnstableV1Interface *createLinuxDmabufInterface(QObject *parent = nullptr);

    /**
     * Creates the ViewporterInterface
     *
     * @return the created viewporter
     * @since 0.518.0
     */
    //ViewporterInterface *createViewporterInterface(QObject *parent = nullptr);

    /**
     * Creates the XdgOutputManagerInterface
     *
     * @return the created manager
     * @since 0.0.547
     */
    XdgOutputManagerInterface *createXdgOutputManager(QObject *parent = nullptr);


    /**
     * Creates the PlasmaVirtualDesktopManagementInterface in interface @p version.
     *
     * @returns The created manager object
     * @since 0.0.552
     **/
    //PlasmaVirtualDesktopManagementInterface *createPlasmaVirtualDesktopManagement(QObject *parent = nullptr);

//    /**
//     * Creates the XdgDecorationManagerInterface
//     * @arg shellInterface A created XdgShellInterface based on XDG_WM_BASE
//     *
//     * @return the created manager
//     * @since 0.0.554
//     */
//    XdgDecorationManagerInterface *createXdgDecorationManager(XdgShellInterface *shellInterface, QObject *parent = nullptr);

    /**
     * Creates the EglStreamControllerInterface
     *
     * @return the created EGL Stream controller
     * @since 0.0.558
     */
    EglStreamControllerInterface *createEglStreamControllerInterface(QObject *parent = nullptr);

    /**
     * Gets the ClientConnection for the given @p client.
     * If there is no ClientConnection yet for the given @p client, it will be created.
     * @param client The native client for which the ClientConnection is retrieved
     * @return The ClientConnection for the given native client
     **/
    ClientConnection *getConnection(wl_client *client);
    QVector<ClientConnection*> connections() const;

    /**
     * Set the EGL @p display for this Wayland display.
     * The EGLDisplay can only be set once and must be alive as long as the Wayland display
     * is alive. The user should have set up the binding between the EGLDisplay and the
     * Wayland display prior to calling this method.
     *
     * @see eglDisplay
     * @since 5.3
     **/
    void setEglDisplay(void *display);
    /**
     * @returns the EGLDisplay used for this Wayland display or EGL_NO_DISPLAY if not set.
     * @see setEglDisplay
     * @since 5.3
     **/
    void *eglDisplay() const;

    Server::D_isplay* newDisplay;

Q_SIGNALS:
    void socketNameChanged(const QString&);
    void automaticSocketNamingChanged(bool);
    void runningChanged(bool);
    void aboutToTerminate();
    void clientConnected(Wrapland::Server::ClientConnection*);
    void clientDisconnected(Wrapland::Server::ClientConnection*);

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
