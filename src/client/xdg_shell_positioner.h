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

struct xdg_positioner;

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

class EventQueue;

class WRAPLANDCLIENT_EXPORT xdg_shell_positioner : public QObject
{
public:
    ~xdg_shell_positioner() override;

    void setup(::xdg_positioner* positioner);
    void release();
    bool isValid() const;

    EventQueue* eventQueue();
    void setEventQueue(EventQueue* queue);

    xdg_shell_positioner_data const& get_data() const;
    void set_data(xdg_shell_positioner_data data);

    operator ::xdg_positioner*();
    operator ::xdg_positioner*() const;

private:
    explicit xdg_shell_positioner(QObject* parent = nullptr);
    friend class XdgShell;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::xdg_shell_constraint_adjustments)

Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_positioner_data)
Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_constraint_adjustment)
Q_DECLARE_METATYPE(Wrapland::Client::xdg_shell_constraint_adjustments)
