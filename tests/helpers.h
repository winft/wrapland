/********************************************************************
Copyright 2020 Roman Gilg <subdiff@gmail.com>

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

#include "../server/subcompositor.h"
#include "../server/surface.h"

namespace Wrapland::Server::Test
{

Surface* surface_at(Surface* surface, QPointF const& position)
{
    if (!surface->isMapped()) {
        return nullptr;
    }

    auto const children = surface->childSubsurfaces();

    // Go from top to bottom. Top most child is last in the vector.
    auto it = children.end();
    while (it != children.begin()) {
        auto const& current = *(--it);
        auto child_surface = current->surface();
        if (!child_surface) {
            continue;
        }
        if (auto s = surface_at(child_surface, position - current->position())) {
            return s;
        }
    }
    // check whether the geometry contains the pos
    if (!surface->size().isEmpty() && QRectF(QPoint(0, 0), surface->size()).contains(position)) {
        return surface;
    }
    return nullptr;
}

Surface* input_surface_at(Surface* surface, QPointF const& position)
{
    // TODO(unknown author): Most of this is very similar to Surface::surfaceAt
    //                       Is there a way to reduce the code duplication?
    if (!surface->isMapped()) {
        return nullptr;
    }

    auto const children = surface->childSubsurfaces();

    // Go from top to bottom. Top most child is last in list.
    auto it = children.end();
    while (it != children.begin()) {
        auto const& child = *(--it);
        auto child_surface = child->surface();
        if (!child_surface) {
            continue;
        }
        if (auto s = input_surface_at(child_surface, position - child->position())) {
            return s;
        }
    }

    // Check whether the geometry and input region contain the pos.
    if (!surface->size().isEmpty() && QRectF(QPoint(0, 0), surface->size()).contains(position)
        && (surface->inputIsInfinite() || surface->input().contains(position.toPoint()))) {
        return surface;
    }
    return nullptr;
}

}
