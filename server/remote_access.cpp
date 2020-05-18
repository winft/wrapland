/****************************************************************************
Copyright 2016  Oleg Chernovskiy <kanedias@xaker.ru>
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
#include "remote_access_p.h"

#include "display.h"
#include "output_p.h"

#include <wayland-remote-access-server-protocol.h>

#include <QHash>
#include <QMutableHashIterator>

#include "logging.h"

#include <functional>

namespace Wrapland::Server
{

RemoteAccessManager::Private::Private(Display* display, RemoteAccessManager* q)
    : RemoteAccessManagerGlobal(q,
                                display,
                                &org_kde_kwin_remote_access_manager_interface,
                                &s_interface)
{
    create();
}

void RemoteAccessManager::Private::sendBufferReady(Output* output, RemoteBufferHandle* buf)
{
    BufferHolder holder{buf, 0};

    // Notify clients.
    qCDebug(WRAPLAND_SERVER) << "Server buffer sent: fd" << buf->fd();

    for (auto bind : getBinds()) {
        auto boundOutputs = output->d_ptr->getBinds(bind->client()->handle());

        if (boundOutputs.empty()) {
            continue;
        }

        // No reason for client to bind wl_output multiple times, send only to first one.
        send<org_kde_kwin_remote_access_manager_send_buffer_ready>(
            bind, buf->fd(), boundOutputs[0]->resource());
        holder.counter++;

        // TODO: how to count back in case client goes away before buffer got by it?
    }

    if (holder.counter == 0) {
        // Buffer was not requested by any client.
        Q_EMIT handle()->bufferReleased(buf);
        return;
    }

    // Store buffer locally, clients will ask for it later.
    sentBuffers[buf->fd()] = holder;
}

struct org_kde_kwin_remote_access_manager_interface const RemoteAccessManager::Private::s_interface
    = {
        getBufferCallback,
        resourceDestroyCallback,
};

void RemoteAccessManager::Private::getBufferCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource,
                                                     uint32_t id,
                                                     int32_t internalBufId)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    // Client asks for buffer we earlier announced, we must have it.
    if (Q_UNLIKELY(!priv->sentBuffers.contains(internalBufId))) {
        // No such buffer (?)
        wl_resource_post_no_memory(wlResource);
        return;
    }

    BufferHolder& bufferHolder = priv->sentBuffers[internalBufId];

    auto buffer = new RemoteBuffer(
        bind->client()->handle(), bind->version(), id, priv->handle(), bufferHolder.buf);
    // TODO: check if created.

    connect(buffer,
            &RemoteBuffer::resourceDestroyed,
            priv->handle(),
            [priv, &bufferHolder, wlResource] {
                if (!priv->getBind(wlResource)) {
                    // Remote buffer destroy confirmed after client is already gone.
                    // All relevant buffers are already unreferenced.
                    return;
                }
                qCDebug(WRAPLAND_SERVER)
                    << "Remote buffer returned, client" << wl_resource_get_id(wlResource) << ", fd"
                    << bufferHolder.buf->fd();
                if (priv->unref(bufferHolder) == 0) {
                    priv->sentBuffers.remove(bufferHolder.buf->fd());
                }
            });

    // Send buffer params.
    buffer->passFd();
}

void RemoteAccessManager::Private::prepareUnbind([[maybe_unused]] RemoteAccessManagerBind* bind)
{
    // As one bind is gone all holders must decrement their ref counter by one.
    QMutableHashIterator<uint32_t, BufferHolder> itr(sentBuffers);
    while (itr.hasNext()) {
        BufferHolder& bh = itr.next().value();
        if (unref(bh) == 0) {
            itr.remove();
        }
    }
}

int RemoteAccessManager::Private::unref(BufferHolder& bufferHolder)
{
    bufferHolder.counter--;
    Q_ASSERT(bufferHolder.counter >= 0);

    if (bufferHolder.counter == 0) {
        // No more clients using this buffer.
        qCDebug(WRAPLAND_SERVER) << "Buffer released, fd:" << bufferHolder.buf->fd();
        Q_EMIT handle()->bufferReleased(bufferHolder.buf);
        return 0;
    }

    return bufferHolder.counter;
}

RemoteAccessManager::RemoteAccessManager(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

RemoteAccessManager::~RemoteAccessManager() = default;

void RemoteAccessManager::sendBufferReady(Output* output, RemoteBufferHandle* buf)
{
    d_ptr->sendBufferReady(output, buf);
}

bool RemoteAccessManager::bound() const
{
    return !d_ptr->getBinds().empty();
}

/////////////////////////// Remote buffer ///////////////////////////

struct org_kde_kwin_remote_buffer_interface const RemoteBuffer::Private::s_interface
    = {destroyCallback};

RemoteBuffer::Private::Private(Client* client,
                               uint32_t version,
                               uint32_t id,
                               RemoteAccessManager* manager,
                               const RemoteBufferHandle* bufferHandle,
                               RemoteBuffer* q)
    : Wayland::Resource<RemoteBuffer>(client,
                                      version,
                                      id,
                                      &org_kde_kwin_remote_buffer_interface,
                                      &s_interface,
                                      q)
    , manager{manager}
    , bufferHandle{bufferHandle}
{
}

void RemoteBuffer::Private::passFd()
{
    send<org_kde_kwin_remote_buffer_send_gbm_handle>(bufferHandle->fd(),
                                                     bufferHandle->width(),
                                                     bufferHandle->height(),
                                                     bufferHandle->stride(),
                                                     bufferHandle->format());
}

RemoteBuffer::RemoteBuffer(Client* client,
                           uint32_t version,
                           uint32_t id,
                           RemoteAccessManager* manager,
                           const RemoteBufferHandle* bufferHandle)
    : d_ptr(new Private(client, version, id, manager, bufferHandle, this))
{
}

void RemoteBuffer::passFd()
{
    d_ptr->passFd();
}

/////////////////////////// Remote buffer handle ///////////////////////////

RemoteBufferHandle::RemoteBufferHandle()
    : QObject()
    , d_ptr(new Private)
{
}

RemoteBufferHandle::~RemoteBufferHandle() = default;

void RemoteBufferHandle::setFd(uint32_t fd)
{
    d_ptr->fd = fd;
}

uint32_t RemoteBufferHandle::fd() const
{
    return d_ptr->fd;
}

void RemoteBufferHandle::setSize(uint32_t width, uint32_t height)
{
    d_ptr->width = width;
    d_ptr->height = height;
}

uint32_t RemoteBufferHandle::width() const
{
    return d_ptr->width;
}

uint32_t RemoteBufferHandle::height() const
{
    return d_ptr->height;
}

void RemoteBufferHandle::setStride(uint32_t stride)
{
    d_ptr->stride = stride;
}

uint32_t RemoteBufferHandle::stride() const
{
    return d_ptr->stride;
}

void RemoteBufferHandle::setFormat(uint32_t format)
{
    d_ptr->format = format;
}

uint32_t RemoteBufferHandle::format() const
{
    return d_ptr->format;
}

}
