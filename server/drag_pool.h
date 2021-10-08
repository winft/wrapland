/*
    SPDX-FileCopyrightText: 2020, 2021 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QMatrix4x4>
#include <functional>

namespace Wrapland::Server
{
class data_device;
class data_offer;
class data_source;
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
    data_source* src{nullptr};

    Pointer* pointer{nullptr};
    Touch* touch{nullptr};
    drag_mode mode{drag_mode::none};

    bool movement_blocked{true};

    struct {
        Surface* origin{nullptr};
        Surface* icon{nullptr};
    } surfaces;

    uint32_t serial;
    QMetaObject::Connection action_notifier;
    QMetaObject::Connection destroy_notifier;
};

struct drag_target_device {
    data_device* dev;
    data_offer* offer{nullptr};
    QMetaObject::Connection offer_action_notifier;
    QMetaObject::Connection destroy_notifier;
};

struct drag_target {
    Surface* surface{nullptr};
    std::vector<drag_target_device> devices;
    QMatrix4x4 transformation;
    QMetaObject::Connection motion_notifier;
    QMetaObject::Connection surface_destroy_notifier;
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

    void start(data_source* src, Surface* origin, Surface* icon, uint32_t serial);
    void cancel();
    void drop();

    dnd_action target_actions_update(dnd_actions receiver_actions, dnd_action preffered_action);

private:
    void perform_drag();
    void update_target(Surface* surface,
                       uint32_t serial,
                       QMatrix4x4 const& inputTransformation = QMatrix4x4());
    void update_offer(uint32_t serial);
    void match_actions(data_offer* offer);
    void cancel_target();

    void end();

    void setup_motion();
    void setup_pointer_motion();
    void setup_touch_motion();

    void for_each_target_device(std::function<void(data_device*)> apply) const;

    drag_source source;
    drag_target target;

    Seat* seat;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::dnd_actions)
