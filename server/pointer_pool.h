/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "pointer.h"
#include "surface.h"

#include <Wrapland/Server/wraplandserver_export.h>

#include <QMatrix4x4>
#include <QObject>
#include <QPoint>
#include <QPointer>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Wrapland::Server
{
class Seat;

enum class button_state;

struct pointer_focus {
    Surface* surface = nullptr;
    std::vector<Pointer*> devices;
    QMetaObject::Connection destroyConnection;
    QPointF offset = QPointF();
    QMatrix4x4 transformation;
    uint32_t serial = 0;
};

/*
 * Handle pointer devices associated to a seat.
 *
 * This class provides support for pointer actions and gestures.
 * Clients are allowed to mantain multiple pointers for the same seat,
 * that can receive focus and should be updated together.
 */
class WRAPLANDSERVER_EXPORT pointer_pool
{
public:
    explicit pointer_pool(Seat* seat);
    ~pointer_pool();

    pointer_focus const& get_focus() const;

    void set_position(const QPointF& position);
    void set_focused_surface(Surface* surface, const QPointF& surfacePosition = QPoint());
    void set_focused_surface(Surface* surface, const QMatrix4x4& transformation);
    void set_focused_surface_position(const QPointF& surfacePosition);
    void set_focused_surface_transformation(const QMatrix4x4& transformation);

    void button_pressed(uint32_t button);
    void button_pressed(Qt::MouseButton button);
    void button_released(uint32_t button);
    void button_released(Qt::MouseButton button);
    void relative_motion(const QSizeF& delta,
                         const QSizeF& deltaNonAccelerated,
                         uint64_t microseconds) const;
    void send_axis(Qt::Orientation orientation,
                   qreal delta,
                   int32_t discreteDelta,
                   PointerAxisSource source) const;
    void send_axis(Qt::Orientation orientation, uint32_t delta) const;

    void start_swipe_gesture(uint32_t fingerCount);
    void update_swipe_gesture(const QSizeF& delta) const;
    void end_swipe_gesture();
    void cancel_swipe_gesture();
    void start_pinch_gesture(uint32_t fingerCount);
    void update_pinch_gesture(const QSizeF& delta, qreal scale, qreal rotation) const;
    void end_pinch_gesture();
    void cancel_pinch_gesture();

    bool is_button_pressed(uint32_t button) const;
    bool is_button_pressed(Qt::MouseButton button) const;
    bool has_implicit_grab(uint32_t serial) const;
    uint32_t button_serial(uint32_t button) const;
    uint32_t button_serial(Qt::MouseButton button) const;

    QPointF position() const;

    std::unordered_map<uint32_t, uint32_t> buttonSerials;
    std::unordered_map<uint32_t, button_state> buttonStates;
    QPointF pos;
    QPointer<Surface> gestureSurface;

    void create_device(Client* client, uint32_t version, uint32_t id);

    // private
    void update_button_serial(uint32_t button, uint32_t serial);
    void update_button_state(uint32_t button, button_state state);

    Seat* seat;
    std::vector<Pointer*> devices;

private:
    pointer_focus focus;
};

}
