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

#include "resource.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct wl_resource;
struct wl_shm_buffer;

namespace Wrapland::Server
{
class Buffer;

namespace Wayland
{
class Display;

class BufferManager
{
public:
    BufferManager();

    Buffer* fromResource(wl_resource* resource) const;

    void addBuffer(Buffer* buffer);
    void removeBuffer(Buffer* buffer);

    bool beginShmAccess(wl_shm_buffer* buffer);
    void endShmAccess();

private:
    wl_shm_buffer* m_accessedShmBuffer;
    int m_accessCounter;

    std::vector<Buffer*> m_buffers;
};

}
}
