/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>
#include <QRect>
#include <QSize>

#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct xdg_surface;
struct xdg_toplevel;

namespace Wrapland::Client
{

class EventQueue;
class Output;
class Surface;
class Seat;

enum class xdg_shell_state {
    maximized = 1 << 0,
    fullscreen = 1 << 1,
    resizing = 1 << 2,
    activated = 1 << 3,
    tiled_left = 1 << 4,
    tiled_right = 1 << 5,
    tiled_top = 1 << 6,
    tiled_bottom = 1 << 7,
};
Q_DECLARE_FLAGS(xdg_shell_states, xdg_shell_state)

class WRAPLANDCLIENT_EXPORT XdgShellToplevel : public QObject
{
    Q_OBJECT
public:
    ~XdgShellToplevel() override;

    /**
     * Setup this XdgShellToplevel to manage the @p toplevel on the relevant @p xdgsurface
     * When using XdgShell::createXdgShellToplevel there is no need to call this
     * method.
     **/
    void setup(xdg_surface* xdgsurface, xdg_toplevel* toplevel);

    /**
     * @returns @c true if managing a xdg_surface.
     **/
    bool isValid() const;
    /**
     * Releases the xdg_surface interface.
     * After the interface has been released the XdgShellToplevel instance is no
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
     * Sets the size for the XdgShellToplevel to @p size.
     * This is mostly an internal information. The actual size of the XdgShellToplevel is
     * determined by the size of the Buffer attached to the XdgShellToplevel's Surface.
     *
     * @param size The new size to be used for the XdgShellToplevel
     * @see size
     * @see sizeChanged
     **/
    void setSize(QSize const& size);

    /**
     * Set this XdgShellToplevel as transient for @p parent.
     **/
    void setTransientFor(XdgShellToplevel* parent);

    /**
     * Sets the window title of this XdgShellToplevel to @p title.
     **/
    void setTitle(QString const& title);

    /**
     * Set an application identifier for the surface.
     **/
    void setAppId(QByteArray const& appId);

    /**
     * Requests to show the window menu at @p pos in surface coordinates.
     **/
    void requestShowWindowMenu(Seat* seat, quint32 serial, QPoint const& pos);

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
     * Request to set this XdgShellToplevel to be maximized if @p set is @c true.
     * If @p set is @c false it requests to unset the maximized state - if set.
     *
     * @param set Whether the XdgShellToplevel should be maximized
     **/
    void setMaximized(bool set);

    /**
     * Request to set this XdgShellToplevel as fullscreen on @p output.
     * If @p set is @c true the Surface should be set to fullscreen, otherwise restore
     * from fullscreen state.
     *
     * @param set Whether the Surface should be fullscreen or not
     * @param output Optional output as hint to the compositor where the Surface should be put
     **/
    void setFullscreen(bool set, Output* output = nullptr);

    /**
     * Request to the compositor to minimize this XdgShellToplevel.
     **/
    void requestMinimize();

    /**
     * Set this surface to have a given maximum size
     * @since 0.0.539
     */
    void setMaxSize(QSize const& size);

    /**
     * Set this surface to have a given minimum size
     * @since 0.0.539
     */
    void setMinSize(QSize const& size);

    /**
     * Sets the position of the window contents within the buffer
     * @since 0.0.559
     */
    void setWindowGeometry(QRect const& windowGeometry);

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
    void configureRequested(QSize const& size,
                            Wrapland::Client::xdg_shell_states states,
                            quint32 serial);

    /**
     * Emitted whenever the size of the XdgShellToplevel changes by e.g. receiving a configure
     * request.
     *
     * @see configureRequested
     * @see size
     * @see setSize
     **/
    void sizeChanged(QSize const&);

private:
    explicit XdgShellToplevel(QObject* parent = nullptr);
    friend class XdgShell;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::xdg_shell_states)

Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_state)
Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_states)
