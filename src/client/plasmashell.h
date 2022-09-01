/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_PLASMASHELL_H
#define WAYLAND_PLASMASHELL_H

#include <QObject>
#include <QSize>
// STD
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct wl_surface;
struct org_kde_plasma_shell;
struct org_kde_plasma_surface;

namespace Wrapland
{
namespace Client
{
class EventQueue;
class Surface;
class PlasmaShellSurface;

/**
 * @short Wrapper for the org_kde_plasma_shell interface.
 *
 * This class provides a convenient wrapper for the org_kde_plasma_shell interface.
 * It's main purpose is to create a PlasmaShellSurface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the Shell interface:
 * @code
 * PlasmaShell *s = registry->createPlasmaShell(name, version);
 * @endcode
 *
 * This creates the PlasmaShell and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * PlasmaShell *s = new PlasmaShell;
 * s->setup(registry->bindPlasmaShell(name, version));
 * @endcode
 *
 * The PlasmaShell can be used as a drop-in replacement for any org_kde_plasma_shell
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @see PlasmaShellSurface
 **/
class WRAPLANDCLIENT_EXPORT PlasmaShell : public QObject
{
    Q_OBJECT
public:
    explicit PlasmaShell(QObject* parent = nullptr);
    virtual ~PlasmaShell();

    /**
     * @returns @c true if managing a org_kde_plasma_shell.
     **/
    bool isValid() const;
    /**
     * Releases the org_kde_plasma_shell interface.
     * After the interface has been released the PlasmaShell instance is no
     * longer valid and can be setup with another org_kde_plasma_shell interface.
     *
     * Right before the interface is released the signal interfaceAboutToBeReleased is emitted.
     * @see interfaceAboutToBeReleased
     **/
    void release();

    /**
     * Setup this Shell to manage the @p shell.
     * When using Registry::createShell there is no need to call this
     * method.
     **/
    void setup(org_kde_plasma_shell* shell);

    /**
     * Sets the @p queue to use for creating a Surface.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a Surface.
     **/
    EventQueue* eventQueue();

    /**
     * Creates a PlasmaShellSurface for the given @p surface and sets it up.
     *
     * If a PlasmaShellSurface for the given @p surface has already been created
     * a pointer to the existing one is returned instead of creating a new surface.
     *
     * @param surface The native surface to create the PlasmaShellSurface for
     * @param parent The parent to use for the PlasmaShellSurface
     * @returns created PlasmaShellSurface
     **/
    PlasmaShellSurface* createSurface(wl_surface* surface, QObject* parent = nullptr);
    /**
     * Creates a PlasmaShellSurface for the given @p surface and sets it up.
     *
     * If a PlasmaShellSurface for the given @p surface has already been created
     * a pointer to the existing one is returned instead of creating a new surface.
     *
     * @param surface The Surface to create the PlasmaShellSurface for
     * @param parent The parent to use for the PlasmaShellSurface
     * @returns created PlasmaShellSurface
     **/
    PlasmaShellSurface* createSurface(Surface* surface, QObject* parent = nullptr);

    operator org_kde_plasma_shell*();
    operator org_kde_plasma_shell*() const;

Q_SIGNALS:
    /**
     * This signal is emitted right before the interface is released.
     **/
    void interfaceAboutToBeReleased();

    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the Compositor got created by
     * Registry::createPlasmaShell
     *
     * @since 5.5
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @short Wrapper for the org_kde_plasma_surface interface.
 *
 * This class is a convenient wrapper for the org_kde_plasma_surface interface.
 *
 * To create an instance use PlasmaShell::createSurface.
 *
 * A PlasmaShellSurface is a privileged Surface which can add further hints to the
 * Wayland server about it's position and the usage role. The Wayland server is allowed
 * to ignore all requests.
 *
 * Even if a PlasmaShellSurface is created for a Surface a normal ShellSurface (or similar)
 * needs to be created to have the Surface mapped as a window by the Wayland server.
 *
 * @see PlasmaShell
 * @see Surface
 **/
class WRAPLANDCLIENT_EXPORT PlasmaShellSurface : public QObject
{
    Q_OBJECT
public:
    explicit PlasmaShellSurface(QObject* parent);
    virtual ~PlasmaShellSurface();

    /**
     * Releases the org_kde_plasma_surface interface.
     * After the interface has been released the PlasmaShellSurface instance is no
     * longer valid and can be setup with another org_kde_plasma_surface interface.
     *
     * This method is automatically invoked when the PlasmaShell which created this
     * PlasmaShellSurface gets released.
     **/
    void release();

    /**
     * Setup this PlasmaShellSurface to manage the @p surface.
     * There is normally no need to call this method as it's invoked by
     * PlasmaShell::createSurface.
     **/
    void setup(org_kde_plasma_surface* surface);

    /**
     * @returns the PlasmaShellSurface * associated with surface,
     * if any, nullptr if not found.
     * @since 5.6
     */
    static PlasmaShellSurface* get(Surface* surf);

    /**
     * @returns @c true if managing a org_kde_plasma_surface.
     **/
    bool isValid() const;
    operator org_kde_plasma_surface*();
    operator org_kde_plasma_surface*() const;

    /**
     * Describes possible roles this PlasmaShellSurface can have.
     * The role can be used by the Wayland server to e.g. change the stacking order accordingly.
     **/
    enum class Role {
        Normal,  ///< A normal Surface
        Desktop, ///< The Surface represents a desktop, normally stacked below all other surfaces
        Panel,   ///< The Surface represents a panel (dock), normally stacked above normal surfaces
        OnScreenDisplay, ///< The Surface represents an on screen display, like a volume changed
                         ///< notification
        Notification,    ///< The Surface represents a notification @since 0.0.524
        ToolTip,         ///< The Surface represents a tooltip @since 0.0.524
        CriticalNotification,
        AppletPopup,
    };
    /**
     * Changes the requested Role to @p role.
     * @see role
     **/
    void setRole(Role role);
    /**
     * @returns The requested Role, default value is @c Role::Normal.
     * @see setRole
     **/
    Role role() const;
    /**
     * Requests to position this PlasmaShellSurface at @p point in global coordinates.
     **/
    void setPosition(QPoint const& point);

    /**
     * Describes how a PlasmaShellSurface with role @c Role::Panel should behave.
     * @see Role
     **/
    enum class PanelBehavior {
        AlwaysVisible,
        AutoHide,
        WindowsCanCover,
        WindowsGoBelow,
    };
    /**
     * Sets the PanelBehavior for a PlasmaShellSurface with Role @c Role::Panel
     * @see setRole
     **/
    void setPanelBehavior(PanelBehavior behavior);

    /**
     * Setting this bit to the window, will make it say it prefers
     * to not be listed in the taskbar. Taskbar implementations
     * may or may not follow this hint.
     * @since 5.5
     */
    void setSkipTaskbar(bool skip);

    /**
     * Setting this bit on a window will indicate it does not prefer
     * to be included in a window switcher.
     * @since 0.0.547
     */
    void setSkipSwitcher(bool skip);

    /**
     * Requests to hide a surface with Role Panel and PanelBahvior AutoHide.
     *
     * Once the compositor has hidden the panel the signal {@link autoHidePanelHidden} gets
     * emitted. Once it is shown again the signal {@link autoHidePanelShown} gets emitted.
     *
     * To show the surface again from client side use {@link requestShowAutoHidingPanel}.
     *
     * @see autoHidePanelHidden
     * @see autoHidePanelShown
     * @see requestShowAutoHidingPanel
     * @since 0.0.528
     **/
    void requestHideAutoHidingPanel();

    /**
     * Requests to show a surface with Role Panel and PanelBahvior AutoHide.
     *
     * This request allows the client to show a surface which it previously
     * requested to be hidden with {@link requestHideAutoHidingPanel}.
     *
     * @see autoHidePanelHidden
     * @see autoHidePanelShown
     * @see requestHideAutoHidingPanel
     * @since 0.0.528
     **/
    void requestShowAutoHidingPanel();

    /**
     * Set whether a PlasmaShellSurface should get focus or not.
     *
     * By default some roles do not take focus. With this request the compositor
     * can be instructed to also pass focus.
     *
     * @param takesFocus Set to @c true if the surface should gain focus.
     * @since 0.0.528
     **/
    // KF6 TODO rename to make it generic
    void setPanelTakesFocus(bool takesFocus);
    void request_open_under_cursor();

Q_SIGNALS:
    /**
     * Emitted when the compositor hided an auto hiding panel.
     * @see requestHideAutoHidingPanel
     * @see autoHidePanelShown
     * @see requestShowAutoHidingPanel
     * @since 0.0.528
     **/
    void autoHidePanelHidden();

    /**
     * Emitted when the compositor showed an auto hiding panel.
     * @see requestHideAutoHidingPanel
     * @see autoHidePanelHidden
     * @see requestShowAutoHidingPanel
     * @since 0.0.528
     **/
    void autoHidePanelShown();

private:
    friend class PlasmaShell;
    class Private;
    std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Client::PlasmaShellSurface::Role)
Q_DECLARE_METATYPE(Wrapland::Client::PlasmaShellSurface::PanelBehavior)

#endif
