/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
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
*********************************************************************/
#include "buffer_p.h"

#include "client.h"
#include "display.h"
#include "logging.h"
#include "surface.h"

#include "wayland/buffer_manager.h"
#include "wayland/display.h"

#include "linux_dmabuf_v1.h"
#include "linux_dmabuf_v1_p.h"

#include <drm_fourcc.h>

#include <EGL/egl.h>
#include <QtGui/qopengl.h>

namespace EGL
{
using eglQueryWaylandBufferWL_func
    = GLboolean (*)(EGLDisplay dpy, struct wl_resource* buffer, EGLint attribute, EGLint* value);
}

namespace Wrapland::Server
{

int constexpr default_bpp{32};

ShmImage::Private::Private(Buffer* buffer, ShmImage::Format format)
    : format{format}
    , stride{wl_shm_buffer_get_stride(buffer->d_ptr->shmBuffer)}
    , bpp{default_bpp}
    , data{static_cast<uchar*>(wl_shm_buffer_get_data(buffer->d_ptr->shmBuffer))}
    , buffer{buffer}
    , display{buffer->d_ptr->display}
{
}

ShmImage::Private::~Private()
{
    display->bufferManager()->endShmAccess();
}

QImage ShmImage::Private::createQImage()
{
    if (!image.isNull()) {
        return image;
    }

    [[maybe_unused]] auto const hasAccess
        = display->bufferManager()->beginShmAccess(buffer->d_ptr->shmBuffer);
    assert(hasAccess);

    QImage::Format qtFormat{QImage::Format_Invalid};
    switch (format) {
    case ShmImage::Format::argb8888:
        qtFormat = QImage::Format_ARGB32_Premultiplied;
        break;
    case ShmImage::Format::xrgb8888:
        qtFormat = QImage::Format_RGB32;
        break;
    default:
        assert(false);
    }

    auto const size = buffer->size();
    return {
        data, size.width(), size.height(), stride, qtFormat, &imageBufferCleanupHandler, display};
}

void ShmImage::Private::imageBufferCleanupHandler(void* info)
{
    auto display = static_cast<Wayland::Display*>(info);
    display->bufferManager()->endShmAccess();
}

ShmImage::ShmImage(Buffer* buffer, ShmImage::Format format)
    : d_ptr{new Private(buffer, format)}
{
}

ShmImage::ShmImage(ShmImage const& img)
    : d_ptr{new Private(img.d_ptr->buffer, img.d_ptr->format)}
{
    d_ptr->display->bufferManager()->beginShmAccess(d_ptr->buffer->d_ptr->shmBuffer);
}

ShmImage& ShmImage::operator=(ShmImage const& img)
{
    if (this != &img) {
        d_ptr->display->bufferManager()->endShmAccess();
        img.d_ptr->display->bufferManager()->beginShmAccess(img.d_ptr->buffer->d_ptr->shmBuffer);

        d_ptr->format = img.d_ptr->format;
        d_ptr->stride = img.d_ptr->stride;
        d_ptr->bpp = img.d_ptr->bpp;
        d_ptr->data = img.d_ptr->data;
        d_ptr->buffer = img.d_ptr->buffer;
        d_ptr->display = img.d_ptr->display;
    }

    return *this;
}

ShmImage::ShmImage(ShmImage&& img) noexcept
    : d_ptr{std::move(img.d_ptr)}
{
}

ShmImage& ShmImage::operator=(ShmImage&& img) noexcept
{
    if (this != &img) {
        d_ptr = std::move(img.d_ptr);
    }

    return *this;
}

ShmImage::~ShmImage() = default;

ShmImage::Format ShmImage::format() const
{
    return d_ptr->format;
}

int32_t ShmImage::stride() const
{
    return d_ptr->stride;
}

int32_t ShmImage::bpp() const
{
    return d_ptr->bpp;
}

uchar* ShmImage::data() const
{
    return d_ptr->data;
}

QImage ShmImage::createQImage()
{
    return d_ptr->createQImage();
}

ShmImage::Format getFormat(wl_shm_buffer* shmBuffer)
{
    switch (wl_shm_buffer_get_format(shmBuffer)) {
    case WL_SHM_FORMAT_ARGB8888:
        return ShmImage::Format::argb8888;
    case WL_SHM_FORMAT_XRGB8888:
        return ShmImage::Format::xrgb8888;
    default:
        return ShmImage::Format::invalid;
    }
}

std::optional<ShmImage> ShmImage::get(Buffer* buffer)
{
    auto display = buffer->d_ptr->display;
    auto shmBuffer = buffer->d_ptr->shmBuffer;

    if (!shmBuffer) {
        return std::nullopt;
    }

    if (!display->bufferManager()->beginShmAccess(shmBuffer)) {
        return std::nullopt;
    }

    auto const imageFormat = getFormat(shmBuffer);
    if (imageFormat == ShmImage::Format::invalid) {
        display->bufferManager()->endShmAccess();
        return std::nullopt;
    }

    return ShmImage(buffer, imageFormat);
}

Buffer::Private::Private(Buffer* q,
                         wl_resource* wlResource,
                         Surface* surface,
                         Wayland::Display* display)
    : resource(wlResource)
    , shmBuffer(wl_shm_buffer_get(wlResource))
    , surface(surface)
    , display(display)
    , q_ptr{q}
{
    if (!shmBuffer
        && wl_resource_instance_of(
            resource, &wl_buffer_interface, &linux_dmabuf_buffer_v1_res_impl::s_interface)) {
        dmabufBuffer
            = Wayland::Resource<linux_dmabuf_buffer_v1_res>::get_handle(resource)->handle.get();
    }

    destroyWrapper.buffer = q;
    destroyWrapper.listener.notify = destroyListenerCallback;
    destroyWrapper.listener.link.prev = nullptr;
    destroyWrapper.listener.link.next = nullptr;
    wl_resource_add_destroy_listener(resource, &destroyWrapper.listener);

    if (shmBuffer) {
        size = QSize(wl_shm_buffer_get_width(shmBuffer), wl_shm_buffer_get_height(shmBuffer));
        // check alpha
        switch (wl_shm_buffer_get_format(shmBuffer)) {
        case WL_SHM_FORMAT_ARGB8888:
            alpha = true;
            break;
        case WL_SHM_FORMAT_XRGB8888:
        default:
            alpha = false;
            break;
        }
    } else if (dmabufBuffer) {
        switch (dmabufBuffer->format()) {
        case DRM_FORMAT_ARGB4444:
        case DRM_FORMAT_ABGR4444:
        case DRM_FORMAT_RGBA4444:
        case DRM_FORMAT_BGRA4444:

        case DRM_FORMAT_ARGB1555:
        case DRM_FORMAT_ABGR1555:
        case DRM_FORMAT_RGBA5551:
        case DRM_FORMAT_BGRA5551:

        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_RGBA8888:
        case DRM_FORMAT_BGRA8888:

        case DRM_FORMAT_ARGB2101010:
        case DRM_FORMAT_ABGR2101010:
        case DRM_FORMAT_RGBA1010102:
        case DRM_FORMAT_BGRA1010102:

        case DRM_FORMAT_XRGB8888_A8:
        case DRM_FORMAT_XBGR8888_A8:
        case DRM_FORMAT_RGBX8888_A8:
        case DRM_FORMAT_BGRX8888_A8:
        case DRM_FORMAT_RGB888_A8:
        case DRM_FORMAT_BGR888_A8:
        case DRM_FORMAT_RGB565_A8:
        case DRM_FORMAT_BGR565_A8:
            alpha = true;
            break;
        default:
            alpha = false;
            break;
        }
        size = dmabufBuffer->size();
    } else if (surface) {
        EGLDisplay eglDisplay = surface->client()->display()->eglDisplay();

        using namespace EGL;
        static eglQueryWaylandBufferWL_func eglQueryWaylandBufferWL{nullptr};
        static bool resolved{false};

        if (!resolved && eglDisplay != EGL_NO_DISPLAY) {
            eglQueryWaylandBufferWL = reinterpret_cast<eglQueryWaylandBufferWL_func>(
                eglGetProcAddress("eglQueryWaylandBufferWL"));
            resolved = true;
        }

        if (eglQueryWaylandBufferWL) {
            EGLint width = 0;
            EGLint height = 0;
            bool valid = false;
            valid = eglQueryWaylandBufferWL(eglDisplay, resource, EGL_WIDTH, &width);
            valid = valid && eglQueryWaylandBufferWL(eglDisplay, resource, EGL_HEIGHT, &height);
            if (valid) {
                size = QSize(width, height);
            }
            // check alpha
            EGLint format = 0;
            if (eglQueryWaylandBufferWL(eglDisplay, resource, EGL_TEXTURE_FORMAT, &format)) {
                switch (format) {
                case EGL_TEXTURE_RGBA:
                    alpha = true;
                    break;
                case EGL_TEXTURE_RGB:
                default:
                    alpha = false;
                    break;
                }
            }
        }
    }
}

Buffer::Private::~Private()
{
    wl_list_remove(&destroyWrapper.listener.link);
    display->bufferManager()->removeBuffer(q_ptr);
}

std::shared_ptr<Buffer> Buffer::make(wl_resource* wlResource, Surface* surface)
{
    auto backendDisplay = Wayland::Display::backendCast(surface->client()->display());
    auto buffer = std::shared_ptr<Buffer>{new Buffer(wlResource, surface)};
    backendDisplay->bufferManager()->addBuffer(buffer);
    return buffer;
}

std::shared_ptr<Buffer> Buffer::make(wl_resource* wlResource, Display* display)
{
    auto backendDisplay = Wayland::Display::backendCast(display);
    auto buffer = std::shared_ptr<Buffer>{new Buffer(wlResource, display)};
    backendDisplay->bufferManager()->addBuffer(buffer);
    return buffer;
}

void Buffer::Private::destroyListenerCallback(wl_listener* listener, [[maybe_unused]] void* data)
{
    // The wl_container_of macro can not be used with auto keyword and in the macro from libwayland
    // the alignment is increased.
    // Relevant clang-tidy checks are:
    // * clang-diagnostic-cast-align
    // * cppcoreguidelines-pro-bounds-pointer-arithmetic
    // * hicpp-use-auto
    // * modernize-use-auto
    // NOLINTNEXTLINE
    DestroyWrapper* wrapper = wl_container_of(listener, wrapper, listener);

    wrapper->buffer->d_ptr->resource = nullptr;
    Q_EMIT wrapper->buffer->resourceDestroyed();
}

Buffer::Buffer(wl_resource* wlResource, Surface* surface)
    : QObject(nullptr)
    , d_ptr(new Private(this,
                        wlResource,
                        surface,
                        Wayland::Display::backendCast(surface->client()->display())))
{
}

Buffer::Buffer(wl_resource* wlResource, Display* display)
    : QObject(nullptr)
    , d_ptr(new Private(this, wlResource, nullptr, Wayland::Display::backendCast(display)))
{
}

std::shared_ptr<Buffer> Buffer::get(Display* display, wl_resource* resource)
{
    if (!resource) {
        return nullptr;
    }
    // TODO(unknown author): verify resource is a buffer
    auto buffer = Wayland::Display::backendCast(display)->bufferManager()->fromResource(resource);
    return buffer ? buffer.value() : make(resource, display);
}

Buffer::~Buffer()
{
    if (d_ptr->committed && d_ptr->resource) {
        wl_buffer_send_release(d_ptr->resource);
        wl_client_flush(wl_resource_get_client(d_ptr->resource));
    }
}

std::optional<ShmImage> Buffer::shmImage()
{
    return ShmImage::get(this);
}

Surface* Buffer::surface() const
{
    return d_ptr->surface;
}

wl_shm_buffer* Buffer::shmBuffer()
{
    return d_ptr->shmBuffer;
}

linux_dmabuf_buffer_v1* Buffer::linuxDmabufBuffer()
{
    return d_ptr->dmabufBuffer;
}

wl_resource* Buffer::resource() const
{
    return d_ptr->resource;
}

QSize Buffer::size() const
{
    return d_ptr->size;
}

bool Buffer::hasAlphaChannel() const
{
    return d_ptr->alpha;
}

void Buffer::setCommitted()
{
    d_ptr->committed = true;
}

}
