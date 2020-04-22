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

#include <Wrapland/Server/wraplandserver_export.h>

struct wl_resource;
struct wl_shm_buffer;

namespace Wrapland
{
namespace Server
{
class D_isplay;
class Surface;
class LinuxDmabufBufferV1;

class WRAPLANDSERVER_EXPORT Buffer : public QObject
{
    Q_OBJECT
public:
    ~Buffer() override;

    void ref();
    void unref();
    bool isReferenced() const;

    Surface* surface() const;
    wl_shm_buffer* shmBuffer();
    LinuxDmabufBufferV1* linuxDmabufBuffer();
    wl_resource* resource() const;

    QImage data();
    QSize size() const;
    void setSize(const QSize& size);

    bool hasAlphaChannel() const;

    static Buffer* get(D_isplay* display, wl_resource* resource);

Q_SIGNALS:
    void sizeChanged();
    void resourceDestroyed();

private:
    friend class Surface;
    Buffer(wl_resource* wlResource, Surface* parent);
    Buffer(wl_resource* wlResource, D_isplay* display);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::Buffer*)