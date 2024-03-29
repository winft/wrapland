/****************************************************************************
Copyright 2017  David Edmundson <kde@davidedmundson.co.uk>

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
#ifndef WRAPLAND_CLIENT_APPMENU_H
#define WRAPLAND_CLIENT_APPMENU_H

#include <QObject>
// STD
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct org_kde_kwin_appmenu_manager;
struct org_kde_kwin_appmenu;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class Surface;
class AppMenu;

/**
 * @short Wrapper for the org_kde_kwin_appmenu_manager interface.
 *
 * This class provides a convenient wrapper for the org_kde_kwin_appmenu_manager interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the AppMenuManager interface:
 * @code
 * AppMenuManager *c = registry->createAppMenuManager(name, version);
 * @endcode
 *
 * This creates the AppMenuManager and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * AppMenuManager *c = new AppMenuManager;
 * c->setup(registry->bindAppMenuManager(name, version));
 * @endcode
 *
 * The AppMenuManager can be used as a drop-in replacement for any org_kde_kwin_appmenu_manager
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.0.542
 **/
class WRAPLANDCLIENT_EXPORT AppMenuManager : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new AppMenuManager.
     * Note: after constructing the AppMenuManager it is not yet valid and one needs
     * to call setup. In order to get a ready to use AppMenuManager prefer using
     * Registry::createAppMenuManager.
     **/
    explicit AppMenuManager(QObject* parent = nullptr);
    virtual ~AppMenuManager();

    /**
     * Setup this AppMenuManager to manage the @p appmenumanager.
     * When using Registry::createAppMenuManager there is no need to call this
     * method.
     **/
    void setup(org_kde_kwin_appmenu_manager* appmenumanager);
    /**
     * @returns @c true if managing a org_kde_kwin_appmenu_manager.
     **/
    bool isValid() const;
    /**
     * Releases the org_kde_kwin_appmenu_manager interface.
     * After the interface has been released the AppMenuManager instance is no
     * longer valid and can be setup with another org_kde_kwin_appmenu_manager interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this AppMenuManager.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this AppMenuManager.
     **/
    EventQueue* eventQueue();

    AppMenu* create(Surface* surface, QObject* parent = nullptr);

    operator org_kde_kwin_appmenu_manager*();
    operator org_kde_kwin_appmenu_manager*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the AppMenuManager got created by
     * Registry::createAppMenuManager
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 *
 * @since 0.0.542
 **/
class WRAPLANDCLIENT_EXPORT AppMenu : public QObject
{
    Q_OBJECT
public:
    virtual ~AppMenu();

    /**
     * Setup this Appmenu to manage the @p appmenu.
     * When using AppMenuManager::createAppmenu there is no need to call this
     * method.
     **/
    void setup(org_kde_kwin_appmenu* appmenu);
    /**
     * @returns @c true if managing a org_kde_kwin_appmenu.
     **/
    bool isValid() const;
    /**
     * Releases the org_kde_kwin_appmenu interface.
     * After the interface has been released the Appmenu instance is no
     * longer valid and can be setup with another org_kde_kwin_appmenu interface.
     **/
    void release();

    /**
     * Sets the appmenu address. The DBus object should be registered before making this call
     * Strings should be valid DBus formatted names, in latin1.
     */
    void setAddress(QString const& serviceName, QString const& objectPath);

    operator org_kde_kwin_appmenu*();
    operator org_kde_kwin_appmenu*() const;

private:
    friend class AppMenuManager;
    explicit AppMenu(QObject* parent = nullptr);
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
