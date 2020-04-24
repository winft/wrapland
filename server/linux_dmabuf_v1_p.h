/********************************************************************
Copyright © 2018 Fredrik Höglund <fredrik@kde.org>
Copyright © 2019-2020 Roman Gilg <subdiff@gmail.com>

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
#pragma once

#include "linux_dmabuf_v1.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include "wayland-linux-dmabuf-unstable-v1-server-protocol.h"

#include <array>

namespace Wrapland::Server
{
class BufferV1;

class LinuxDmabufV1::Private : public Wayland::Global<LinuxDmabufV1>
{
public:
    Private(LinuxDmabufV1* q, D_isplay* display);
    ~Private() override final;

    uint32_t version() const override final;
    void bindInit(Wayland::Resource<LinuxDmabufV1, Global<LinuxDmabufV1>>* bind) override final;

    static const struct wl_buffer_interface* bufferInterface();
    static void createParamsCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);

    LinuxDmabufV1::Impl* impl;
    QHash<uint32_t, QSet<uint64_t>> supportedFormatsWithModifiers;

    LinuxDmabufV1* q_ptr;

private:
    static const uint32_t s_version;

    static const struct zwp_linux_dmabuf_v1_interface s_interface;
};

class LinuxDmabufBufferV1::Private
{
public:
    Private(uint32_t format, const QSize& size, LinuxDmabufBufferV1* q);
    ~Private() = default;

    uint32_t format;
    QSize size;

    BufferV1* buffer;

private:
    LinuxDmabufBufferV1* q_ptr;
};

class BufferV1 : public Wayland::Resource<LinuxDmabufBufferV1>
{
public:
    BufferV1(Client* client, uint32_t version, uint32_t id, LinuxDmabufBufferV1* q);
    ~BufferV1() override = default;

    static const struct wl_buffer_interface* interface();

private:
    static const struct wl_buffer_interface s_interface;

    LinuxDmabufBufferV1* q_ptr;
};

class ParamsWrapperV1;
class ParamsV1 : public Wayland::Resource<ParamsWrapperV1>
{
public:
    ParamsV1(Client* client,
             uint32_t version,
             uint32_t id,
             LinuxDmabufV1::Private* dmabuf,
             ParamsWrapperV1* q);
    ~ParamsV1();

    void add(int fd, uint32_t plane_idx, uint32_t offset, uint32_t stride, uint64_t modifier);
    void create(uint32_t bufferId, const QSize& size, uint32_t format, uint32_t flags);

private:
    static void addCallback(wl_client* wlClient,
                            wl_resource* wlResource,
                            int fd,
                            uint32_t plane_idx,
                            uint32_t offset,
                            uint32_t stride,
                            uint32_t modifier_hi,
                            uint32_t modifier_lo);

    static void createCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               int width,
                               int height,
                               uint32_t format,
                               uint32_t flags);

    static void createImmedCallback(wl_client* wlClient,
                                    wl_resource* wlResource,
                                    uint32_t new_id,
                                    int width,
                                    int height,
                                    uint32_t format,
                                    uint32_t flags);

    static struct zwp_linux_buffer_params_v1_interface const s_interface;

    wl_resource* m_resource;
    LinuxDmabufV1::Private* m_dmabuf;
    std::array<LinuxDmabufV1::Plane, 4> m_planes;
    size_t m_planeCount = 0;
    bool m_createRequested = false;
};

// TODO: Make this wrapper go away! For that Wayland::Resource can't depend any longer on the
// resourceDestroy signal being available.
class ParamsWrapperV1 : public QObject
{
    Q_OBJECT
public:
    ParamsWrapperV1(Client* client, uint32_t version, uint32_t id, LinuxDmabufV1::Private* dmabuf);
    ParamsV1* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

}
