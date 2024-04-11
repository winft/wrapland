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

#include <tuple>

#include <wayland-server.h>

namespace Wrapland::Server::Wayland
{

template<auto sender, uint32_t min_version = 0, typename... Args>
void send(wl_resource* resource, uint32_t version, Args&&... args)
{
    if constexpr (min_version <= 1) {
        sender(resource, std::forward<Args>(args)...);
    } else {
        if (version >= min_version) {
            sender(resource, std::forward<Args>(args)...);
        }
    }
}

template<auto sender, uint32_t min_version, typename Tuple, std::size_t... Indices>
void send_tuple_impl(wl_resource* resource,
                     uint32_t version,
                     Tuple const&& tuple,
                     [[maybe_unused]] std::index_sequence<Indices...> indices)
{
    send<sender, min_version>(resource, version, std::get<Indices>(tuple)...);
}

template<auto sender, uint32_t min_version = 0, typename... Args>
void send_tuple(wl_resource* resource, uint32_t version, std::tuple<Args...>&& tuple)
{
    auto constexpr size = std::tuple_size_v<std::decay_t<decltype(tuple)>>;
    auto constexpr indices = std::make_index_sequence<size>{};
    send_tuple_impl<sender, min_version>(resource, version, std::move(tuple), indices);
}

}
