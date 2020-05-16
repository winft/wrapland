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

#include <QDebug>

#include <functional>
#include <memory>

namespace Wrapland::Server::Wayland
{

template<typename WaylandObject>
class Capsule
{
public:
    explicit Capsule(std::function<void(WaylandObject*)> dtor)
        : m_object{nullptr}
        , m_dtor(dtor)
    {
    }

    Capsule(Capsule&) = delete;
    Capsule& operator=(Capsule) = delete;
    Capsule(Capsule&&) noexcept = default;
    Capsule& operator=(Capsule&&) noexcept = default;

    ~Capsule()
    {
        if (m_object) {
            m_dtor(m_object);
        }
    }

    void create(WaylandObject* object)
    {
        m_object = object;
    }

    void release()
    {
        m_object = nullptr;
    }

    bool valid() const
    {
        return m_object != nullptr;
    }

    auto get() const
    {
        return m_object;
    }

private:
    WaylandObject* m_object;
    std::function<void(WaylandObject*)> m_dtor;
};

}
