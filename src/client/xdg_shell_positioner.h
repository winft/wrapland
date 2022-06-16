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
 * Builder class describing how a popup should be positioned
 * when created
 *
 * @since 0.0.539
 */
class WRAPLANDCLIENT_EXPORT XdgPositioner
{
public:
    /*
     * Flags describing how a popup should be reposition if constrained
     */
    enum class Constraint {
        /*
         * Slide the popup on the X axis until there is room
         */
        SlideX = 1 << 0,
        /*
         * Slide the popup on the Y axis until there is room
         */
        SlideY = 1 << 1,
        /*
         * Invert the anchor and gravity on the X axis
         */
        FlipX = 1 << 2,
        /*
         * Invert the anchor and gravity on the Y axis
         */
        FlipY = 1 << 3,
        /*
         * Resize the popup in the X axis
         */
        ResizeX = 1 << 4,
        /*
         * Resize the popup in the Y axis
         */
        ResizeY = 1 << 5,
    };

    Q_DECLARE_FLAGS(Constraints, Constraint)

    XdgPositioner(const QSize& initialSize = QSize(), const QRect& anchor = QRect());
    XdgPositioner(const XdgPositioner& other);
    ~XdgPositioner();

    /**
     * Which edge of the anchor should the popup be positioned around
     */
    // KF6 TODO use a better data type (enum of 8 options) rather than flags which allow invalid
    // values
    Qt::Edges anchorEdge() const;
    void setAnchorEdge(Qt::Edges edge);

    /**
     * Specifies in what direction the popup should be positioned around the anchor
     * i.e if the gravity is "bottom", then then the top of top of the popup will be at the anchor
     * edge if the gravity is top, then the bottom of the popup will be at the anchor edge
     *
     */
    // KF6 TODO use a better data type (enum of 8 options) rather than flags which allow invalid
    // values
    Qt::Edges gravity() const;
    void setGravity(Qt::Edges edge);

    /**
     * The area this popup should be positioned around
     */
    QRect anchorRect() const;
    void setAnchorRect(const QRect& anchor);

    /**
     * The size of the surface that is to be positioned.
     */
    QSize initialSize() const;
    void setInitialSize(const QSize& size);

    /**
     * Specifies how the compositor should position the popup if it does not fit in the requested
     * position
     */
    Constraints constraints() const;
    void setConstraints(Constraints constraints);

    /**
     * An additional offset that should be applied from the anchor.
     */
    QPoint anchorOffset() const;
    void setAnchorOffset(const QPoint& offset);

    bool isReactive() const;
    void setReactive(bool isReactive);

    uint32_t parentConfigure() const;
    void setParentConfigure(uint32_t parentConfigure);

    QSize parentSize() const;
    void setParentSize(const QSize& parent);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::XdgPositioner::Constraints)

Q_DECLARE_METATYPE(Wrapland::Client::XdgPositioner)
Q_DECLARE_METATYPE(Wrapland::Client::XdgPositioner::Constraint)
Q_DECLARE_METATYPE(Wrapland::Client::XdgPositioner::Constraints)
