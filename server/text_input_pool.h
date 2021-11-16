/*
    SPDX-FileCopyrightText: 2021 Roman Glig <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>

namespace Wrapland::Server
{
struct input_method_v2_state;
class Seat;
class Surface;
class text_input_v2;
struct text_input_v2_state;
class text_input_v3;
struct text_input_v3_state;

class WRAPLANDSERVER_EXPORT text_input_pool
{
public:
    explicit text_input_pool(Seat* seat);

    void register_device(text_input_v2* ti);
    void register_device(text_input_v3* ti);

    void set_focused_surface(Surface* surface);
    bool set_v2_focused_surface(Surface* surface);
    bool set_v3_focused_surface(Surface* surface);

    void sync_to_text_input(input_method_v2_state const& prev,
                            input_method_v2_state const& next) const;

    void sync_to_input_method(text_input_v2_state const& prev,
                              text_input_v2_state const& next) const;
    void sync_to_input_method(text_input_v3_state const& prev,
                              text_input_v3_state const& next) const;

    struct {
        Surface* surface = nullptr;
        QMetaObject::Connection destroy_connection;
    } focus;

    // Both text inputs may be active at a time.
    // That doesn't make sense, but there's no reason to enforce only one.
    struct {
        quint32 serial = 0;
        text_input_v2* text_input{nullptr};
    } v2;
    struct {
        text_input_v3* text_input{nullptr};
    } v3;

    std::vector<text_input_v2*> v2_devices;
    std::vector<text_input_v3*> v3_devices;

    Seat* seat;
};

}
