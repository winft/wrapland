/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>
#include <QRect>
#include <QSize>

#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

namespace Wrapland::Client
{

/**
 * Flags describing how a popup should be reposition if constrained
 */
enum class xdg_shell_constraint_adjustment {
    slide_x = 1 << 0,
    slide_y = 1 << 1,
    flip_x = 1 << 2,
    flip_y = 1 << 3,
    resize_x = 1 << 4,
    resize_y = 1 << 5,
};
Q_DECLARE_FLAGS(xdg_shell_constraint_adjustments, xdg_shell_constraint_adjustment)

struct xdg_shell_positioner_data {
    struct {
        QRect rect;
        Qt::Edges edge;
        QPoint offset;
    } anchor;

    QSize size;
    Qt::Edges gravity;
    xdg_shell_constraint_adjustments constraint_adjustments;
};

/**
 * Builder class describing how a popup should be positioned
 * when created
 *
 * @since 0.0.539
 */
class WRAPLANDCLIENT_EXPORT xdg_shell_positioner
{
public:
    xdg_shell_positioner(xdg_shell_positioner_data data = {});
    xdg_shell_positioner(xdg_shell_positioner const& other);
    ~xdg_shell_positioner();

    xdg_shell_positioner_data const& get_data() const;

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::xdg_shell_constraint_adjustments)

Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_positioner_data)
Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_positioner)
Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_constraint_adjustment)
Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_constraint_adjustments)
