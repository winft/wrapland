/********************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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
#include "event_queue.h"
#include "linux_dmabuf_v1_p.h"

#include <drm_fourcc.h>

namespace Wrapland::Client
{

constexpr size_t modifier_shift = 32;
constexpr uint32_t modifier_cut = 0xFFFFFFFF;

const struct zwp_linux_dmabuf_v1_listener LinuxDmabufV1::Private::s_listener = {
    LinuxDmabufV1::Private::callbackFormat,
    LinuxDmabufV1::Private::callbackModifier,
};

LinuxDmabufV1::Private::Private(LinuxDmabufV1* q)
    : q_ptr(q)
{
}

LinuxDmabufV1::LinuxDmabufV1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

LinuxDmabufV1::~LinuxDmabufV1() = default;

void LinuxDmabufV1::Private::callbackFormat(
    [[maybe_unused]] void* data,
    [[maybe_unused]] zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
    uint32_t format)
{
    /* DEPRECATED */
    auto dmabuf = reinterpret_cast<LinuxDmabufV1*>(data);
    dmabuf->d_ptr->formats.push_back({format, DRM_FORMAT_MOD_INVALID});
    Q_EMIT dmabuf->supportedFormatsChanged();
}

void LinuxDmabufV1::Private::callbackModifier(
    void* data,
    [[maybe_unused]] zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
    uint32_t format,
    uint32_t modifier_hi,
    uint32_t modifier_lo)
{
    LinuxDmabufV1* dmabuf = reinterpret_cast<LinuxDmabufV1*>(data);

    auto modifier = (static_cast<uint64_t>(modifier_hi) << modifier_shift) | modifier_lo;
    dmabuf->d_ptr->formats.push_back({format, modifier});
    Q_EMIT dmabuf->supportedFormatsChanged();
}

std::vector<drm_format> const& LinuxDmabufV1::supportedFormats()
{
    return d_ptr->formats;
}

ParamsV1* LinuxDmabufV1::createParamsV1(QObject* parent)
{
    Q_ASSERT(isValid());
    auto params = new ParamsV1(parent);
    auto w = zwp_linux_dmabuf_v1_create_params(d_ptr->dmabuf);

    if (d_ptr->queue) {
        d_ptr->queue->addProxy(w);
    }
    params->setup(w);
    return params;
}

void LinuxDmabufV1::release()
{
    d_ptr->dmabuf.release();
}

bool LinuxDmabufV1::isValid() const
{
    return d_ptr->dmabuf.isValid();
}

void LinuxDmabufV1::setup(zwp_linux_dmabuf_v1* dmabuf)
{
    Q_ASSERT(dmabuf);
    Q_ASSERT(!d_ptr->dmabuf);
    d_ptr->dmabuf.setup(dmabuf);
    zwp_linux_dmabuf_v1_add_listener(dmabuf, &(d_ptr->s_listener), this);
}

void LinuxDmabufV1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* LinuxDmabufV1::eventQueue()
{
    return d_ptr->queue;
}

LinuxDmabufV1::operator zwp_linux_dmabuf_v1*()
{
    return d_ptr->dmabuf;
}

LinuxDmabufV1::operator zwp_linux_dmabuf_v1*() const
{
    return d_ptr->dmabuf;
}

zwp_linux_buffer_params_v1_listener const ParamsV1::Private::s_listener = {
    ParamsV1::Private::callbackCreateSucceeded,
    ParamsV1::Private::callbackBufferCreationFail,
};

ParamsV1::Private::Private(ParamsV1* q)
    : q_ptr(q)
{
}

ParamsV1::ParamsV1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

ParamsV1::~ParamsV1()
{
    release();
}

void ParamsV1::release()
{
    d_ptr->params.release();
}

void ParamsV1::Private::callbackCreateSucceeded(
    [[maybe_unused]] void* data,
    [[maybe_unused]] zwp_linux_buffer_params_v1* wlParams,
    wl_buffer* wlBuffer)
{
    auto params = reinterpret_cast<ParamsV1*>(data);
    params->d_ptr->createdBuffer = wlBuffer;
    Q_EMIT params->createSuccess(wlBuffer);
}

void ParamsV1::Private::callbackBufferCreationFail(
    void* data,
    [[maybe_unused]] zwp_linux_buffer_params_v1* wlParams)
{
    auto params = reinterpret_cast<ParamsV1*>(data);
    params->d_ptr->createdBuffer = nullptr;
    Q_EMIT params->createFail();
}

void ParamsV1::setup(zwp_linux_buffer_params_v1* params)
{
    Q_ASSERT(params);
    Q_ASSERT(!d_ptr->params);
    d_ptr->params.setup(params);
    zwp_linux_buffer_params_v1_add_listener(d_ptr->params, &(d_ptr->s_listener), this);
}

bool ParamsV1::isValid() const
{
    return d_ptr->params.isValid();
}

void ParamsV1::addDmabuf(int32_t fd,
                         uint32_t plane_idx,
                         uint32_t offset,
                         uint32_t stride,
                         uint64_t modifier)
{
    auto modifier_hi = static_cast<uint32_t>(modifier >> modifier_shift);
    auto modifier_lo = static_cast<uint32_t>(modifier & modifier_cut);
    zwp_linux_buffer_params_v1_add(
        d_ptr->params, fd, plane_idx, offset, stride, modifier_hi, modifier_lo);
}

void ParamsV1::createDmabuf(int32_t width, int32_t height, uint32_t format, uint32_t flags)
{
    if (d_ptr->createdBuffer) {
        Q_EMIT createSuccess(d_ptr->createdBuffer);
        return;
    }
    zwp_linux_buffer_params_v1_create(d_ptr->params, width, height, format, flags);
}

wl_buffer*
ParamsV1::createDmabufImmediate(int32_t width, int32_t height, uint32_t format, uint32_t flags)
{
    if (d_ptr->createdBuffer) {
        return d_ptr->createdBuffer;
    }
    d_ptr->createdBuffer
        = zwp_linux_buffer_params_v1_create_immed(d_ptr->params, width, height, format, flags);
    return d_ptr->createdBuffer;
}

wl_buffer* ParamsV1::getBuffer()
{
    return d_ptr->createdBuffer;
}

ParamsV1::operator zwp_linux_buffer_params_v1*()
{
    return d_ptr->params;
}

ParamsV1::operator zwp_linux_buffer_params_v1*() const
{
    return d_ptr->params;
}

}
