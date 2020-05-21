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
#include <QObject>

#include "../buffer.h"
#include "buffer_manager.h"

#include <algorithm>
#include <cassert>

namespace Wrapland::Server::Wayland
{

Buffer* BufferManager::fromResource(wl_resource* resource) const
{
    auto it = std::find_if(m_buffers.cbegin(), m_buffers.cend(), [resource](Buffer* buffer) {
        return buffer->resource() == resource;
    });

    return it != m_buffers.cend() ? *it : nullptr;
}

void BufferManager::addBuffer(Buffer* buffer)
{
    m_buffers.push_back(buffer);
}

void BufferManager::removeBuffer(Buffer* buffer)
{
    m_buffers.erase(std::remove(m_buffers.begin(), m_buffers.end(), buffer));
}

bool BufferManager::beginShmAccess(wl_shm_buffer* buffer)
{
    assert(buffer);

    if (m_accessedShmBuffer != nullptr && m_accessedShmBuffer != buffer) {
        return false;
    }

    wl_shm_buffer_begin_access(buffer);
    m_accessedShmBuffer = buffer;
    m_accessCounter++;

    return true;
}

void BufferManager::endShmAccess()
{
    assert(m_accessCounter > 0);

    m_accessCounter--;
    wl_shm_buffer_end_access(m_accessedShmBuffer);

    if (m_accessCounter == 0) {
        m_accessedShmBuffer = nullptr;
    }
}

}
