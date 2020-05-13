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

#include "wayland/display.h"

#include "drm_fourcc.h"

#include "wayland-server-protocol.h"

#include <Wrapland/Server/wraplandserver_export.h>

#include <QVector>

#include <array>
#include <assert.h>
#include <unistd.h>

namespace Wrapland::Server
{

LinuxDmabufV1::Private::Private(LinuxDmabufV1* q, D_isplay* display)
    : LinuxDmabufV1Global(q, display, &zwp_linux_dmabuf_v1_interface, &s_interface)
{
}

LinuxDmabufV1::Private::~Private() = default;

const struct zwp_linux_dmabuf_v1_interface LinuxDmabufV1::Private::s_interface = {
    resourceDestroyCallback,
    createParamsCallback,
};

void LinuxDmabufV1::Private::bindInit(Wayland::Resource<LinuxDmabufV1, LinuxDmabufV1Global>* bind)
{
    // Send formats & modifiers.
    QHash<uint32_t, QSet<uint64_t>>::const_iterator it = supportedFormatsWithModifiers.constBegin();

    while (it != supportedFormatsWithModifiers.constEnd()) {
        QSet<uint64_t> modifiers = it.value();
        if (modifiers.isEmpty()) {
            modifiers << DRM_FORMAT_MOD_INVALID;
        }

        for (uint64_t modifier : qAsConst(modifiers)) {
            if (bind->version() >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
                const uint32_t modifier_lo = modifier & 0xFFFFFFFF;
                const uint32_t modifier_hi = modifier >> 32;
                send<zwp_linux_dmabuf_v1_send_modifier, ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION>(
                    it.key(), modifier_hi, modifier_lo);
            } else if (modifier == DRM_FORMAT_MOD_LINEAR || modifier == DRM_FORMAT_MOD_INVALID) {
                send<zwp_linux_dmabuf_v1_send_format>(it.key());
            }
        }

        it++;
    }
}

void LinuxDmabufV1::Private::createParamsCallback(wl_client* wlClient,
                                                  wl_resource* wlResource,
                                                  uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr;
    auto client = priv->display()->getClient(wlClient);

    auto params = new ParamsWrapperV1(client->handle(), priv->version(), id, priv);
}

LinuxDmabufV1::LinuxDmabufV1(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
}

LinuxDmabufV1::~LinuxDmabufV1() = default;

void LinuxDmabufV1::setImpl(LinuxDmabufV1::Impl* impl)
{
    d_ptr->impl = impl;
}

void LinuxDmabufV1::setSupportedFormatsWithModifiers(QHash<uint32_t, QSet<uint64_t>> set)
{
    d_ptr->supportedFormatsWithModifiers = set;
}

ParamsWrapperV1::ParamsWrapperV1(Client* client,
                                 uint32_t version,
                                 uint32_t id,
                                 LinuxDmabufV1::Private* dmabuf)
    : QObject()
    , d_ptr(new ParamsV1(client, version, id, dmabuf, this))
{
}

ParamsV1::ParamsV1(Client* client,
                   uint32_t version,
                   uint32_t id,
                   LinuxDmabufV1::Private* dmabuf,
                   ParamsWrapperV1* q)
    : Wayland::Resource<ParamsWrapperV1>(client,
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

ParamsV1::~ParamsV1()
{
    // Close the file descriptors
    for (auto& plane : m_planes) {
        if (plane.fd != -1) {
            ::close(plane.fd);
        }
    }
}

struct zwp_linux_buffer_params_v1_interface const ParamsV1::s_interface = {
    destroyCallback,
    addCallback,
    createCallback,
    createImmedCallback,
};

void ParamsV1::addCallback(wl_client* wlClient,
                           wl_resource* wlResource,
                           int fd,
                           uint32_t plane_idx,
                           uint32_t offset,
                           uint32_t stride,
                           uint32_t modifier_hi,
                           uint32_t modifier_lo)
{
    auto params = handle(wlResource);
    params->d_ptr->add(fd, plane_idx, offset, stride, (uint64_t(modifier_hi) << 32) | modifier_lo);
}

void ParamsV1::createCallback(wl_client* wlClient,
                              wl_resource* wlResource,
                              int width,
                              int height,
                              uint32_t format,
                              uint32_t flags)
{
    auto params = handle(wlResource);
    params->d_ptr->create(0, QSize(width, height), format, flags);
}

void ParamsV1::createImmedCallback(wl_client* wlClient,
                                   wl_resource* wlResource,
                                   uint32_t new_id,
                                   int width,
                                   int height,
                                   uint32_t format,
                                   uint32_t flags)
{
    auto params = handle(wlResource);
    params->d_ptr->create(new_id, QSize(width, height), format, flags);
}

void ParamsV1::create(uint32_t bufferId, const QSize& size, uint32_t format, uint32_t flags)
{
    // Validate the parameters
    // -----------------------
    const uint32_t width = size.width();
    const uint32_t height = size.height();

    if (m_createRequested) {
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                  "params was already used to create a wl_buffer");
        return;
    }
    m_createRequested = true;

    if (m_planeCount == 0) {
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                  "no dmabuf has been added to the params");
        return;
    }

    // Check for holes in the dmabufs set (e.g. [0, 1, 3])
    for (uint32_t i = 0; i < m_planeCount; i++) {
        if (m_planes[i].fd != -1) {
            continue;
        }
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                  "no dmabuf has been added for plane %i",
                  i);
        return;
    }

    if (width < 1 || height < 1) {
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                  "invalid width %d or height %d",
                  width,
                  height);
        return;
    }

    for (uint32_t i = 0; i < m_planeCount; i++) {
        auto& plane = m_planes[i];

        if (uint64_t(plane.offset) + plane.stride > UINT32_MAX) {
            postError(
                ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS, "size overflow for plane %i", i);
            return;
        }

        if (i == 0 && uint64_t(plane.offset) + plane.stride * height > UINT32_MAX) {
            postError(
                ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS, "size overflow for plane %i", i);
            return;
        }

        // Don't report an error as it might be caused by the kernel not supporting seeking on
        // dmabuf
        off_t size = ::lseek(plane.fd, 0, SEEK_END);
        if (size == -1)
            continue;

        if (plane.offset >= size) {
            postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                      "invalid offset %i for plane %i",
                      plane.offset,
                      i);
            return;
        }

        if (plane.offset + plane.stride > size) {
            postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                      "invalid stride %i for plane %i",
                      plane.stride,
                      i);
            return;
        }

        // Only valid for first plane as other planes might be
        // sub-sampled according to fourcc format
        if (i == 0 && plane.offset + plane.stride * height > size) {
            postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                      "invalid buffer stride or height for plane %i",
                      i);
            return;
        }
    }

    // Import the buffer
    // -----------------
    QVector<LinuxDmabufV1::Plane> planes;
    planes.reserve(m_planeCount);
    for (uint32_t i = 0; i < m_planeCount; i++) {
        planes << m_planes[i];
    }

    auto buffer = m_dmabuf->impl->importBuffer(planes, format, size, (LinuxDmabufV1::Flags)flags);
    if (!buffer) {
        if (bufferId == 0) {
            send<zwp_linux_buffer_params_v1_send_failed>();
            return;
        }
        // Since the behavior is left implementation defined by the protocol in case of create_immed
        // failure due to an unknown cause, we choose to treat it as a fatal error and immediately
        // kill the client instead of creating an invalid handle and waiting for it to be used.
        postError(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                  "importing the supplied dmabufs failed");
    }
    // The buffer has ownership of the file descriptors now
    for (auto& plane : m_planes) {
        plane.fd = -1;
    }

    buffer->d_ptr->buffer = new BufferV1(client()->handle(), 1, bufferId, buffer);
    // TODO: error handling

    // Send a 'created' event when the request is not for an immediate import, i.e. bufferId is 0.
    if (bufferId == 0) {
        send<zwp_linux_buffer_params_v1_send_created>(buffer->d_ptr->buffer->resource());
    }
}

void ParamsV1::add(int fd, uint32_t plane_idx, uint32_t offset, uint32_t stride, uint64_t modifier)
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

    auto& plane = m_planes[plane_idx];

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

LinuxDmabufBufferV1::Private::Private(uint32_t format, const QSize& size, LinuxDmabufBufferV1* q)
    : format(format)
    , size(size)
    , buffer(nullptr)
{
}

// TODO: This does not necessarily need to be a QObject. resourceDestroyed signal is not really
// needed.
LinuxDmabufBufferV1::LinuxDmabufBufferV1(uint32_t format, const QSize& size, QObject* parent)
    : QObject()
    , d_ptr(new LinuxDmabufBufferV1::Private(format, size, this))
{
}

LinuxDmabufBufferV1::~LinuxDmabufBufferV1()
{
    delete d_ptr;
}

uint32_t LinuxDmabufBufferV1::format() const
{
    return d_ptr->format;
}

QSize LinuxDmabufBufferV1::size() const
{
    return d_ptr->size;
}

BufferV1::BufferV1(Client* client, uint32_t version, uint32_t id, LinuxDmabufBufferV1* q)
    : Wayland::Resource<LinuxDmabufBufferV1>(client,
                                             version,
                                             id,
                                             &wl_buffer_interface,
                                             &s_interface,
                                             q)
{
}

const struct wl_buffer_interface BufferV1::s_interface = {destroyCallback};

const struct wl_buffer_interface* BufferV1::interface()
{
    return &s_interface;
}

}
