/****************************************************************************
Copyright 2018  David Edmundson <kde@davidedmundson.co.uk>

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
#ifndef WRAPLAND_CLIENT_XDG_DECORATION_UNSTABLE_V1_H
#define WRAPLAND_CLIENT_XDG_DECORATION_UNSTABLE_V1_H

#include <QObject>

#include <Wrapland/Client/wraplandclient_export.h>

struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class XdgDecoration;
class XdgShellSurface;

/**
 * @short Wrapper for the zxdg_decoration_manager_v1 interface.
 *
 * This class provides a convenient wrapper for the zxdg_decoration_manager_v1 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the XdgDecorationManager interface:
 * @code
 * XdgDecorationManager *c = registry->createXdgDecorationManager(name, version);
 * @endcode
 *
 * This creates the XdgDecorationManager and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * XdgDecorationManager *c = new XdgDecorationManager;
 * c->setup(registry->bindXdgDecorationManager(name, version));
 * @endcode
 *
 * The XdgDecorationManager can be used as a drop-in replacement for any zxdg_decoration_manager_v1
 * pointer as it provides matching cast operators.
 *
 * If you use the QtWayland QPA you do not need to use this class.
 *
 * @see Registry
 * @since 5.54
 **/
class WRAPLANDCLIENT_EXPORT XdgDecorationManager : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new XdgDecorationManager.
     * Note: after constructing the XdgDecorationManager it is not yet valid and one needs
     * to call setup. In order to get a ready to use XdgDecorationManager prefer using
     * Registry::createXdgDecorationManager.
     **/
    explicit XdgDecorationManager(QObject *parent = nullptr);
    virtual ~XdgDecorationManager();

    /**
     * Setup this XdgDecorationManager to manage the @p xdgdecorationmanager.
     * When using Registry::createXdgDecorationManager there is no need to call this
     * method.
     **/
    void setup(zxdg_decoration_manager_v1 *xdgdecorationmanager);
    /**
     * @returns @c true if managing a zxdg_decoration_manager_v1.
     **/
    bool isValid() const;
    /**
     * Releases the zxdg_decoration_manager_v1 interface.
     * After the interface has been released the XdgDecorationManager instance is no
     * longer valid and can be setup with another zxdg_decoration_manager_v1 interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this XdgDecorationManager.
     **/
    void setEventQueue(EventQueue *queue);
    /**
     * @returns The event queue to use for creating objects with this XdgDecorationManager.
     **/
    EventQueue *eventQueue();

    XdgDecoration *getToplevelDecoration(XdgShellSurface *toplevel, QObject *parent = nullptr);

    operator zxdg_decoration_manager_v1*();
    operator zxdg_decoration_manager_v1*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the XdgDecorationManager got created by
     * Registry::createXdgDecorationManager
     **/
    void removed();

private:
    class Private;
    QScopedPointer<Private> d;
};

class WRAPLANDCLIENT_EXPORT XdgDecoration : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        ClientSide,
        ServerSide
    };

    Q_ENUM(Mode)

    virtual ~XdgDecoration();

    /**
     * Setup this XdgDecoration to manage the @p xdgdecoration.
     * When using XdgDecorationManager::createXdgDecoration there is no need to call this
     * method.
     **/
    void setup(zxdg_toplevel_decoration_v1 *xdgdecoration);
    /**
     * @returns @c true if managing a zxdg_toplevel_decoration_v1.
     **/
    bool isValid() const;
    /**
     * Releases the zxdg_toplevel_decoration_v1 interface.
     * After the interface has been released the XdgDecoration instance is no
     * longer valid and can be setup with another zxdg_toplevel_decoration_v1 interface.
     **/
    void release();

    /**
    * @brief Request that the server puts us in a given mode. The compositor will respond with a modeChange
    * The compositor may ignore this request.
    */
    void setMode(Mode mode);

    /**
     * @brief Unset our requested mode. The compositor can then configure this surface with the default mode
     */
    void unsetMode();

    /**
     * The mode configured by the server.
     */
    Mode mode() const;

    operator zxdg_toplevel_decoration_v1*();
    operator zxdg_toplevel_decoration_v1*() const;

Q_SIGNALS:
    void modeChanged(Wrapland::Client::XdgDecoration::Mode mode);

private:
    friend class XdgDecorationManager;
    explicit XdgDecoration(QObject *parent = nullptr);
    class Private;
    QScopedPointer<Private> d;
};


}
}

#endif
