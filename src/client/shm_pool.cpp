/********************************************************************
Copyright 2013  Martin Gräßlin <mgraesslin@kde.org>

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
#include "shm_pool.h"
#include "buffer.h"
#include "buffer_p.h"
#include "event_queue.h"
#include "logging.h"
#include "wayland_pointer_p.h"
// Qt
#include <QDebug>
#include <QImage>
#include <QTemporaryFile>
// system
#include <sys/mman.h>
#include <unistd.h>
// wayland
#include <wayland-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN ShmPool::Private
{
public:
    Private(ShmPool* q);
    bool createPool();
    bool resizePool(int32_t newSize);
    QList<std::shared_ptr<Buffer>>::iterator
    getBuffer(QSize const& size, int32_t stride, Buffer::Format format);
    WaylandPointer<wl_shm, wl_shm_destroy> shm;
    WaylandPointer<wl_shm_pool, wl_shm_pool_destroy> pool;
    void* poolData = nullptr;
    int32_t size = 1024;
    std::unique_ptr<QTemporaryFile> tmpFile;
    bool valid = false;
    int offset = 0;
    QList<std::shared_ptr<Buffer>> buffers;
    EventQueue* queue = nullptr;

private:
    ShmPool* q;
};

ShmPool::Private::Private(ShmPool* q)
    : tmpFile(new QTemporaryFile())
    , q(q)
{
}

ShmPool::ShmPool(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

ShmPool::~ShmPool()
{
    release();
}

void ShmPool::release()
{
    d->buffers.clear();
    if (d->poolData) {
        munmap(d->poolData, d->size);
        d->poolData = nullptr;
    }
    d->pool.release();
    d->shm.release();
    d->tmpFile->close();
    d->valid = false;
    d->offset = 0;
}

void ShmPool::setup(wl_shm* shm)
{
    Q_ASSERT(shm);
    Q_ASSERT(!d->shm);
    d->shm.setup(shm);
    d->valid = d->createPool();
}

void ShmPool::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* ShmPool::eventQueue()
{
    return d->queue;
}

bool ShmPool::Private::createPool()
{
    if (!tmpFile->open()) {
        qCDebug(WRAPLAND_CLIENT) << "Could not open temporary file for Shm pool";
        return false;
    }
    if (unlink(tmpFile->fileName().toUtf8().constData()) != 0) {
        qCDebug(WRAPLAND_CLIENT) << "Unlinking temporary file for Shm pool from file system failed";
    }
    if (ftruncate(tmpFile->handle(), size) < 0) {
        qCDebug(WRAPLAND_CLIENT) << "Could not set size for Shm pool file";
        return false;
    }
    poolData = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, tmpFile->handle(), 0);
    pool.setup(wl_shm_create_pool(shm, tmpFile->handle(), size));

    if (!poolData || !pool) {
        qCDebug(WRAPLAND_CLIENT) << "Creating Shm pool failed";
        return false;
    }
    return true;
}

bool ShmPool::Private::resizePool(int32_t newSize)
{
    if (ftruncate(tmpFile->handle(), newSize) < 0) {
        qCDebug(WRAPLAND_CLIENT) << "Could not set new size for Shm pool file";
        return false;
    }
    wl_shm_pool_resize(pool, newSize);
    munmap(poolData, size);
    poolData = mmap(nullptr, newSize, PROT_READ | PROT_WRITE, MAP_SHARED, tmpFile->handle(), 0);
    size = newSize;
    if (!poolData) {
        qCDebug(WRAPLAND_CLIENT) << "Resizing Shm pool failed";
        return false;
    }
    Q_EMIT q->poolResized();
    return true;
}

namespace
{
static Buffer::Format toBufferFormat(QImage const& image)
{
    switch (image.format()) {
    case QImage::Format_ARGB32_Premultiplied:
        return Buffer::Format::ARGB32;
    case QImage::Format_RGB32:
        return Buffer::Format::RGB32;
    case QImage::Format_ARGB32:
        qCWarning(WRAPLAND_CLIENT)
            << "Unsupported image format: " << image.format()
            << ". expect slow performance. Use QImage::Format_ARGB32_Premultiplied";
        return Buffer::Format::ARGB32;
    default:
        qCWarning(WRAPLAND_CLIENT)
            << "Unsupported image format: " << image.format() << ". expect slow performance.";
        return Buffer::Format::ARGB32;
    }
}
}

Buffer::Ptr ShmPool::createBuffer(QImage const& image)
{
    if (image.isNull() || !d->valid) {
        return std::weak_ptr<Buffer>();
    }
    auto format = toBufferFormat(image);
    auto it = d->getBuffer(image.size(), image.bytesPerLine(), format);
    if (it == d->buffers.end()) {
        return std::weak_ptr<Buffer>();
    }
    if (format == Buffer::Format::ARGB32 && image.format() != QImage::Format_ARGB32_Premultiplied) {
        auto imageCopy = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        (*it)->copy(imageCopy.bits());
    } else {
        (*it)->copy(image.bits());
    }
    return std::weak_ptr<Buffer>(*it);
}

Buffer::Ptr
ShmPool::createBuffer(QSize const& size, int32_t stride, void const* src, Buffer::Format format)
{
    if (size.isEmpty() || !d->valid) {
        return std::weak_ptr<Buffer>();
    }
    auto it = d->getBuffer(size, stride, format);
    if (it == d->buffers.end()) {
        return std::weak_ptr<Buffer>();
    }
    (*it)->copy(src);
    return std::weak_ptr<Buffer>(*it);
}

namespace
{
static wl_shm_format toWaylandFormat(Buffer::Format format)
{
    switch (format) {
    case Buffer::Format::ARGB32:
        return WL_SHM_FORMAT_ARGB8888;
    case Buffer::Format::RGB32:
        return WL_SHM_FORMAT_XRGB8888;
    }
    abort();
}
}

Buffer::Ptr ShmPool::getBuffer(QSize const& size, int32_t stride, Buffer::Format format)
{
    auto it = d->getBuffer(size, stride, format);
    if (it == d->buffers.end()) {
        return std::weak_ptr<Buffer>();
    }
    return std::weak_ptr<Buffer>(*it);
}

QList<std::shared_ptr<Buffer>>::iterator
ShmPool::Private::getBuffer(QSize const& s, int32_t stride, Buffer::Format format)
{
    for (auto it = buffers.begin(); it != buffers.end(); ++it) {
        auto buffer = *it;
        if (!buffer->isReleased() || buffer->isUsed()) {
            continue;
        }
        if (buffer->size() != s || buffer->stride() != stride || buffer->format() != format) {
            continue;
        }
        buffer->setReleased(false);
        return it;
    }
    int32_t const byteCount = s.height() * stride;
    if (offset + byteCount > size) {
        if (!resizePool(size + byteCount)) {
            return buffers.end();
        }
    }
    // we don't have a buffer which we could reuse - need to create a new one
    wl_buffer* native = wl_shm_pool_create_buffer(
        pool, offset, s.width(), s.height(), stride, toWaylandFormat(format));
    if (!native) {
        return buffers.end();
    }
    if (queue) {
        queue->addProxy(native);
    }
    Buffer* buffer = new Buffer(q, native, s, stride, offset, format);
    offset += byteCount;
    auto it = buffers.insert(buffers.end(), std::shared_ptr<Buffer>(buffer));
    return it;
}

bool ShmPool::isValid() const
{
    return d->valid;
}

void* ShmPool::poolAddress() const
{
    return d->poolData;
}

wl_shm* ShmPool::shm()
{
    return d->shm;
}

}
}
