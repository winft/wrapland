/********************************************************************
Copyright © 2018 Fredrik Höglund <fredrik@kde.org>
Copyright © 2019-2020 Roman Gilg <subdiff@gmail.com>

Based on the libweston implementation,
Copyright © 2014, 2015 Collabora, Ltd.

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
#include "linux_dmabuf_v1.h"
#include "linux_dmabuf_v1_p.h"

#include "display.h"
#include "wayland/display.h"

#include "wayland-server-protocol.h"
#include <drm_fourcc.h>

#include <QVector>

#include <array>
#include <cassert>
#include <unistd.h>

namespace Wrapland::Server
{

linux_dmabuf_v1::Private::Private(linux_dmabuf_v1* q,
                                  Display* display,
                                  linux_dmabuf_import_v1 import)
    : linux_dmabuf_v1_global(q, display, &zwp_linux_dmabuf_v1_interface, &s_interface)
    , import{std::move(import)}
{
    create();
}

linux_dmabuf_v1::Private::~Private() = default;

const struct zwp_linux_dmabuf_v1_interface linux_dmabuf_v1::Private::s_interface = {
    resourceDestroyCallback,
    cb<create_params_callback>,
    // TODO(romangg): Update linux-dmabuf protocol version (currently at 3).
    // NOLINTNEXTLINE(clang-diagnostic-missing-field-initializers)
};

constexpr size_t modifier_shift = 32;

void linux_dmabuf_v1::Private::bindInit(linux_dmabuf_v1_bind* bind)
{
    // Send formats & modifiers.
    QHash<uint32_t, QSet<uint64_t>>::const_iterator it = supported_formats.constBegin();

    for (;;) {
        if (it == supported_formats.constEnd()) {
            break;
        }
        QSet<uint64_t> modifiers = it.value();
        if (modifiers.isEmpty()) {
            modifiers << DRM_FORMAT_MOD_INVALID;
        }

        for (uint64_t modifier : qAsConst(modifiers)) {
            if (bind->version >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
                const uint32_t modifier_lo = modifier & 0xFFFFFFFF;
                const uint32_t modifier_hi = modifier >> modifier_shift;
                send<zwp_linux_dmabuf_v1_send_modifier, ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION>(
                    bind, it.key(), modifier_hi, modifier_lo);
            } else if (modifier == DRM_FORMAT_MOD_LINEAR || modifier == DRM_FORMAT_MOD_INVALID) {
                send<zwp_linux_dmabuf_v1_send_format>(bind, it.key());
            }
        }

        it++;
    }
}

void linux_dmabuf_v1::Private::create_params_callback(linux_dmabuf_v1_bind* bind, uint32_t id)
{
    auto priv = bind->global()->handle->d_ptr.get();

    [[maybe_unused]] auto params
        = new linux_dmabuf_params_wrapper_v1(bind->client->handle, bind->version, id, priv);
}

linux_dmabuf_v1::linux_dmabuf_v1(Display* display, linux_dmabuf_import_v1 import)
    : d_ptr(new Private(this, display, std::move(import)))
{
}

linux_dmabuf_v1::~linux_dmabuf_v1() = default;

void linux_dmabuf_v1::set_formats(QHash<uint32_t, QSet<uint64_t>> const& set)
{
    d_ptr->supported_formats = set;
}

linux_dmabuf_params_wrapper_v1::linux_dmabuf_params_wrapper_v1(Client* client,
                                                               uint32_t version,
                                                               uint32_t id,
                                                               linux_dmabuf_v1::Private* dmabuf)
    : QObject(nullptr)
    , d_ptr(new linux_dmabuf_params_v1(client, version, id, dmabuf, this))
{
}

linux_dmabuf_params_v1::linux_dmabuf_params_v1(Client* client,
                                               uint32_t version,
                                               uint32_t id,
                                               linux_dmabuf_v1::Private* dmabuf,
                                               linux_dmabuf_params_wrapper_v1* q)
    : Wayland::Resource<linux_dmabuf_params_wrapper_v1>(client,
                                                        version,
                                                        id,
                                                        &zwp_linux_buffer_params_v1_interface,
                                                        &s_interface,
                                                        q)
    , m_dmabuf(dmabuf)
{
    for (auto& plane : m_planes) {
        plane.fd = -1;
        plane.offset = 0;
        plane.stride = 0;
        plane.modifier = 0;
    }
}

linux_dmabuf_params_v1::~linux_dmabuf_params_v1()
{
    // Close the file descriptors
    for (auto& plane : m_planes) {
        if (plane.fd != -1) {
            ::close(plane.fd);
        }
    }
}

struct zwp_linux_buffer_params_v1_interface const linux_dmabuf_params_v1::s_interface = {
    destroyCallback,
    add_callback,
    create_callback,
    create_immed_callback,
};

void linux_dmabuf_params_v1::add_callback(wl_client* /*wlClient*/,
                                          wl_resource* wlResource,
                                          int fd,
                                          uint32_t plane_idx,
                                          uint32_t offset,
                                          uint32_t stride,
                                          uint32_t modifier_hi,
                                          uint32_t modifier_lo)
{
    auto params = get_handle(wlResource);
    params->d_ptr->add(
        fd, plane_idx, offset, stride, (uint64_t(modifier_hi) << modifier_shift) | modifier_lo);
}

void linux_dmabuf_params_v1::create_callback(wl_client* /*wlClient*/,
                                             wl_resource* wlResource,
                                             int width,
                                             int height,
                                             uint32_t format,
                                             uint32_t flags)
{
    auto params = get_handle(wlResource);
    params->d_ptr->create(0, QSize(width, height), format, flags);
}

void linux_dmabuf_params_v1::create_immed_callback(wl_client* /*wlClient*/,
                                                   wl_resource* wlResource,
                                                   uint32_t new_id,
                                                   int width,
                                                   int height,
                                                   uint32_t format,
                                                   uint32_t flags)
{
    auto params = get_handle(wlResource);
    params->d_ptr->create(new_id, QSize(width, height), format, flags);
}

void linux_dmabuf_params_v1::create(uint32_t buffer_id,
                                    const QSize& size,
                                    uint32_t format,
                                    uint32_t flags)
{
    if (!validate_params(size)) {
        return;
    }

    QVector<linux_dmabuf_plane_v1> planes;
    planes.reserve(static_cast<int>(m_planeCount));

    for (uint32_t i = 0; i < m_planeCount; i++) {
        planes << m_planes.at(i);
    }

    auto buffer = m_dmabuf->import(planes, format, size, linux_dmabuf_flags_v1(flags));
    if (!buffer) {
        if (buffer_id == 0) {
            send<zwp_linux_buffer_params_v1_send_failed>();
            return;
        }
        // Since the behavior is left implementation defined by the protocol in case of create_immed
        // failure due to an unknown cause, we choose to treat it as a fatal error and immediately
        // kill the client instead of creating an invalid handle and waiting for it to be used.
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                  "importing the supplied dmabufs failed");
        return;
    }

    // The buffer has ownership of the file descriptors now
    for (auto& plane : m_planes) {
        plane.fd = -1;
    }

    // We import the buffer from the consumer. The consumer ensures the buffer exists.
    // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
    auto res = new linux_dmabuf_buffer_v1_res(client->handle, 1, buffer_id, buffer);

    // TODO(romangg): error handling

    // Send a 'created' event when the request is not for an immediate import, i.e. buffer_id is 0.
    if (buffer_id == 0) {
        send<zwp_linux_buffer_params_v1_send_created>(res->impl->resource);
    }
}

bool linux_dmabuf_params_v1::validate_params(QSize const& size)
{
    auto const width = static_cast<uint32_t>(size.width());
    auto const height = static_cast<uint32_t>(size.height());

    if (m_createRequested) {
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                  "params was already used to create a wl_buffer");
        return false;
    }
    m_createRequested = true;

    if (m_planeCount == 0) {
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                  "no dmabuf has been added to the params");
        return false;
    }

    // Check for holes in the dmabufs set (e.g. [0, 1, 3])
    for (uint32_t i = 0; i < m_planeCount; i++) {
        if (m_planes.at(i).fd != -1) {
            continue;
        }
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                  "no dmabuf has been added for plane %i",
                  i);
        return false;
    }

    if (width < 1 || height < 1) {
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                  "invalid width %d or height %d",
                  width,
                  height);
        return false;
    }

    for (uint32_t i = 0; i < m_planeCount; i++) {
        auto& plane = m_planes.at(i);

        if (uint64_t(plane.offset) + plane.stride > UINT32_MAX) {
            postError(
                ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS, "size overflow for plane %i", i);
            return false;
        }

        if (i == 0
            && uint64_t(plane.offset) + static_cast<uint64_t>(plane.stride * height) > UINT32_MAX) {
            postError(
                ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS, "size overflow for plane %i", i);
            return false;
        }

        // Don't report an error as it might be caused by the kernel not supporting seeking on
        // dmabuf
        off_t size = ::lseek(plane.fd, 0, SEEK_END);
        if (size == -1) {
            continue;
        }

        if (plane.offset >= size) {
            postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                      "invalid offset %i for plane %i",
                      plane.offset,
                      i);
            return false;
        }

        if (plane.offset + plane.stride > size) {
            postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                      "invalid stride %i for plane %i",
                      plane.stride,
                      i);
            return false;
        }

        // Only valid for first plane as other planes might be
        // sub-sampled according to fourcc format
        if (i == 0 && plane.offset + plane.stride * height > size) {
            postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                      "invalid buffer stride or height for plane %i",
                      i);
            return false;
        }
    }

    return true;
}

void linux_dmabuf_params_v1::add(int fd,
                                 uint32_t plane_idx,
                                 uint32_t offset,
                                 uint32_t stride,
                                 uint64_t modifier)
{
    if (m_createRequested) {
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                  "params was already used to create a wl_buffer");
        ::close(fd);
        return;
    }

    if (plane_idx >= m_planes.size()) {
        postError(
            ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "plane index %u is too high", plane_idx);
        ::close(fd);
        return;
    }

    auto& plane = m_planes.at(plane_idx);

    if (plane.fd != -1) {
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                  "a dmabuf has already been added for plane %u",
                  plane_idx);
        ::close(fd);
        return;
    }

    plane.fd = fd;
    plane.offset = offset;
    plane.stride = stride;
    plane.modifier = modifier;

    m_planeCount++;
}

linux_dmabuf_buffer_v1::Private::Private(uint32_t format, const QSize& size)
    : format(format)
    , size(size)
{
}

linux_dmabuf_buffer_v1::linux_dmabuf_buffer_v1(uint32_t format, const QSize& size)
    : d_ptr{new linux_dmabuf_buffer_v1::Private(format, size)}
{
}

linux_dmabuf_buffer_v1::~linux_dmabuf_buffer_v1()
{
    delete d_ptr;
}

uint32_t linux_dmabuf_buffer_v1::format() const
{
    return d_ptr->format;
}

QSize linux_dmabuf_buffer_v1::size() const
{
    return d_ptr->size;
}

linux_dmabuf_buffer_v1_res::linux_dmabuf_buffer_v1_res(Client* client,
                                                       uint32_t version,
                                                       uint32_t id,
                                                       linux_dmabuf_buffer_v1* handle)
    : handle{handle}
    , impl{new linux_dmabuf_buffer_v1_res_impl(client, version, id, this)}
{
}

linux_dmabuf_buffer_v1_res_impl::linux_dmabuf_buffer_v1_res_impl(Client* client,
                                                                 uint32_t version,
                                                                 uint32_t id,
                                                                 linux_dmabuf_buffer_v1_res* q)
    : Wayland::Resource<linux_dmabuf_buffer_v1_res>(client,
                                                    version,
                                                    id,
                                                    &wl_buffer_interface,
                                                    &s_interface,
                                                    q)
{
}

struct wl_buffer_interface const linux_dmabuf_buffer_v1_res_impl::s_interface = {destroyCallback};

}
