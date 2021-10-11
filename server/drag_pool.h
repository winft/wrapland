/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QMatrix4x4>
#include <QObject>

namespace Wrapland::Server
{
class data_device;
class Pointer;
class Seat;
class Surface;
class Touch;

enum class dnd_action {
    none = 0,
    copy = 1 << 0,
    move = 1 << 1,
    ask = 1 << 2,
};
Q_DECLARE_FLAGS(dnd_actions, dnd_action)

enum class drag_mode {
    none,
    pointer,
    touch,
};

struct drag_source {
    data_device* dev{nullptr};
    Pointer* pointer{nullptr};
    Touch* touch{nullptr};
    drag_mode mode{drag_mode::none};

    bool movement_blocked{true};

    QMetaObject::Connection destroy_notifier;
    QMetaObject::Connection device_destroy_notifier;
};

struct drag_target {
    data_device* dev{nullptr};
    Surface* surface{nullptr};
    QMatrix4x4 transformation;
    QMetaObject::Connection destroy_notifier;
};

/*
 * Handle drags on behalf of a seat.
 */
class WRAPLANDSERVER_EXPORT drag_pool
{
public:
    explicit drag_pool(Seat* seat);

    drag_source const& get_source() const;
    drag_target const& get_target() const;

    void set_target(Surface* new_surface,
                    const QPointF& globalPosition,
                    const QMatrix4x4& inputTransformation);
    void set_target(Surface* new_surface, const QMatrix4x4& inputTransformation = QMatrix4x4());
    void set_source_client_movement_blocked(bool block);

    bool is_in_progress() const;
    bool is_pointer_drag() const;
    bool is_touch_drag() const;

    void cancel();
    void end(uint32_t serial);
    void perform_drag(data_device* dataDevice);

private:
    drag_source source;
    drag_target target;

    Seat* seat;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::dnd_actions)
