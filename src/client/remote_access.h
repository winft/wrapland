/****************************************************************************
Copyright 2016  Oleg Chernovskiy <kanedias@xaker.ru>

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
#ifndef WRAPLAND_CLIENT_REMOTE_ACCESS_H
#define WRAPLAND_CLIENT_REMOTE_ACCESS_H

#include <QObject>
//STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct org_kde_kwin_remote_access_manager;
struct org_kde_kwin_remote_buffer;
struct wl_output;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class RemoteBuffer;

/**
 * @short Wrapper for the org_kde_kwin_remote_access_manager interface.
 *
 * This class provides a convenient wrapper for the org_kde_kwin_remote_access_manager interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the RemoteAccessManager interface:
 * @code
 * RemoteAccessManager *c = registry->createRemoteAccessManager(name, version);
 * @endcode
 *
 * This creates the RemoteAccessManager and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * RemoteAccessManager *c = new RemoteAccessManager;
 * c->setup(registry->bindRemoteAccessManager(name, version));
 * @endcode
 *
 * The RemoteAccessManager can be used as a drop-in replacement for any org_kde_kwin_remote_access_manager
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT RemoteAccessManager : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new RemoteAccessManager.
     * Note: after constructing the RemoteAccessManager it is not yet valid and one needs
     * to call setup. In order to get a ready to use RemoteAccessManager prefer using
     * Registry::createRemoteAccessManager.
     **/
    explicit RemoteAccessManager(QObject *parent = nullptr);
    virtual ~RemoteAccessManager();

    /**
     * Setup this RemoteAccessManager to manage the @p remoteaccessmanager.
     * When using Registry::createRemoteAccessManager there is no need to call this
     * method.
     **/
    void setup(org_kde_kwin_remote_access_manager *remoteaccessmanager);
    /**
     * @returns @c true if managing a org_kde_kwin_remote_access_manager.
     **/
    bool isValid() const;
    /**
     * Releases the org_kde_kwin_remote_access_manager interface.
     * After the interface has been released the RemoteAccessManager instance is no
     * longer valid and can be setup with another org_kde_kwin_remote_access_manager interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this RemoteAccessManager.
     **/
    void setEventQueue(EventQueue *queue);
    /**
     * @returns The event queue to use for creating objects with this RemoteAccessManager.
     **/
    EventQueue *eventQueue();

    operator org_kde_kwin_remote_access_manager*();
    operator org_kde_kwin_remote_access_manager*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the RemoteAccessManager got created by
     * Registry::createRemoteAccessManager
     **/
    void removed();

    /**
     * Buffer from server is ready to be delivered to this client
     * @param buffer_id internal buffer id to be created
     **/
    void bufferReady(void* output, RemoteBuffer *rbuf);

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @short Wrapper for org_kde_kwin_remote_buffer interface.
 * The instances of this class are created by parent RemoteAccessManager.
 * Deletion (by noLongerNeeded call) is in responsibility of underlying system.
 */
class WRAPLANDCLIENT_EXPORT RemoteBuffer : public QObject
{
    Q_OBJECT
public:
    virtual ~RemoteBuffer();
    /**
     * Setup this RemoteBuffer to manage the @p remotebuffer.
     **/
    void setup(org_kde_kwin_remote_buffer *remotebuffer);
    /**
     * @returns @c true if managing a org_kde_kwin_remote_buffer.
     **/
    bool isValid() const;
    /**
     * Releases the org_kde_kwin_remote_buffer interface.
     * After the interface has been released the RemoteBuffer instance is no
     * longer valid and can be setup with another org_kde_kwin_remote_buffer interface.
     **/
    void release();

    operator org_kde_kwin_remote_buffer*();
    operator org_kde_kwin_remote_buffer*() const;
    
    qint32 fd() const;
    quint32 width() const;
    quint32 height() const;
    quint32 stride() const;
    quint32 format() const;


Q_SIGNALS:
    void parametersObtained();

private:

    friend class RemoteAccessManager;
    explicit RemoteBuffer(QObject *parent = nullptr);
    class Private;
    std::unique_ptr<Private> d;
};


}
}

#endif
