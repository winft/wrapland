/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "virtual_keyboard_v1.h"

#include "wayland_pointer_p.h"

#include <QObject>
#include <filesystem>

#include <wayland-virtual-keyboard-v1-client-protocol.h>

namespace Wrapland::Client
{

// Copied from Server library.
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
        if (file) {
            std::fclose(file);
        }
    }
    FILE* file{nullptr};
};

class Q_DECL_HIDDEN virtual_keyboard_manager_v1::Private
{
public:
    ~Private() = default;

    void release();
    bool isValid();
    virtual_keyboard_v1* create_virtual_keyboard(Seat* seat, QObject* parent = nullptr);

    operator zwp_virtual_keyboard_manager_v1*()
    {
        return manager_ptr;
    }
    operator zwp_virtual_keyboard_manager_v1*() const
    {
        return manager_ptr;
    }

    WaylandPointer<zwp_virtual_keyboard_manager_v1, zwp_virtual_keyboard_manager_v1_destroy>
        manager_ptr;

    EventQueue* queue{nullptr};
};

class Q_DECL_HIDDEN virtual_keyboard_v1::Private
{
public:
    Private(Seat* seat, virtual_keyboard_v1* q_ptr);
    virtual ~Private() = default;

    void setup(zwp_virtual_keyboard_v1* ti);

    bool isValid() const;

    WaylandPointer<zwp_virtual_keyboard_v1, zwp_virtual_keyboard_v1_destroy> virtual_keyboard_ptr;

    EventQueue* queue{nullptr};
    Seat* seat;

    file_wrap keymap;

private:
    virtual_keyboard_v1* q_ptr;
};

}
