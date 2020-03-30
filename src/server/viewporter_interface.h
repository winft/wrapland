/********************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include "global.h"
#include "resource.h"

namespace Wrapland
{
namespace Server
{

class Display;
class SurfaceInterface;
class ViewportInterface;

/**
 * @brief Represents the Global for wp_viewporter interface.
 *
 * This class creates ViewportInterfaces and attaches them to SurfaceInterfaces.
 *
 * @see ViewportInterface
 * @see SurfaceInterface
 * @since 0.518.0
 **/
class WRAPLANDSERVER_EXPORT ViewporterInterface : public Global
{
    Q_OBJECT
public:
    ~ViewporterInterface() override;

Q_SIGNALS:
    /**
     * Emitted whenever this ViewporterInterface created a ViewportInterface.
     **/
    void viewportCreated(ViewportInterface*);

private:
    explicit ViewporterInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

/**
 * @brief Resource for the wp_viewport interface.
 *
 **/
class WRAPLANDSERVER_EXPORT ViewportInterface : public Resource
{
    Q_OBJECT
public:
    ~ViewportInterface() override;

Q_SIGNALS:
    void destinationSizeSet(const QSize &size);
    void sourceRectangleSet(const QRectF &rect);

private:
    explicit ViewportInterface(ViewporterInterface *viewporter, wl_resource *parentResource,
                               SurfaceInterface *surface);
    friend class ViewporterInterface;

    class Private;
    Private *d_func() const;
};

}
}
