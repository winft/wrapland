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
#include <drm_fourcc.h>

namespace Wrapland::Server
{

class linux_dmabuf_params_v1;

constexpr uint32_t linux_dmabuf_v1_version = 3;
using linux_dmabuf_v1_global = Wayland::Global<linux_dmabuf_v1, linux_dmabuf_v1_version>;
using linux_dmabuf_v1_bind = Wayland::Bind<linux_dmabuf_v1_global>;

class linux_dmabuf_v1::Private : public linux_dmabuf_v1_global
{
public:
    Private(linux_dmabuf_v1* q, Display* display, linux_dmabuf_import_v1 import);
    ~Private() override;

    void bindInit(linux_dmabuf_v1_bind* bind) final;
    static void create_params_callback(linux_dmabuf_v1_bind* bind, uint32_t id);

    std::vector<linux_dmabuf_params_v1*> pending_params;
    linux_dmabuf_import_v1 import;
    std::vector<drm_format> supported_formats;

private:
    static const struct zwp_linux_dmabuf_v1_interface s_interface;
};

class linux_dmabuf_buffer_v1_res_impl;

class linux_dmabuf_buffer_v1_res : public QObject
{
    Q_OBJECT
public:
    linux_dmabuf_buffer_v1_res(Client* client,
                               uint32_t version,
                               uint32_t id,
                               std::unique_ptr<linux_dmabuf_buffer_v1> handle);

    std::unique_ptr<linux_dmabuf_buffer_v1> handle;
    linux_dmabuf_buffer_v1_res_impl* impl;

Q_SIGNALS:
    void resourceDestroyed();
};

class linux_dmabuf_buffer_v1_res_impl : public Wayland::Resource<linux_dmabuf_buffer_v1_res>
{
public:
    linux_dmabuf_buffer_v1_res_impl(Client* client,
                                    uint32_t version,
                                    uint32_t id,
                                    linux_dmabuf_buffer_v1_res* q);

    static struct wl_buffer_interface const s_interface;
};

class linux_dmabuf_params_v1_impl : public Wayland::Resource<linux_dmabuf_params_v1>
{
public:
    linux_dmabuf_params_v1_impl(Client* client,
                                uint32_t version,
                                uint32_t id,
                                linux_dmabuf_v1::Private* dmabuf,
                                linux_dmabuf_params_v1* q);
    ~linux_dmabuf_params_v1_impl() override;

    void add(int fd, uint32_t plane_idx, uint32_t offset, uint32_t stride, uint64_t modifier);
    void create(uint32_t buffer_id, QSize const& size, uint32_t format, uint32_t flags);

    linux_dmabuf_v1::Private* m_dmabuf;

private:
    static void add_callback(wl_client* wlClient,
                             wl_resource* wlResource,
                             int fd,
                             uint32_t plane_idx,
                             uint32_t offset,
                             uint32_t stride,
                             uint32_t modifier_hi,
                             uint32_t modifier_lo);

    static void create_callback(wl_client* wlClient,
                                wl_resource* wlResource,
                                int width,
                                int height,
                                uint32_t format,
                                uint32_t flags);

    static void create_immed_callback(wl_client* wlClient,
                                      wl_resource* wlResource,
                                      uint32_t new_id,
                                      int width,
                                      int height,
                                      uint32_t format,
                                      uint32_t flags);

    bool validate_params(QSize const& size);

    static struct zwp_linux_buffer_params_v1_interface const s_interface;

    std::array<linux_dmabuf_plane_v1, 4> m_planes;
    size_t m_planeCount = 0;
    bool m_createRequested = false;
    uint64_t modifier{DRM_FORMAT_MOD_INVALID};
    bool modifier_sent{false};
};

class linux_dmabuf_params_v1 : public QObject
{
    Q_OBJECT
public:
    linux_dmabuf_params_v1(Client* client,
                           uint32_t version,
                           uint32_t id,
                           linux_dmabuf_v1::Private* dmabuf);
    ~linux_dmabuf_params_v1() override;

    linux_dmabuf_params_v1_impl* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

}
