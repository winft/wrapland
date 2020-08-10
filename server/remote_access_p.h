/****************************************************************************
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
****************************************************************************/
#pragma once

#include "remote_access.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-remote-access-server-protocol.h>

namespace Wrapland::Server
{

/**
 * Helper struct for manual reference counting. Automatic counting via a shared pointer is not
 * possible here as we hold strong reference in sentBuffers.
 */
struct BufferHolder {
    RemoteBufferHandle* buf;
    int counter = 0;
};

constexpr uint32_t RemoteAccessManagerVersion = 1;
using RemoteAccessManagerGlobal = Wayland::Global<RemoteAccessManager, RemoteAccessManagerVersion>;
using RemoteAccessManagerBind = Wayland::Resource<RemoteAccessManager, RemoteAccessManagerGlobal>;

class RemoteAccessManager::Private : public RemoteAccessManagerGlobal
{
public:
    Private(Display* display, RemoteAccessManager* q);

    void sendBufferReady(WlOutput* output, RemoteBufferHandle* buf);

protected:
    void prepareUnbind(RemoteAccessManagerBind* bind) override;

private:
    static void getBufferCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  uint32_t id,
                                  int32_t internalBufId);

    int unref(BufferHolder& buffer);

    static struct org_kde_kwin_remote_access_manager_interface const s_interface;

    /**
     * Buffers that were sent but still not acked by server
     * Keys are fd numbers as they are unique
     */
    QHash<uint32_t, BufferHolder> sentBuffers;
};

class RemoteBufferHandle::Private
{
public:
    // Note: The received fd number on client side is different
    // and only in context of the client process meaningful.
    // Thus we can use server-side fd as an implicit unique id.
    uint32_t fd = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t format = 0;
};

class RemoteBuffer : public QObject
{
    Q_OBJECT
public:
    void passFd();

Q_SIGNALS:
    void resourceDestroyed();

private:
    explicit RemoteBuffer(Client* client,
                          uint32_t version,
                          uint32_t id,
                          RemoteAccessManager* manager,
                          const RemoteBufferHandle* bufferHandle);
    friend class RemoteAccessManager;

    class Private;
    Private* d_ptr;
};

class RemoteBuffer::Private : public Wayland::Resource<RemoteBuffer>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            RemoteAccessManager* manager,
            const RemoteBufferHandle* bufferHandle,
            RemoteBuffer* q);

    void passFd();

    RemoteAccessManager* manager;

private:
    static void setAddressCallback(wl_client* wlClient,
                                   wl_resource* wlResource,
                                   char const* serviceName,
                                   char const* objectPath);

    static const struct org_kde_kwin_remote_buffer_interface s_interface;

    const RemoteBufferHandle* bufferHandle;
};

}
