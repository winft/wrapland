/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <algorithm>
#include <vector>

template<typename T, typename Surface>
static T* interfaceForSurface(Surface* surface, std::vector<T*> const& interfaces)
{
    if (!surface) {
        return nullptr;
    }

    auto it = std::find_if(interfaces.begin(), interfaces.end(), [surface](auto iface) {
        return surface->client() == iface->client();
    });
    return it == interfaces.end() ? nullptr : *it;
}

template<typename Surface, typename T>
static std::vector<T*> interfacesForSurface(Surface const* surface,
                                            std::vector<T*> const& interfaces)
{
    std::vector<T*> ret;
    if (!surface) {
        return ret;
    }

    std::copy_if(interfaces.cbegin(),
                 interfaces.cend(),
                 std::back_inserter(ret),
                 [surface](T* device) { return device->client() == surface->client(); });
    return ret;
}

template<typename Surface, typename Vector, typename UnaryFunction>
static void forEachInterface(Surface* surface, Vector const& interfaces, UnaryFunction method)
{
    if (!surface) {
        return;
    }
    for (auto const& iface : interfaces) {
        if (iface->client() == surface->client()) {
            method(iface);
        }
    }
}

template<typename Device, typename Seat>
bool has_keyboard_focus(Device* device, Seat* seat)
{
    auto focused_surface = seat->hasKeyboard() ? seat->keyboards().get_focus().surface : nullptr;
    return device && focused_surface && (focused_surface->client() == device->client());
}

template<typename V, typename T>
bool contains(V const& container, T const& arg)
{
    return std::find(container.cbegin(), container.cend(), arg) != container.cend();
}
template<typename V, typename T>
void remove_all(V& container, T const& arg)
{
    container.erase(std::remove(container.begin(), container.end(), arg), container.end());
}
template<typename V, typename F>
void remove_all_if(V& container, F&& condition)
{
    container.erase(std::remove_if(container.begin(), container.end(), condition), container.end());
}
template<typename V, typename T>
bool remove_one(V& container, T const& arg)
{
    auto it = std::find(container.begin(), container.end(), arg);
    if (it == container.end()) {
        return false;
    }
    container.erase(it);
    return true;
}
