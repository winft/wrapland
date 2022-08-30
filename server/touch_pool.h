/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <QPoint>

#include <cstdint>
#include <map>
#include <vector>

namespace Wrapland::Server
{
class Client;
class Seat;
class Surface;
class Touch;

struct touch_focus {
    Surface* surface{nullptr};
    std::vector<Touch*> devices;
    QPointF offset;
    QPointF first_touch_position;
    QMetaObject::Connection surface_lost_notifier;
};

/*
 * Handle touch devices associated to a seat.
 */
class WRAPLANDSERVER_EXPORT touch_pool
{
public:
    explicit touch_pool(Seat* seat);
    touch_pool(touch_pool const&) = delete;
    touch_pool& operator=(touch_pool const&) = delete;
    touch_pool(touch_pool&&) noexcept = default;
    touch_pool& operator=(touch_pool&&) noexcept = default;
    ~touch_pool();

    touch_focus const& get_focus() const;
    std::vector<Touch*> const& get_devices() const;

    void set_focused_surface(Surface* surface, QPointF const& surfacePosition = QPointF());
    void set_focused_surface_position(QPointF const& surfacePosition);
    int32_t touch_down(QPointF const& globalPosition);
    void touch_up(int32_t id);
    void touch_move(int32_t id, QPointF const& globalPosition);
    void touch_move_any(QPointF const& pos);
    void touch_frame() const;
    void cancel_sequence();
    bool has_implicit_grab(uint32_t serial) const;
    bool is_in_progress() const;

private:
    friend class Seat;
    void create_device(Client* client, uint32_t version, uint32_t id);

    touch_focus focus;

    // Key: Distinct id per touch point, Value: Wayland display serial.
    std::map<int32_t, uint32_t> ids;

    std::vector<Touch*> devices;
    Seat* seat;
};

}
