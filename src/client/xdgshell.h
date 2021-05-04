/****************************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
****************************************************************************/
#ifndef WRAPLAND_CLIENT_XDG_SHELL_H
#define WRAPLAND_CLIENT_XDG_SHELL_H

#include <QObject>
#include <QRect>
#include <QSize>
// STD
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

// These are structs for xdg wm base stable
struct xdg_wm_base;
struct xdg_shell;
struct xdg_surface;
struct xdg_popup;
struct xdg_toplevel;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class Output;
class Surface;
class Seat;
class XdgShellPopup;
class XdgShellSurface;

/**
 * Builder class describing how a popup should be positioned
 * when created
 *
 * @since 0.0.539
 */
class WRAPLANDCLIENT_EXPORT XdgPositioner
{
public:
    /*
     * Flags describing how a popup should be reposition if constrained
     */
    enum class Constraint {
        /*
         * Slide the popup on the X axis until there is room
         */
        SlideX = 1 << 0,
        /*
         * Slide the popup on the Y axis until there is room
         */
        SlideY = 1 << 1,
        /*
         * Invert the anchor and gravity on the X axis
         */
        FlipX = 1 << 2,
        /*
         * Invert the anchor and gravity on the Y axis
         */
        FlipY = 1 << 3,
        /*
         * Resize the popup in the X axis
         */
        ResizeX = 1 << 4,
        /*
         * Resize the popup in the Y axis
         */
        ResizeY = 1 << 5,
    };

    Q_DECLARE_FLAGS(Constraints, Constraint)

    XdgPositioner(const QSize& initialSize = QSize(), const QRect& anchor = QRect());
    XdgPositioner(const XdgPositioner& other);
    ~XdgPositioner();

    /**
     * Which edge of the anchor should the popup be positioned around
     */
    // KF6 TODO use a better data type (enum of 8 options) rather than flags which allow invalid
    // values
    Qt::Edges anchorEdge() const;
    void setAnchorEdge(Qt::Edges edge);

    /**
     * Specifies in what direction the popup should be positioned around the anchor
     * i.e if the gravity is "bottom", then then the top of top of the popup will be at the anchor
     * edge if the gravity is top, then the bottom of the popup will be at the anchor edge
     *
     */
    // KF6 TODO use a better data type (enum of 8 options) rather than flags which allow invalid
    // values
    Qt::Edges gravity() const;
    void setGravity(Qt::Edges edge);

    /**
     * The area this popup should be positioned around
     */
    QRect anchorRect() const;
    void setAnchorRect(const QRect& anchor);

    /**
     * The size of the surface that is to be positioned.
     */
    QSize initialSize() const;
    void setInitialSize(const QSize& size);

    /**
     * Specifies how the compositor should position the popup if it does not fit in the requested
     * position
     */
    Constraints constraints() const;
    void setConstraints(Constraints constraints);

    /**
     * An additional offset that should be applied from the anchor.
     */
    QPoint anchorOffset() const;
    void setAnchorOffset(const QPoint& offset);

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @short Wrapper for the xdg_shell interface.
 *
 * This class provides a convenient wrapper for the xdg_shell interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the XdgShell interface:
 * @code
 * XdgShell *c = registry->createXdgShell(name, version);
 * @endcode
 *
 * This creates the XdgShell and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * XdgShell *c = new XdgShell;
 * c->setup(registry->bindXdgShell(name, version));
 * @endcode
 *
 * The XdgShell can be used as a drop-in replacement for any xdg_shell
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.0.525
 **/
class WRAPLANDCLIENT_EXPORT XdgShell : public QObject
{
    Q_OBJECT
public:
    virtual ~XdgShell();

    /**
     * Setup this XdgShell to manage the @p xdg_wm_base.
     * When using Registry::createXdgShell there is no need to call this
     * method.
     **/
    void setup(xdg_wm_base* xdg_wm_base);
    /**
     * @returns @c true if managing a xdg_shell.
     **/
    bool isValid() const;
    /**
     * Releases the xdg_shell interface.
     * After the interface has been released the XdgShell instance is no
     * longer valid and can be setup with another xdg_shell interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this XdgShell.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this XdgShell.
     **/
    EventQueue* eventQueue();

    /**
     * Creates a new XdgShellSurface for the given @p surface.
     **/
    XdgShellSurface* createSurface(Surface* surface, QObject* parent = nullptr);

    /**
     * Creates a new XdgShellPopup for the given @p surface on top of @p parentSurface with the
     * given @p positioner.
     * @since 0.0.539
     **/
    XdgShellPopup* createPopup(Surface* surface,
                               XdgShellSurface* parentSurface,
                               const XdgPositioner& positioner,
                               QObject* parent = nullptr);

    /**
     * Creates a new XdgShellPopup for the given @p surface on top of @p parentSurface with the
     * given @p positioner.
     * @since 0.0.539
     **/
    XdgShellPopup* createPopup(Surface* surface,
                               XdgShellPopup* parentSurface,
                               const XdgPositioner& positioner,
                               QObject* parent = nullptr);

    operator xdg_wm_base*();
    operator xdg_wm_base*() const;
    operator xdg_shell*();
    operator xdg_shell*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the XdgShell got created by
     * Registry::createXdgShell
     **/
    void removed();

protected:
    /**
     * Creates a new XdgShell.
     * Note: after constructing the XdgShell it is not yet valid and one needs
     * to call setup. In order to get a ready to use XdgShell prefer using
     * Registry::createXdgShell.
     **/
    class Private;
    explicit XdgShell(Private* p, QObject* parent = nullptr);

private:
    std::unique_ptr<Private> d;
};

/**
 *
 * @since 0.0.525
 **/
class WRAPLANDCLIENT_EXPORT XdgShellSurface : public QObject
{
    Q_OBJECT
public:
    virtual ~XdgShellSurface();
    /**
     * States the Surface can be in
     **/
    enum class State {
        /**
         * The Surface is maximized.
         **/
        Maximized = 1 << 0,
        /**
         * The Surface is fullscreen.
         **/
        Fullscreen = 1 << 1,
        /**
         * The Surface is currently being resized by the Compositor.
         **/
        Resizing = 1 << 2,
        /**
         * The Surface is considered active. Does not imply keyboard focus.
         **/
        Activated = 1 << 3,
        TiledLeft = 1 << 4,
        TiledTop = 1 << 5,
        TiledRight = 1 << 6,
        TiledBottom = 1 << 7,
    };
    Q_DECLARE_FLAGS(States, State)

    /**
     * Setup this XdgShellSurface to manage the @p toplevel on the relevant @p xdgsurface
     * When using XdgShell::createXdgShellSurface there is no need to call this
     * method.
     **/
    void setup(xdg_surface* xdgsurface, xdg_toplevel* toplevel);

    /**
     * @returns @c true if managing a xdg_surface.
     **/
    bool isValid() const;
    /**
     * Releases the xdg_surface interface.
     * After the interface has been released the XdgShellSurface instance is no
     * longer valid and can be setup with another xdg_surface interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for bound proxies.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for bound proxies.
     **/
    EventQueue* eventQueue();

    /**
     * The currently configured size.
     * @see sizeChanged
     * @see setSize
     **/
    QSize size() const;

    /**
     * Sets the size for the XdgShellSurface to @p size.
     * This is mostly an internal information. The actual size of the XdgShellSurface is
     * determined by the size of the Buffer attached to the XdgShellSurface's Surface.
     *
     * @param size The new size to be used for the XdgShellSurface
     * @see size
     * @see sizeChanged
     **/
    void setSize(const QSize& size);

    /**
     * Set this XdgShellSurface as transient for @p parent.
     **/
    void setTransientFor(XdgShellSurface* parent);

    /**
     * Sets the window title of this XdgShellSurface to @p title.
     **/
    void setTitle(const QString& title);

    /**
     * Set an application identifier for the surface.
     **/
    void setAppId(const QByteArray& appId);

    /**
     * Requests to show the window menu at @p pos in surface coordinates.
     **/
    void requestShowWindowMenu(Seat* seat, quint32 serial, const QPoint& pos);

    /**
     * Requests a move on the given @p seat after the pointer button press with the given @p serial.
     *
     * @param seat The seat on which to move the window
     * @param serial The serial of the pointer button press which should trigger the move
     **/
    void requestMove(Seat* seat, quint32 serial);

    /**
     * Requests a resize on the given @p seat after the pointer button press with the given @p
     * serial.
     *
     * @param seat The seat on which to resize the window
     * @param serial The serial of the pointer button press which should trigger the resize
     * @param edges A hint for the compositor to set e.g. an appropriate cursor image
     **/
    void requestResize(Seat* seat, quint32 serial, Qt::Edges edges);

    /**
     * When a configure event is received, if a client commits the
     * Surface in response to the configure event, then the client
     * must make an ackConfigure request sometime before the commit
     * request, passing along the @p serial of the configure event.
     * @see configureRequested
     **/
    void ackConfigure(quint32 serial);

    /**
     * Request to set this XdgShellSurface to be maximized if @p set is @c true.
     * If @p set is @c false it requests to unset the maximized state - if set.
     *
     * @param set Whether the XdgShellSurface should be maximized
     **/
    void setMaximized(bool set);

    /**
     * Request to set this XdgShellSurface as fullscreen on @p output.
     * If @p set is @c true the Surface should be set to fullscreen, otherwise restore
     * from fullscreen state.
     *
     * @param set Whether the Surface should be fullscreen or not
     * @param output Optional output as hint to the compositor where the Surface should be put
     **/
    void setFullscreen(bool set, Output* output = nullptr);

    /**
     * Request to the compositor to minimize this XdgShellSurface.
     **/
    void requestMinimize();

    /**
     * Set this surface to have a given maximum size
     * @since 0.0.539
     */
    void setMaxSize(const QSize& size);

    /**
     * Set this surface to have a given minimum size
     * @since 0.0.539
     */
    void setMinSize(const QSize& size);

    /**
     * Sets the position of the window contents within the buffer
     * @since 0.0.559
     */
    void setWindowGeometry(const QRect& windowGeometry);

    operator xdg_surface*();
    operator xdg_surface*() const;
    operator xdg_toplevel*();
    operator xdg_toplevel*() const;

Q_SIGNALS:
    /**
     * The compositor requested to close this window.
     **/
    void closeRequested();
    /**
     * The compositor sent a configure with the new @p size and the @p states.
     * Before the next commit of the surface the @p serial needs to be passed to ackConfigure.
     **/
    void configureRequested(const QSize& size,
                            Wrapland::Client::XdgShellSurface::States states,
                            quint32 serial);

    /**
     * Emitted whenever the size of the XdgShellSurface changes by e.g. receiving a configure
     * request.
     *
     * @see configureRequested
     * @see size
     * @see setSize
     **/
    void sizeChanged(const QSize&);

protected:
    class Private;
    explicit XdgShellSurface(Private* p, QObject* parent = nullptr);

private:
    std::unique_ptr<Private> d;
};

/**
 * A XdgShellPopup is a short-lived, temporary surface that can be
 * used to implement menus. It takes an explicit grab on the surface
 * that will be dismissed when the user dismisses the popup. This can
 * be done by the user clicking outside the surface, using the keyboard,
 * or even locking the screen through closing the lid or a timeout.
 * @since 0.0.525
 **/
class WRAPLANDCLIENT_EXPORT XdgShellPopup : public QObject
{
    Q_OBJECT
public:
    virtual ~XdgShellPopup();

    /**
     * Setup this XdgShellPopup to manage the @p xdgpopupv on associated @p xdgsurface
     * When using XdgShell::createXdgShellPopup there is no need to call this
     * method.
     * @since 0.0.5XDGSTABLE
     **/
    void setup(xdg_surface* xdgsurface, xdg_popup* xdgpopup);

    /**
     * @returns @c true if managing an xdg_popup.
     **/
    bool isValid() const;
    /**
     * Releases the xdg_popup interface.
     * After the interface has been released the XdgShellPopup instance is no
     * longer valid and can be setup with another xdg_popup interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for bound proxies.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for bound proxies.
     **/
    EventQueue* eventQueue();

    /**
     * Requests a grab on this popup
     * @since 0.0.539
     */
    void requestGrab(Seat* seat, quint32 serial);

    /**
     * When a configure event is received, if a client commits the
     * Surface in response to the configure event, then the client
     * must make an ackConfigure request sometime before the commit
     * request, passing along the @p serial of the configure event.
     * @see configureRequested
     * @since 0.0.556
     **/
    void ackConfigure(quint32 serial);

    /**
     * Sets the position of the window contents within the buffer
     * @since 0.0.559
     */
    void setWindowGeometry(const QRect& windowGeometry);

    operator xdg_surface*();
    operator xdg_surface*() const;
    operator xdg_popup*();
    operator xdg_popup*() const;

Q_SIGNALS:
    /**
     * This signal is emitted when a XdgShellPopup is dismissed by the
     * compositor. The user should delete this instance at this point.
     **/
    void popupDone();

    /**
     * Emitted when the server has configured the popup with the final location of @p
     * relativePosition This is emitted for V6 surfaces only
     * @since 0.0.539
     **/
    void configureRequested(const QRect& relativePosition, quint32 serial);

protected:
    class Private;
    explicit XdgShellPopup(Private* p, QObject* parent = nullptr);

private:
    std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::XdgShellSurface::States)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::XdgPositioner::Constraints)

Q_DECLARE_METATYPE(Wrapland::Client::XdgPositioner)
Q_DECLARE_METATYPE(Wrapland::Client::XdgShellSurface::State)
Q_DECLARE_METATYPE(Wrapland::Client::XdgShellSurface::States)
Q_DECLARE_METATYPE(Wrapland::Client::XdgPositioner::Constraint)
Q_DECLARE_METATYPE(Wrapland::Client::XdgPositioner::Constraints)

#endif
