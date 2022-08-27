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

#include "keyboard.h"
#include "logging.h"

#include "wayland/resource.h"

#include <QPointer>
#include <filesystem>

namespace Wrapland::Server
{

struct file_wrap {
    file_wrap() = default;
    explicit file_wrap(FILE* file)
        : file{file}
    {
    }
    file_wrap(file_wrap const&) = delete;
    file_wrap& operator=(file_wrap const&) = delete;
    file_wrap(file_wrap&&) noexcept = delete;
    file_wrap& operator=(file_wrap&& other) noexcept
    {
        this->file = other.file;
        other.file = nullptr;
        return *this;
    }
    ~file_wrap()
    {
        if (file && !std::fclose(file)) {
            qCWarning(WRAPLAND_SERVER, "Failed to close keymap file %p.", static_cast<void*>(file));
        }
    }
    FILE* file{nullptr};
};

class Keyboard::Private : public Wayland::Resource<Keyboard>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Seat* _seat, Keyboard* q_ptr);

    void sendKeymap(int fd, quint32 size);
    void sendModifiers();
    void sendModifiers(quint32 serial,
                       quint32 depressed,
                       quint32 latched,
                       quint32 locked,
                       quint32 group);

    void sendLeave(quint32 serial, Surface* surface);
    void sendEnter(quint32 serial, Surface* surface);

    Surface* focusedSurface = nullptr;
    QMetaObject::Connection destroyConnection;

    file_wrap keymap;
    bool needs_keymap_update{true};

    Seat* seat;
    Keyboard* q_ptr;

private:
    static const struct wl_keyboard_interface s_interface;
};

}
