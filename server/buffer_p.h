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
#include "buffer.h"

#include <wayland-server.h>

namespace Wrapland::Server
{
namespace Wayland
{
class Display;
}

struct DestroyWrapper {
    Buffer* buffer;
    struct wl_listener listener;
};

class Buffer::Private
{
public:
    Private(Buffer* q, wl_resource* wlResource, Surface* surface, Wayland::Display* display);
    ~Private();

    QImage::Format format() const;
    QImage createImage();

    wl_resource* resource;
    wl_shm_buffer* shmBuffer;
    LinuxDmabufBufferV1* dmabufBuffer;

    Surface* surface;
    int refCount;
    QSize size;
    bool alpha;
    bool committed;

private:
    static void destroyListenerCallback(wl_listener* listener, void* data);
    static void imageBufferCleanupHandler(void* info);

    Wayland::Display* display;
    Buffer* q_ptr;
    DestroyWrapper destroyWrapper;
};

}
