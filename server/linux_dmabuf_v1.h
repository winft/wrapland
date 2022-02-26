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

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <QSize>

#include <functional>
#include <memory>
#include <unistd.h>
#include <unordered_set>
#include <vector>

namespace Wrapland::Server
{

class Buffer;
class Display;

struct drm_format {
    uint32_t format;
    std::unordered_set<uint64_t> modifiers;
};

enum class linux_dmabuf_flag_v1 {
    y_inverted = 0x1,
    interlaced = 0x2,
    bottom_field_first = 0x4,
};

Q_DECLARE_FLAGS(linux_dmabuf_flags_v1, linux_dmabuf_flag_v1)

struct linux_dmabuf_plane_v1 {
    int fd;            /// The dmabuf file descriptor
    uint32_t offset;   /// The offset from the start of buffer
    uint32_t stride;   /// The distance from the start of a row to the next row in bytes
    uint64_t modifier; /// The layout modifier
};

class linux_dmabuf_buffer_v1
{
public:
    linux_dmabuf_buffer_v1(std::vector<linux_dmabuf_plane_v1> planes,
                           uint32_t format,
                           QSize const& size,
                           linux_dmabuf_flags_v1 flags)
        : planes{std::move(planes)}
        , format{format}
        , size{size}
        , flags{flags}
    {
    }
    virtual ~linux_dmabuf_buffer_v1()
    {
        for (auto& plane : planes) {
            if (plane.fd != -1) {
                ::close(plane.fd);
            }
        }
    }

    std::vector<linux_dmabuf_plane_v1> planes;
    uint32_t format;
    QSize size;
    linux_dmabuf_flags_v1 flags;
};

using linux_dmabuf_import_v1 = std::function<std::unique_ptr<linux_dmabuf_buffer_v1>(
    std::vector<linux_dmabuf_plane_v1> const& planes,
    uint32_t format,
    QSize const& size,
    linux_dmabuf_flags_v1 flags)>;

class WRAPLANDSERVER_EXPORT linux_dmabuf_v1 : public QObject
{
    Q_OBJECT
public:
    linux_dmabuf_v1(Display* display, linux_dmabuf_import_v1 import);
    ~linux_dmabuf_v1() override;

    void set_formats(std::vector<drm_format> const& formats);

private:
    friend class Buffer;
    friend class linux_dmabuf_params_v1;
    friend class linux_dmabuf_params_v1_impl;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::linux_dmabuf_v1*)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::linux_dmabuf_flags_v1)
