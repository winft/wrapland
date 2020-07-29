/********************************************************************
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
#pragma once

#include <QImage>
#include <QObject>

#include <memory>
#include <optional>

#include <Wrapland/Server/wraplandserver_export.h>

struct wl_resource;
struct wl_shm_buffer;

namespace Wrapland::Server
{
class Buffer;
class Display;
class Surface;
class LinuxDmabufBufferV1;

class WRAPLANDSERVER_EXPORT ShmImage
{
public:
    enum class Format {
        invalid,
        argb8888,
        xrgb8888,
    };
    ShmImage(ShmImage const& img);
    ShmImage& operator=(ShmImage const& img);

    ShmImage(ShmImage&& img) noexcept;
    ShmImage& operator=(ShmImage&& img) noexcept;

    ~ShmImage();

    Format format() const;
    int32_t stride() const;
    int32_t bpp() const;

    uchar* data() const;

    QImage createQImage();

    static std::optional<ShmImage> get(Buffer* buffer);

private:
    ShmImage(Buffer* buffer, ShmImage::Format format);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT Buffer : public QObject
{
    Q_OBJECT
public:
    ~Buffer() override;

    Surface* surface() const;
    wl_shm_buffer* shmBuffer();
    LinuxDmabufBufferV1* linuxDmabufBuffer();
    wl_resource* resource() const;

    std::optional<ShmImage> shmImage();

    QSize size() const;
    void setSize(const QSize& size);
    void setCommitted();

    bool hasAlphaChannel() const;

    static std::shared_ptr<Buffer> get(Display* display, wl_resource* resource);

Q_SIGNALS:
    void sizeChanged();
    void resourceDestroyed();

private:
    friend class ShmImage;
    friend class Surface;

    static std::shared_ptr<Buffer> make(wl_resource* wlResource, Surface* surface);
    static std::shared_ptr<Buffer> make(wl_resource* wlResource, Display* display);

    Buffer(wl_resource* wlResource, Surface* surface);
    Buffer(wl_resource* wlResource, Display* display);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::Buffer*)
