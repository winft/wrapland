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
class DataDevice;
class Pointer;
class Seat;
class Surface;
class Touch;

enum class drag_mode {
    none,
    pointer,
    touch,
};

struct drag_source {
    DataDevice* dev{nullptr};
    Pointer* pointer{nullptr};
    Touch* touch{nullptr};
    drag_mode mode{drag_mode::none};
    QMetaObject::Connection destroy_notifier;
    QMetaObject::Connection device_destroy_notifier;
};

/*
 * Handle drags on behalf of a seat.
 */
class WRAPLANDSERVER_EXPORT drag_pool
{
public:
    explicit drag_pool(Seat* seat);

    drag_source const& get_source() const;

    void set_target(Surface* new_surface,
                    const QPointF& globalPosition,
                    const QMatrix4x4& inputTransformation);
    void set_target(Surface* new_surface, const QMatrix4x4& inputTransformation = QMatrix4x4());

    bool is_in_progress() const;
    bool is_pointer_drag() const;
    bool is_touch_drag() const;

    DataDevice* target = nullptr;
    Surface* surface = nullptr;
    QMatrix4x4 transformation;

    void cancel();
    void end(uint32_t serial);
    void perform_drag(DataDevice* dataDevice);

private:
    drag_source source;

    QMetaObject::Connection target_destroy_connection;

    Seat* seat;
};

}
